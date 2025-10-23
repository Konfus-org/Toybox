#include "SDL3AudioPlugin.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Debug/Tracers.h"
#include <SDL3/SDL_init.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Tbx::Plugins::SDL3Audio
{
    // Calculate stereo gains for a spatialized listener. Distance attenuates the overall
    // volume while the horizontal angle determines a simple left/right pan.
    static StereoSpace CalculateSpatialGains(const Vector3& position)
    {
        const float x = position.X;
        const float y = position.Y;
        const float z = position.Z;

        // Use an inverse distance rolloff so sounds closer than the reference distance
        // remain at full volume and gradually attenuate as they move away.
        const float distance = std::sqrt(x * x + y * y + z * z);
        constexpr float minDistance = 1.0f;
        constexpr float rolloff = 0.08f;
        const float attenuatedDistance = std::max(distance - minDistance, 0.0f);
        const float attenuation = 1.0f / (1.0f + rolloff * attenuatedDistance);

        // Determine pan by projecting onto the XZ plane and normalising.
        const float horizontal = std::sqrt(x * x + z * z);
        float pan = 0.0f;
        if (horizontal > std::numeric_limits<float>::epsilon())
        {
            pan = std::clamp(x / horizontal, -1.0f, 1.0f);
        }

        // Convert [-1, 1] pan into equal power stereo gains so panning does not change
        // perceived loudness.
        StereoSpace space = {};
        space.Left = attenuation * std::sqrt(std::max(0.0f, 0.5f * (1.0f - pan)));
        space.Right = attenuation * std::sqrt(std::max(0.0f, 0.5f * (1.0f + pan)));
        return space;
    }

    static PlaybackParams BuildParamsFromInstance(const PlaybackInstance& instance)
    {
        PlaybackParams params = {};
        params.Volume = instance.Volume;
        params.Pitch = instance.Pitch;
        params.Speed = instance.Speed;
        params.Looping = instance.Loop;
        if (instance.Spatial)
        {
            params.Stereo = instance.SpatialGain;
        }
        return params;
    }

    SDL3AudioPlugin::SDL3AudioPlugin(Ref<EventBus> eventBus)
    {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to initialize SDL audio subsystem: {}", SDL_GetError());
        }

        SDL_AudioSpec desired = {};
        desired.format = SDL_AUDIO_F32;
        desired.channels = 2;
        desired.freq = 48000;

        _device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired);
        if (_device == 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to open SDL audio device: {}", SDL_GetError());
        }

        if (!SDL_GetAudioDeviceFormat(_device, &_deviceSpec, nullptr))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to query audio device format: {}", SDL_GetError());
            SDL_CloseAudioDevice(_device);
        }

        if (!SDL_ResumeAudioDevice(_device))
        {
            TBX_TRACE_WARNING("SDL3Audio: Unable to resume audio device: {}", SDL_GetError());
        }

        TBX_TRACE_INFO("SDL3Audio: Initialized with device format {}, Hz {}, channels {}", 
            _deviceSpec.freq, static_cast<int>(_deviceSpec.channels), SDL_GetAudioFormatName(_deviceSpec.format));
    }

    SDL3AudioPlugin::~SDL3AudioPlugin()
    {
        SDL_PauseAudioDevice(_device);

        for (auto& [_, playback] : _playbackInstances)
        {
            DestroyPlayback(playback);
        }
        _playbackInstances.clear();

        SDL_CloseAudioDevice(_device);
        _device = 0;

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        
        // Allow whichever plugin shuts down last to clean up SDL globally.
        if (SDL_WasInit(0) == 0)
        {
            SDL_Quit();
        }
    }

    void SDL3AudioPlugin::Play(const Audio& audio)
    {
        StartPlayback(audio, ResolveSpatialSettings(audio));
    }

    void SDL3AudioPlugin::Pause(const Audio& audio)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        SDL_PauseAudioStreamDevice(it->second.Stream);
    }

    void SDL3AudioPlugin::Stop(const Audio& audio)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it == _playbackInstances.end())
        {
            return;
        }

        it->second.IsPlaying = false;
        RemovePlayback(audio, it->second);
    }

    void SDL3AudioPlugin::SetPosition(const Audio& audio, const Vector3& position)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, nullptr, false);
        if (instance == nullptr)
        {
            return;
        }

        auto spacialSettings = ResolveSpatialSettings(audio, position);
        if (spacialSettings.Enabled)
        {
            PlaybackParams params = BuildParamsFromInstance(*instance);
            if (!SetPlaybackParams(*instance, audio, params))
            {
                RemovePlayback(audio, *instance);
            }
        }
        else
        {
            TBX_TRACE_WARNING("SDL3Audio: Spatial playback requested for asset {} but it could not be set. Is the audio device stereo?", audio.Id.ToString());
        }
    }

    void SDL3AudioPlugin::SetPitch(const Audio& audio, float pitch)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, nullptr, false);
        if (instance == nullptr)
        {
            return;
        }

        PlaybackParams params = BuildParamsFromInstance(*instance);
        params.Pitch = pitch;

        if (!SetPlaybackParams(*instance, audio, params))
        {
            RemovePlayback(audio, *instance);
        }
    }

    void SDL3AudioPlugin::SetPlaybackSpeed(const Audio& audio, float speed)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, nullptr, false);
        if (instance == nullptr)
        {
            return;
        }

        PlaybackParams params = BuildParamsFromInstance(*instance);
        params.Speed = speed;

        if (!SetPlaybackParams(*instance, audio, params))
        {
            RemovePlayback(audio, *instance);
        }
    }

    void SDL3AudioPlugin::SetLooping(const Audio& audio, bool loop)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, nullptr, false);
        if (instance == nullptr)
        {
            return;
        }

        PlaybackParams params = BuildParamsFromInstance(*instance);
        params.Looping = loop;

        if (!SetPlaybackParams(*instance, audio, params))
        {
            RemovePlayback(audio, *instance);
        }
    }

    void SDL3AudioPlugin::SetVolume(const Audio& audio, float volume)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, nullptr, false);
        if (instance == nullptr)
        {
            return;
        }

        PlaybackParams params = BuildParamsFromInstance(*instance);
        params.Volume = volume;

        if (!SetPlaybackParams(*instance, audio, params))
        {
            RemovePlayback(audio, *instance);
        }
    }

    bool SDL3AudioPlugin::CanLoadAudio(const std::filesystem::path& filepath) const
    {
        return IsSupportedExtension(filepath);
    }

    Ref<Audio> SDL3AudioPlugin::LoadAudio(const std::filesystem::path& filepath)
    {
        SDL_AudioSpec sourceSpec = {};
        Uint8* rawBuffer = nullptr;
        Uint32 rawLength = 0;

        if (filepath.extension() == ".wav" || filepath.extension() == ".wave")
        {
            if (!SDL_LoadWAV(filepath.string().c_str(), &sourceSpec, &rawBuffer, &rawLength))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to load '{}': {}", filepath.string(), SDL_GetError());
                return nullptr;
            }
        }
        else
        {
            TBX_ASSERT(false, "SDL3Audio: Unsupported audio file format.");
            return nullptr;
        }

        SDL_AudioSpec targetSpec = sourceSpec;
        targetSpec.format = SDL_AUDIO_F32;

        Uint8* convertedBuffer = nullptr;
        int convertedLength = 0;
        bool converted = SDL_ConvertAudioSamples(&sourceSpec, rawBuffer, static_cast<int>(rawLength), &targetSpec, &convertedBuffer, &convertedLength);
        SDL_free(rawBuffer);

        if (!converted)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to convert audio '{}': {}", filepath.string(), SDL_GetError());
            return nullptr;
        }

        AudioFormat format = ConvertSpecToFormat(targetSpec);
        SampleData samples(convertedBuffer, convertedBuffer + convertedLength);
        SDL_free(convertedBuffer);
        auto audio = MakeRef<SDLAudio>(samples, format);
        //audio->Owner = shared_from_this();
        return audio;
    }

    bool SDL3AudioPlugin::SetPlaybackParams(PlaybackInstance& instance, const Audio& audio, const PlaybackParams& params)
    {
        if (!instance.Stream)
        {
            return false;
        }

        const bool wasLooping = instance.Loop;
        const StereoSpace previousSpace = instance.SpatialGain;

        instance.Volume = params.Volume;
        instance.Pitch = params.Pitch;
        instance.Speed = params.Speed;
        instance.Loop = params.Looping;

        if (instance.Spatial)
        {
            instance.SpatialGain = params.Stereo;
        }
        else
        {
            instance.SpatialGain = StereoSpace{};
        }

        const float ratio = std::clamp(instance.Pitch * instance.Speed, 0.01f, 100.0f);
        if (!SDL_SetAudioStreamFrequencyRatio(instance.Stream, ratio))
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to adjust audio stream playback ratio: {}", SDL_GetError());
        }

        if (!SDL_SetAudioStreamGain(instance.Stream, instance.Volume))
        {
            TBX_TRACE_WARNING("SDL3Audio: Failed to adjust audio stream volume: {}", SDL_GetError());
        }

        if (!instance.IsPlaying)
        {
            return true;
        }

        const bool spatialChanged = instance.Spatial && (previousSpace.Left != instance.SpatialGain.Left || previousSpace.Right != instance.SpatialGain.Right);
        if (spatialChanged)
        {
            if (!ConfigureChannelMap(instance, instance.SpatialGain))
            {
                TBX_TRACE_WARNING("SDL3Audio: Unable to refresh spatial channel map: {}", SDL_GetError());
            }
        }

        if (instance.Loop)
        {
            const int queued = SDL_GetAudioStreamQueued(instance.Stream);
            if (queued < 0)
            {
                TBX_TRACE_WARNING("SDL3Audio: Failed to query queued audio for asset {}: {}", audio.Id.ToString(), SDL_GetError());
                return true;
            }

            if (queued == 0 || !wasLooping)
            {
                return SubmitAudioData(instance, audio, false);
            }
        }

        return true;
    }

    SpatialSettings SDL3AudioPlugin::ResolveSpatialSettings(const Audio& audio) const
    {
        SpatialSettings settings = {};
        settings.Requested = false;
        return settings;
    }

    SpatialSettings SDL3AudioPlugin::ResolveSpatialSettings(const Audio& audio, const Vector3& position) const
    {
        SpatialSettings settings = {};
        settings.Requested = true;

        // Spatial playback relies on float samples so we can mix them directly into stereo output.
        const bool formatSupportsSpatial = audio.Format.SampleFormat == AudioSampleFormat::Float32 && audio.Format.Channels > 0;
        if (!formatSupportsSpatial)
        {
            TBX_TRACE_WARNING("SDL3Audio: Spatial playback requested for asset {} but unsupported format was provided.", audio.Id.ToString());
            return settings;
        }

        // Panning requires at least two channels on the output device.
        if (_deviceSpec.channels < 2)
        {
            TBX_TRACE_WARNING("SDL3Audio: Spatial playback requested for asset {} but the audio device is not stereo.", audio.Id.ToString());
            return settings;
        }

        // Pre-calculate the gains so any subsequently queued buffers reuse the same spatial values.
        settings.Enabled = true;
        settings.Gain = CalculateSpatialGains(position);
        return settings;
    }

    PlaybackInstance* SDL3AudioPlugin::GetOrCreatePlayback(const Audio& audio, const SpatialSettings* spatial, bool createIfMissing)
    {
        auto it = _playbackInstances.find(audio.Id);
        if (it != _playbackInstances.end())
        {
            PlaybackInstance& instance = it->second;
            if (spatial != nullptr)
            {
                const bool layoutMismatch = instance.Stream != nullptr && instance.Spatial != spatial->Enabled;
                if (layoutMismatch)
                {
                    DestroyPlayback(instance);
                }

                instance.Spatial = spatial->Enabled;
                if (layoutMismatch || instance.Stream == nullptr)
                {
                    instance.SpatialGain = instance.Spatial ? spatial->Gain : StereoSpace{};
                    if (!BuildPlaybackStream(instance, audio, *spatial))
                    {
                        RemovePlayback(audio, instance);
                        return nullptr;
                    }
                }
            }

            if (instance.Stream == nullptr)
            {
                SpatialSettings resolved = spatial ? *spatial : ResolveSpatialSettings(audio);
                instance.Spatial = resolved.Enabled;
                instance.SpatialGain = instance.Spatial ? resolved.Gain : StereoSpace{};
                if (!BuildPlaybackStream(instance, audio, resolved))
                {
                    RemovePlayback(audio, instance);
                    return nullptr;
                }
            }
            return &instance;
        }

        if (!createIfMissing)
        {
            return nullptr;
        }

        SpatialSettings resolved = spatial ? *spatial : ResolveSpatialSettings(audio);
        PlaybackInstance instance = {};
        instance.Spatial = resolved.Enabled;
        instance.SpatialGain = instance.Spatial ? resolved.Gain : StereoSpace{};

        if (!BuildPlaybackStream(instance, audio, resolved))
        {
            return nullptr;
        }

        auto [iter, inserted] = _playbackInstances.try_emplace(audio.Id, std::move(instance));
        if (!inserted)
        {
            DestroyPlayback(instance);
            return &iter->second;
        }

        return &iter->second;
    }

    bool SDL3AudioPlugin::BuildPlaybackStream(PlaybackInstance& instance, const Audio& audio, const SpatialSettings& settings)
    {
        DestroyPlayback(instance);

        if (audio.Format.SampleFormat == AudioSampleFormat::Unknown || audio.Data.empty())
        {
            TBX_TRACE_WARNING("SDL3Audio: Audio asset {} contains no playable data.", audio.Id.ToString());
            return false;
        }

        SDL_AudioSpec sourceSpec = ConvertFormatToSpec(audio.Format);
        if (sourceSpec.format == 0)
        {
            TBX_TRACE_WARNING("SDL3Audio: Unsupported audio sample format for asset {}.", audio.Id.ToString());
            return false;
        }

        if (settings.Enabled)
        {
            sourceSpec.format = SDL_AUDIO_F32;
            sourceSpec.channels = 2;
        }

        SDL_AudioStream* stream = SDL_CreateAudioStream(&sourceSpec, &_deviceSpec);
        if (stream == nullptr)
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to create audio stream: {}", SDL_GetError());
            return false;
        }

        if (!SDL_BindAudioStream(_device, stream))
        {
            TBX_TRACE_ERROR("SDL3Audio: Failed to bind audio stream: {}", SDL_GetError());
            SDL_DestroyAudioStream(stream);
            return false;
        }

        instance.Stream = stream;
        if (settings.Enabled)
        {
            if (!ConfigureChannelMap(instance, settings.Gain))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to configure spatial channel map: {}", SDL_GetError());
                SDL_UnbindAudioStream(stream);
                SDL_DestroyAudioStream(stream);
                instance.Stream = nullptr;
                return false;
            }
        }

        if (!SubmitAudioData(instance, audio, true))
        {
            SDL_UnbindAudioStream(stream);
            SDL_DestroyAudioStream(stream);
            instance.Stream = nullptr;
            return false;
        }

        return true;
    }

    bool SDL3AudioPlugin::ConfigureChannelMap(PlaybackInstance& instance, const StereoSpace& stereo)
    {
        /*const int outputChannels = static_cast<int>(_deviceSpec.channels);
        if (outputChannels < 2)
        {
            return false;
        }

        const int inputChannels = 2;
        std::vector<int> channelMap(static_cast<size_t>(outputChannels) * static_cast<size_t>(inputChannels), 0.0f);
        const auto index = [inputChannels](int destination, int source) -> size_t
            {
                return static_cast<size_t>(destination) * static_cast<size_t>(inputChannels) + static_cast<size_t>(source);
            };

        channelMap[index(0, 0)] = stereo.Left;
        if (outputChannels >= 2)
        {
            channelMap[index(1, 1)] = stereo.Right;
        }
        int count;
        return SDL_SetAudioStreamOutputChannelMap(instance.Stream, channelMap.data(), count);*/
        return true;
    }

    bool SDL3AudioPlugin::SubmitAudioData(PlaybackInstance& instance, const Audio& audio, bool resetStream)
    {
        if (!instance.Stream || audio.Data.empty())
        {
            return false;
        }

        if (resetStream)
        {
            if (!SDL_ClearAudioStream(instance.Stream))
            {
                TBX_TRACE_WARNING("SDL3Audio: Failed to clear audio stream: {}", SDL_GetError());
            }
        }

        const auto queueRaw = [&](const void* buffer, size_t size) -> bool
        {
            if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
            {
                TBX_TRACE_ERROR("SDL3Audio: Audio asset {} is too large to queue for playback.", audio.Id.ToString());
                return false;
            }

            // SDL streams are fed with raw bytes and expect an explicit flush to make the
            // new data available to the device.
            if (!SDL_PutAudioStreamData(instance.Stream, buffer, static_cast<int>(size)))
            {
                TBX_TRACE_ERROR("SDL3Audio: Failed to queue audio data: {}", SDL_GetError());
                return false;
            }

            if (!SDL_FlushAudioStream(instance.Stream))
            {
                TBX_TRACE_WARNING("SDL3Audio: Failed to flush audio stream: {}", SDL_GetError());
            }

            return true;
        };

        const auto dataSize = audio.Data.size();
        if (!instance.Spatial)
        {
            return queueRaw(audio.Data.data(), dataSize);
        }

        if (audio.Format.SampleFormat != AudioSampleFormat::Float32)
        {
            TBX_TRACE_ERROR("SDL3Audio: Spatial playback requires float32 audio data for asset {}.", audio.Id.ToString());
            return false;
        }

        if (dataSize % sizeof(float) != 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Unexpected audio buffer size for asset {}.", audio.Id.ToString());
            return false;
        }

        const int channels = std::max(audio.Format.Channels, 1);
        const size_t sampleCount = dataSize / sizeof(float);
        if (sampleCount == 0 || sampleCount % static_cast<size_t>(channels) != 0)
        {
            TBX_TRACE_ERROR("SDL3Audio: Spatial playback could not interpret audio samples for asset {}.", audio.Id.ToString());
            return false;
        }

        const size_t frameCount = sampleCount / static_cast<size_t>(channels);
        std::vector<float> processed(frameCount * 2);
        const float invChannelCount = 1.0f / static_cast<float>(channels);
        const float* samples = reinterpret_cast<const float*>(audio.Data.data());

        for (size_t frame = 0; frame < frameCount; ++frame)
        {
            float monoSample = 0.0f;
            const size_t baseIndex = frame * static_cast<size_t>(channels);
            for (int channel = 0; channel < channels; ++channel)
            {
                // Average all channels to produce a single mono sample before handing it
                // to the channel map for stereo distribution.
                monoSample += samples[baseIndex + static_cast<size_t>(channel)];
            }
            monoSample *= invChannelCount;
            processed[frame * 2] = monoSample;
            processed[frame * 2 + 1] = monoSample;
        }

        return queueRaw(processed.data(), processed.size() * sizeof(float));
    }

    void SDL3AudioPlugin::StartPlayback(const Audio& audio, const SpatialSettings& spatial)
    {
        PlaybackInstance* instance = GetOrCreatePlayback(audio, &spatial, true);
        if (instance == nullptr)
        {
            return;
        }

        PlaybackParams params = BuildParamsFromInstance(*instance);
        if (spatial.Enabled)
        {
            params.Stereo = spatial.Gain;
        }

        instance->IsPlaying = true;
        if (!SetPlaybackParams(*instance, audio, params))
        {
            RemovePlayback(audio, *instance);
        }
        SDL_ResumeAudioStreamDevice(instance->Stream);
    }

    void SDL3AudioPlugin::RemovePlayback(const Audio& audio, PlaybackInstance& instance)
    {
        DestroyPlayback(instance);
        _playbackInstances.erase(audio.Id);
    }

    void SDL3AudioPlugin::DestroyPlayback(PlaybackInstance& instance)
    {
        if (!instance.Stream)
        {
            return;
        }

        SDL_UnbindAudioStream(instance.Stream);
        SDL_ClearAudioStream(instance.Stream);
        SDL_DestroyAudioStream(instance.Stream);
        instance.Stream = nullptr;
    }

    bool SDL3AudioPlugin::IsSupportedExtension(const std::filesystem::path& path)
    {
        const auto extension = path.extension().string();
        return extension == ".wav" || extension == ".wave";
    }

    AudioFormat SDL3AudioPlugin::ConvertSpecToFormat(const SDL_AudioSpec& spec)
    {
        AudioSampleFormat sampleFormat = AudioSampleFormat::Unknown;
        switch (spec.format)
        {
        case SDL_AUDIO_U8:
            sampleFormat = AudioSampleFormat::UInt8;
            break;
        case SDL_AUDIO_S16:
            sampleFormat = AudioSampleFormat::Int16;
            break;
        case SDL_AUDIO_S32:
            sampleFormat = AudioSampleFormat::Int32;
            break;
        case SDL_AUDIO_F32:
            sampleFormat = AudioSampleFormat::Float32;
            break;
        default:
            sampleFormat = AudioSampleFormat::Unknown;
            break;
        }

        AudioFormat format = {};
        format.SampleFormat = sampleFormat;
        format.SampleRate = spec.freq;
        format.Channels = spec.channels;
        return format;
    }

    SDL_AudioSpec SDL3AudioPlugin::ConvertFormatToSpec(const AudioFormat& format)
    {
        SDL_AudioSpec spec = {};
        spec.freq = format.SampleRate;
        spec.channels = static_cast<Uint8>(std::max(0, format.Channels));

        switch (format.SampleFormat)
        {
        case AudioSampleFormat::UInt8:
            spec.format = SDL_AUDIO_U8;
            break;
        case AudioSampleFormat::Int16:
            spec.format = SDL_AUDIO_S16;
            break;
        case AudioSampleFormat::Int32:
            spec.format = SDL_AUDIO_S32;
            break;
        case AudioSampleFormat::Float32:
            spec.format = SDL_AUDIO_F32;
            break;
        default:
            TBX_ASSERT(false, "SDL3Audio: Unsupported audio sample format.");
            spec.format = SDL_AUDIO_UNKNOWN;
            break;
        }

        return spec;
    }
}
