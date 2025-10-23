#pragma once
#include <Tbx/Audio/AudioMixer.h>
#include <Tbx/Assets/AssetLoaders.h>
#include <Tbx/Plugins/Plugin.h>
#include <SDL3/SDL_audio.h>
#include <filesystem>
#include <unordered_map>

namespace Tbx::Plugins::SDL3Audio
{
    struct SDLAudio : public Audio, public IProductOfPluginFactory
    {
        using Audio::Audio;
    };

    struct StereoSpace
    {
        float Left = 1.0f;
        float Right = 1.0f;
    };

    struct SpatialSettings
    {
        bool Requested = false;
        bool Enabled = false;
        StereoSpace Gain = {};
    };

    struct PlaybackInstance
    {
        SDL_AudioStream* Stream = nullptr;
        float Pitch = 1.0f;
        float Speed = 1.0f;
        float Volume = 1.0f;
        bool Loop = false;
        bool IsPlaying = false;
        bool Spatial = false;
        StereoSpace SpatialGain = {};
    };

    struct PlaybackParams
    {
        float Volume = 1.0f;
        float Pitch = 1.0f;
        float Speed = 1.0f;
        bool Looping = false;
        StereoSpace Stereo = {};
    };

    class SDL3AudioPlugin final
        : public FactoryPlugin<SDLAudio>
        , public IAudioLoader
        , public IAudioMixer
    {
    public:
        SDL3AudioPlugin(Ref<EventBus> eventBus);
        ~SDL3AudioPlugin() override;

        void Play(const Audio& audio) override;
        void Pause(const Audio& audio) override;
        void Stop(const Audio& audio) override;

        void SetPosition(const Audio& audio, const Vector3& position) override;
        void SetPitch(const Audio& audio, float pitch) override;
        void SetPlaybackSpeed(const Audio& audio, float speed) override;
        void SetLooping(const Audio& audio, bool loop) override;
        void SetVolume(const Audio& audio, float volume) override;

        bool CanLoadAudio(const std::filesystem::path& filepath) const override;

    protected:
        Ref<Audio> LoadAudio(const std::filesystem::path& filepath) override;

    private:
        bool SetPlaybackParams(PlaybackInstance& instance, const Audio& audio, const PlaybackParams& params);
        PlaybackInstance* GetOrCreatePlayback(const Audio& audio, const SpatialSettings* spatial, bool createIfMissing);
        bool BuildPlaybackStream(PlaybackInstance& instance, const Audio& audio, const SpatialSettings& settings);
        bool ConfigureChannelMap(PlaybackInstance& instance, const StereoSpace& stereo);
        bool SubmitAudioData(PlaybackInstance& instance, const Audio& audio, bool resetStream);
        void StartPlayback(const Audio& audio, const SpatialSettings& spatial);
        void RemovePlayback(const Audio& audio, PlaybackInstance& instance);
        void DestroyPlayback(PlaybackInstance& instance);

        SpatialSettings ResolveSpatialSettings(const Audio& audio) const;
        SpatialSettings ResolveSpatialSettings(const Audio& audio, const Vector3& position) const;

        static bool IsSupportedExtension(const std::filesystem::path& path);
        static AudioFormat ConvertSpecToFormat(const SDL_AudioSpec& spec);
        static SDL_AudioSpec ConvertFormatToSpec(const AudioFormat& format);

    private:
        SDL_AudioDeviceID _device = 0;
        SDL_AudioSpec _deviceSpec = {};
        std::unordered_map<Uid, PlaybackInstance> _playbackInstances = {};
    };

    TBX_REGISTER_PLUGIN(SDL3AudioPlugin);
}
