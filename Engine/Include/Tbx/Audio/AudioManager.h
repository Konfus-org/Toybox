#pragma once
#include "Tbx/DllExport.h"
#include "Tbx/Audio/AudioMixer.h"
#include "Tbx/Events/EventBus.h"
#include "Tbx/Events/EventListener.h"
#include "Tbx/Events/StageEvents.h"
#include "Tbx/Stages/Stage.h"
#include <vector>

namespace Tbx
{
    /// <summary>
    /// Coordinates audio playback by querying stages for active audio blocks and delegating to the mixer.
    /// </summary>
    class TBX_EXPORT AudioManager
    {
    public:
        AudioManager() = default;
        AudioManager(Ref<IAudioMixer> mixer, Ref<EventBus> eventBus);

        void Update();

    private:
        void OnStageOpened(const StageOpenedEvent& e);
        void OnStageClosed(const StageClosedEvent& e);
        void ProcessStage(const Stage* stage);

    private:
        Ref<IAudioMixer> _mixer = nullptr;
        EventListener _eventListener = {};
        std::vector<const Stage*> _openStages = {};
    };
}
