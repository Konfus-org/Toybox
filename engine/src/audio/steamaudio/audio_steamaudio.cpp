#include "tbx/audio/audio.h"
#include "tbx/core/log.h"
#include <SDL3/SDL.h>
#include <phonon.h>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace tbx::audio
{
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int FRAME_SIZE = 1024;

    /// @brief
    /// Purpose: One playing source as the audio thread sees it: a clip reference, a cursor,
    /// and the latest spatial parameters update() computed on the main thread.
    struct Voice
    {
        std::shared_ptr<const AudioClip> clip = {};
        double cursor = 0.0;
        float volume = 1.0f;
        float attenuation = 1.0f;
        IPLVector3 direction = {0.0f, 0.0f, -1.0f};
        bool is_looping = false;
        bool is_active = true;
        bool is_finished = false;
        IPLBinauralEffect effect = nullptr;
    };

    /// @brief
    /// Purpose: The whole audio engine, torn down by reset() and rebuilt lazily by update().
    struct AudioState
    {
        IPLContext context = nullptr;
        IPLHRTF hrtf = nullptr;
        SDL_AudioStream* stream = nullptr;
        IPLAudioBuffer mono = {};
        IPLAudioBuffer stereo = {};
        std::mutex voices_mutex;
        std::unordered_map<uint32, Voice> voices; // keyed by ToyId value
        std::unordered_map<Uuid, std::shared_ptr<const AudioClip>> clips;
        float listener_volume = 1.0f;
        std::vector<float> interleaved;

        ~AudioState()
        {
            if (stream)
                SDL_DestroyAudioStream(stream);
            for (auto& [key, voice] : voices)
                if (voice.effect)
                    iplBinauralEffectRelease(&voice.effect);
            if (mono.data)
                iplAudioBufferFree(context, &mono);
            if (stereo.data)
                iplAudioBufferFree(context, &stereo);
            if (hrtf)
                iplHRTFRelease(&hrtf);
            if (context)
                iplContextRelease(&context);
        }
    };

    static std::unique_ptr<AudioState> g_audio = {};

    //// DSP (audio thread) ////

    static void mix_block(AudioState& state, const int frame_count)
    {
        state.interleaved.assign(static_cast<size>(frame_count) * 2, 0.0f);
        const std::scoped_lock lock(state.voices_mutex);
        for (auto& [key, voice] : state.voices)
        {
            if (!voice.is_active || !voice.clip || voice.clip->samples.empty())
                continue;
            const AudioClip& clip = *voice.clip;
            const double step =
                static_cast<double>(clip.sample_rate) / SAMPLE_RATE; // nearest-sample resample
            const size frames_in_clip = clip.samples.size() / clip.channels;

            // Fill the mono input from the clip (channels averaged), advancing the cursor.
            for (int frame = 0; frame < frame_count; ++frame)
            {
                auto index = static_cast<size>(voice.cursor);
                if (index >= frames_in_clip)
                {
                    if (voice.is_looping)
                    {
                        voice.cursor = 0.0;
                        index = 0;
                    }
                    else
                    {
                        voice.is_active = false;
                        voice.is_finished = true;
                        for (int rest = frame; rest < frame_count; ++rest)
                            state.mono.data[0][rest] = 0.0f;
                        break;
                    }
                }
                float sample = 0.0f;
                for (int channel = 0; channel < clip.channels; ++channel)
                    sample += clip.samples[index * clip.channels + channel];
                state.mono.data[0][frame] = sample / clip.channels;
                voice.cursor += step;
            }

            auto parameters = IPLBinauralEffectParams {};
            parameters.direction = voice.direction;
            parameters.interpolation = IPL_HRTFINTERPOLATION_NEAREST;
            parameters.spatialBlend = 1.0f;
            parameters.hrtf = g_audio->hrtf;
            iplBinauralEffectApply(voice.effect, &parameters, &state.mono, &state.stereo);

            const float gain = voice.volume * voice.attenuation * state.listener_volume;
            for (int frame = 0; frame < frame_count; ++frame)
            {
                state.interleaved[frame * 2 + 0] += state.stereo.data[0][frame] * gain;
                state.interleaved[frame * 2 + 1] += state.stereo.data[1][frame] * gain;
            }
        }
    }

    static void SDLCALL feed_device(
        void* userdata,
        SDL_AudioStream* stream,
        const int additional_amount,
        int)
    {
        auto& state = *static_cast<AudioState*>(userdata);
        int remaining_bytes = additional_amount;
        while (remaining_bytes > 0)
        {
            const int block_bytes =
                std::min(remaining_bytes, FRAME_SIZE * 2 * static_cast<int>(sizeof(float)));
            const int frame_count = block_bytes / (2 * static_cast<int>(sizeof(float)));
            mix_block(state, frame_count);
            SDL_PutAudioStreamData(
                stream, state.interleaved.data(), frame_count * 2 * sizeof(float));
            remaining_bytes -= block_bytes;
        }
    }

    //// SETUP ////

    static AudioState* ensure_audio_ready()
    {
        if (g_audio)
            return g_audio.get();
        auto state = std::make_unique<AudioState>();

        auto context_settings = IPLContextSettings {};
        context_settings.version = STEAMAUDIO_VERSION;
        if (iplContextCreate(&context_settings, &state->context) != IPL_STATUS_SUCCESS)
        {
            log_error("Steam Audio context creation failed; audio disabled");
            return nullptr;
        }
        auto audio_settings = IPLAudioSettings {};
        audio_settings.samplingRate = SAMPLE_RATE;
        audio_settings.frameSize = FRAME_SIZE;
        auto hrtf_settings = IPLHRTFSettings {};
        hrtf_settings.type = IPL_HRTFTYPE_DEFAULT;
        hrtf_settings.volume = 1.0f;
        if (iplHRTFCreate(state->context, &audio_settings, &hrtf_settings, &state->hrtf)
            != IPL_STATUS_SUCCESS)
        {
            log_error("Steam Audio HRTF creation failed; audio disabled");
            return nullptr;
        }
        iplAudioBufferAllocate(state->context, 1, FRAME_SIZE, &state->mono);
        iplAudioBufferAllocate(state->context, 2, FRAME_SIZE, &state->stereo);

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            log_warn("SDL audio unavailable ({}); spatializer runs silent", SDL_GetError());
        else
        {
            auto spec = SDL_AudioSpec {};
            spec.format = SDL_AUDIO_F32;
            spec.channels = 2;
            spec.freq = SAMPLE_RATE;
            state->stream = SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, feed_device, state.get());
            if (state->stream)
                SDL_ResumeAudioStreamDevice(state->stream);
            else
                log_warn("no audio playback device ({}); spatializer runs silent", SDL_GetError());
        }

        g_audio = std::move(state);
        return g_audio.get();
    }

    //// BOUNDARY ////

    void reset()
    {
        g_audio.reset();
    }

    void update(Sandbox& sandbox, Assets& assets, const float)
    {
        AudioState* state = ensure_audio_ready();
        if (!state)
            return;
        auto& registry = sandbox.get_registry();

        // The first enabled listener frames the world; no listener, everything is silent.
        auto listener_inverse = Mat4(1.0f);
        bool has_listener = false;
        for (const auto [entity, listener] : registry.view<AudioListener>().each())
        {
            if (!registry.get<ToyHandle>(entity).is_enabled)
                continue;
            listener_inverse = math::inverse(sandbox.get_world_matrix(Toy(sandbox, entity)));
            state->listener_volume = listener.volume;
            has_listener = true;
            break;
        }

        const std::scoped_lock lock(state->voices_mutex);

        // Mirror playing sources into voices; spatial extent comes from the toy's Collider.
        for (const auto [entity, source] : registry.view<AudioSource>().each())
        {
            const auto key = static_cast<uint32>(entity);
            const bool wants_voice = has_listener
                && registry.get<ToyHandle>(entity).is_enabled && source.is_playing
                && source.clip.is_valid();
            auto existing = state->voices.find(key);

            if (!wants_voice)
            {
                if (existing != state->voices.end())
                {
                    if (existing->second.effect)
                        iplBinauralEffectRelease(&existing->second.effect);
                    state->voices.erase(existing);
                }
                continue;
            }

            if (existing == state->voices.end())
            {
                // Clips copy once into a shared cache so hot reloads never race the mixer.
                auto& cached = state->clips[source.clip.id];
                if (!cached)
                {
                    const auto clip = assets.get(source.clip);
                    if (!clip)
                        continue; // still loading
                    cached = std::make_shared<AudioClip>(clip->get());
                }
                auto voice = Voice {};
                voice.clip = cached;
                auto audio_settings = IPLAudioSettings {};
                audio_settings.samplingRate = SAMPLE_RATE;
                audio_settings.frameSize = FRAME_SIZE;
                auto effect_settings = IPLBinauralEffectSettings {};
                effect_settings.hrtf = state->hrtf;
                iplBinauralEffectCreate(
                    state->context, &audio_settings, &effect_settings, &voice.effect);
                existing = state->voices.emplace(key, std::move(voice)).first;
            }

            Voice& voice = existing->second;
            if (voice.is_finished)
            {
                registry.get<AudioSource>(entity).is_playing = false;
                continue;
            }

            const Mat4 world = sandbox.get_world_matrix(Toy(sandbox, entity));
            const Vec3 local =
                Vec3(listener_inverse * world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const float distance = math::length(local);
            const Vec3 direction =
                distance > 0.0001f ? local * (1.0f / distance) : Vec3(0.0f, 0.0f, -1.0f);

            // A Collider softens attenuation by its extent (the shared Shape vocabulary).
            float extent = 0.0f;
            if (const auto* collider = registry.try_get<Collider>(entity))
            {
                switch (collider->shape)
                {
                    case Shape::SPHERE:
                    case Shape::CAPSULE:
                        extent = collider->radius;
                        break;
                    case Shape::BOX:
                        extent = std::max(
                            {collider->half_extents.x,
                             collider->half_extents.y,
                             collider->half_extents.z});
                        break;
                }
            }
            voice.direction = IPLVector3 {direction.x, direction.y, direction.z};
            voice.attenuation = 1.0f / (1.0f + std::max(0.0f, distance - extent));
            voice.volume = source.volume;
            voice.is_looping = source.is_looping;
        }
    }
}
