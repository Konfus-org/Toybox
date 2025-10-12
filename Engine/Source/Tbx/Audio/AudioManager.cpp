#include "Tbx/PCH.h"
#include "Tbx/Audio/AudioManager.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Stages/Views.h"
#include <algorithm>

namespace Tbx
{
    AudioManager::AudioManager(Ref<IAudioMixer> mixer, Ref<EventBus> eventBus)
        : _mixer(std::move(mixer))
        , _eventListener(eventBus)
        , _eventBus(std::move(eventBus))
    {
        if (!_eventBus)
        {
            return;
        }

        _eventListener.Listen(this, &AudioManager::OnStageOpened);
        _eventListener.Listen(this, &AudioManager::OnStageClosed);
    }

    void AudioManager::Update()
    {
        if (!_mixer)
        {
            return;
        }

        for (auto it = _openStages.begin(); it != _openStages.end();)
        {
            if (!(*it))
            {
                it = _openStages.erase(it);
                continue;
            }

            ProcessStage(*it);
            ++it;
        }
    }

    void AudioManager::OnStageOpened(const StageOpenedEvent& e)
    {
        auto stage = e.GetStage();
        if (!stage)
        {
            return;
        }

        auto exists = std::find_if(_openStages.begin(), _openStages.end(),
            [&stage](const Ref<Stage>& candidate)
            {
                return candidate == stage;
            });
        if (exists != _openStages.end())
        {
            return;
        }

        _openStages.push_back(stage);
    }

    void AudioManager::OnStageClosed(const StageClosedEvent& e)
    {
        auto stage = e.GetStage();
        if (!stage)
        {
            return;
        }

        _openStages.erase(std::remove_if(_openStages.begin(), _openStages.end(),
            [&stage](const Ref<Stage>& candidate)
            {
                return candidate == stage;
            }), _openStages.end());
    }

    void AudioManager::ProcessStage(const Ref<Stage>& stage)
    {
        if (!stage || !stage->Root)
        {
            return;
        }

        StageView<Audio> view(stage->Root);
        for (const auto& toy : view)
        {
            if (!toy)
            {
                continue;
            }

            Ref<Audio> audioBlock;
            if (!toy->Blocks.TryGet<Audio>(audioBlock) || !audioBlock)
            {
                continue;
            }

            auto& audio = *audioBlock;
            if (audio.IsPlaying())
            {
                _mixer->SetPitch(audio, audio.Pitch);
                _mixer->SetPlaybackSpeed(audio, audio.PlaybackSpeed);
                _mixer->Play(audio);
            }
            else
            {
                _mixer->Stop(audio);
            }
        }
    }
}
