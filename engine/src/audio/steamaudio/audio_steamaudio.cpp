#include "tbx/audio/audio.h"
#include "tbx/ecs/sandbox.h"
#include "tbx/audio/source.h"
#include "tbx/audio/listener.h"
#include "tbx/runtime.h"
#include "tbx/math/transform.h"
#include "tbx/physics/collider.h"
#include "tbx/debug/log.h"
#include <SDL3/SDL.h>
#include <atomic>
#include <iterator>
#include <memory>
#include <mutex>
#include <phonon.h>
#include <type_traits>
#include <unordered_map>

namespace tbx
{
    static constexpr int SAMPLE_RATE = 48000;
    static constexpr int FRAME_SIZE = 1024;

    /// @brief
    /// Purpose: Deleter for the Steam Audio binaural effect C handle — release lives here
    /// and nowhere else; every erase/clear of a Voice releases through it.
    struct BinauralEffectReleaser
    {
        void operator()(IPLBinauralEffect effect) const
        {
            iplBinauralEffectRelease(&effect);
        }
    };
    using BinauralEffectHolder =
        std::unique_ptr<std::remove_pointer_t<IPLBinauralEffect>, BinauralEffectReleaser>;

    /// @brief
    /// Purpose: One playing source as the audio thread sees it: a clip reference, a cursor,
    /// and the latest spatial parameters update_audio() computed on the main thread.
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
        BinauralEffectHolder effect = {};
    };

    /// @brief
    /// Purpose: The whole audio engine behind the boundary, built lazily on the first update
    /// (the destructor stops the SDL stream first, so the mixer callback is silent before
    /// any buffer frees).
    struct AudioState::Backend
    {
        ~Backend()
        {
            if (stream)
                SDL_DestroyAudioStream(stream);
            // The mixer is silent now; voices must release their effects BEFORE the context
            // goes (member order alone would destroy the map after this body released it).
            voices.clear();
            if (mono.data)
                iplAudioBufferFree(context, &mono);
            if (stereo.data)
                iplAudioBufferFree(context, &stereo);
            if (hrtf)
                iplHRTFRelease(&hrtf);
            if (context)
                iplContextRelease(&context);
        }

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
    };

    AudioState::AudioState() = default;
    AudioState::~AudioState() = default;

    //// DSP (audio thread) ////

    static void mix_block(AudioState& audio, const int frame_count)
    {
        AudioState::Backend& state = *audio.backend;
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
            parameters.hrtf = state.hrtf;
            iplBinauralEffectApply(voice.effect.get(), &parameters, &state.mono, &state.stereo);

            const float gain = voice.volume * voice.attenuation * state.listener_volume
                * audio.master_volume.load(std::memory_order_relaxed);
            for (int frame = 0; frame < frame_count; ++frame)
            {
                state.interleaved[frame * 2 + 0] += state.stereo.data[0][frame] * gain;
                state.interleaved[frame * 2 + 1] += state.stereo.data[1][frame] * gain;
            }
        }
    }

    static void SDLCALL
        feed_device(void* userdata, SDL_AudioStream* stream, const int additional_amount, int)
    {
        auto& audio = *static_cast<AudioState*>(userdata);
        int remaining_bytes = additional_amount;
        while (remaining_bytes > 0)
        {
            const int block_bytes =
                std::min(remaining_bytes, FRAME_SIZE * 2 * static_cast<int>(sizeof(float)));
            const int frame_count = block_bytes / (2 * static_cast<int>(sizeof(float)));
            mix_block(audio, frame_count);
            SDL_PutAudioStreamData(
                stream,
                audio.backend->interleaved.data(),
                frame_count * 2 * sizeof(float));
            remaining_bytes -= block_bytes;
        }
    }

    //// SETUP ////

    static std::optional<std::reference_wrapper<AudioState::Backend>> ensure_audio_ready(
        AudioState& audio)
    {
        if (audio.backend)
            return *audio.backend;
        auto state = std::make_unique<AudioState::Backend>();

        auto context_settings = IPLContextSettings {};
        context_settings.version = STEAMAUDIO_VERSION;
        if (iplContextCreate(&context_settings, &state->context) != IPL_STATUS_SUCCESS)
        {
            TBX_ERROR("Steam Audio context creation failed; audio disabled");
            return {};
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
            TBX_ERROR("Steam Audio HRTF creation failed; audio disabled");
            return {};
        }
        iplAudioBufferAllocate(state->context, 1, FRAME_SIZE, &state->mono);
        iplAudioBufferAllocate(state->context, 2, FRAME_SIZE, &state->stereo);

        // The callback receives the value-held AudioState (stable inside the heap-held
        // RuntimeState): master volume + backend buffers together.
        audio.backend = std::move(state);
        AudioState::Backend& backend = *audio.backend;
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
            TBX_WARN("SDL audio unavailable ({}); spatializer runs silent", SDL_GetError());
        else
        {
            auto spec = SDL_AudioSpec {};
            spec.format = SDL_AUDIO_F32;
            spec.channels = 2;
            spec.freq = SAMPLE_RATE;
            backend.stream = SDL_OpenAudioDeviceStream(
                SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
                &spec,
                feed_device,
                &audio);
            if (backend.stream)
                SDL_ResumeAudioStreamDevice(backend.stream);
            else
                TBX_WARN("no audio playback device ({}); spatializer runs silent", SDL_GetError());
        }
        return backend;
    }

    //// BOUNDARY ////

    void update_audio(
        AudioState& audio,
        Sandbox& sandbox,
        AssetsState& assets,
        EventsState& events,
        const float)
    {
        const auto ready = ensure_audio_ready(audio);
        if (!ready)
            return;
        AudioState::Backend& state = ready->get();

        // The first enabled listener frames the world; no listener, everything is silent.
        auto listener_inverse = Mat4(1.0f);
        bool has_listener = false;
        sandbox.for_each_with<AudioListener>(
            [&](Toy toy, AudioListener& listener)
            {
                if (has_listener || !toy.is_enabled())
                    return;
                listener_inverse = inverse(toy.get_world_transform());
                state.listener_volume = listener.volume;
                has_listener = true;
            });

        const std::scoped_lock lock(state.voices_mutex);

        // Mirror playing sources into voices; spatial extent comes from the toy's Collider.
        sandbox.for_each_with<AudioSource>(
            [&](Toy toy, AudioSource& source)
            {
            const auto key = static_cast<uint32>(toy.get_id());
            const bool wants_voice = has_listener && toy.is_enabled() && source.is_playing
                                     && source.clip.is_valid();
            auto existing = state.voices.find(key);

            if (!wants_voice)
            {
                if (existing != state.voices.end())
                    state.voices.erase(existing); // the holder releases the effect
                return;
            }

            if (existing == state.voices.end())
            {
                // Clips copy once into a shared cache so hot reloads never race the mixer.
                auto& cached = state.clips[source.clip.id];
                if (!cached)
                {
                    const auto clip = load_asset_now(assets, events, source.clip); // resolves by tracked path
                    if (!clip)
                        return;
                    cached = std::make_shared<AudioClip>(clip->get());
                }
                auto voice = Voice {};
                voice.clip = cached;
                auto audio_settings = IPLAudioSettings {};
                audio_settings.samplingRate = SAMPLE_RATE;
                audio_settings.frameSize = FRAME_SIZE;
                auto effect_settings = IPLBinauralEffectSettings {};
                effect_settings.hrtf = state.hrtf;
                auto effect = IPLBinauralEffect(nullptr);
                iplBinauralEffectCreate(state.context, &audio_settings, &effect_settings, &effect);
                voice.effect.reset(effect);
                existing = state.voices.emplace(key, std::move(voice)).first;
            }

            Voice& voice = existing->second;
            if (voice.is_finished)
            {
                source.is_playing = false;
                return;
            }

            const Mat4 world = toy.get_world_transform();
            const Vec3 local = Vec3(listener_inverse * world * Vec4(0.0f, 0.0f, 0.0f, 1.0f));
            const float distance = length(local);
            const Vec3 direction =
                distance > 0.0001f ? local * (1.0f / distance) : Vec3(0.0f, 0.0f, -1.0f);

            // A Collider softens attenuation by its extent (the shared Shape vocabulary).
            float extent = 0.0f;
            if (const auto* collider = toy.try_get_block<Collider>())
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
                    case Shape::MESH:
                        break; // no analytic extent — point-source attenuation
                }
            }
            voice.direction = IPLVector3 {direction.x, direction.y, direction.z};
            voice.attenuation = 1.0f / (1.0f + std::max(0.0f, distance - extent));
            voice.volume = source.volume;
            voice.is_looping = source.is_looping;
            });

        // Reap voices whose toy despawned (or lost its AudioSource) while playing — the
        // mirror loop above never revisits them, so they would otherwise mix and hold their
        // effects until shutdown (mirrors the physics body sweep).
        for (auto it = state.voices.begin(); it != state.voices.end();)
        {
            auto toy = Toy(sandbox, static_cast<ToyId>(it->first));
            const bool is_stale = !toy.is_alive() || !toy.has_block<AudioSource>();
            it = is_stale ? state.voices.erase(it) : std::next(it);
        }
    }
}
