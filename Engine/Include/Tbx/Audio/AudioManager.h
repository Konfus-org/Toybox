#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/AudioMixer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/AppEvents.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Stages/Stage.h"
#include <vector>

namespace Tbx
{
    enum class TBX_EXPORT AudioState
    {
        Playing,
        Paused,
        Muted,
        Stopped
    };

    class TBX_EXPORT IAudioManager
    {
    public:
        virtual ~IAudioManager();
        virtual void Update() = 0;

    public:
        AudioState State = AudioState::Playing;
    };

    class TBX_EXPORT HeadlessAudioManager final : public IAudioManager
    {
    public:
        HeadlessAudioManager();
        void Update() override {}
    };

    /// <summary>
    /// Coordinates audio playback by querying stages for active audio blocks and delegating to the mixer.
    /// </summary>
    class TBX_EXPORT AudioManager final : public IAudioManager
    {
    public:
        AudioManager();
        AudioManager(Ref<IAudioMixer> mixer, Ref<EventBus> eventBus);

        void Update() override;

    private:
        void OnAppStatusChangedEvent(const AppStatusChangedEvent& e);
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);
        void ProcessStages();
        void ProcessStage(const Stage* stage);

    private:
        Ref<IAudioMixer> _mixer = nullptr;
        EventListener _eventListener = {};
        std::vector<const Stage*> _openStages = {};
        AudioState _preMinimizeState = AudioState::Playing;
        bool _stopped = false;

    };
}
