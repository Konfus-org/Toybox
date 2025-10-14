#include "Tbx/PCH.h"
#include "Tbx/Audio/AudioManager.h"
#include "Tbx/Audio/Audio.h"
#include "Tbx/Graphics/Camera.h"
#include "Tbx/Stages/Views.h"
#include "Tbx/Math/Transform.h"
#include <algorithm>
#include <limits>
#include <vector>

namespace Tbx
{
    AudioManager::AudioManager(Ref<IAudioMixer> mixer, Ref<EventBus> eventBus)
        : _mixer(std::move(mixer))
        , _eventListener(eventBus)
    {
        _eventListener.Listen<StageOpenedEvent>([this](const StageOpenedEvent& e) { OnStageOpened(e); });
        _eventListener.Listen<StageClosedEvent>([this](const StageClosedEvent& e) { OnStageClosed(e); });
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
        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it == _openStages.end())
        {
            _openStages.push_back(stage);
        }
    }

    void AudioManager::OnStageClosed(const StageClosedEvent& e)
    {
        auto stage = e.GetStage();
        auto it = std::find(_openStages.begin(), _openStages.end(), stage);
        if (it != _openStages.end())
        {
            _openStages.erase(it);
        }
    }

    void AudioManager::ProcessStage(const Stage* stage)
    {
        if (!stage || !stage->Root)
        {
            return;
        }

        std::vector<Vector3> cameraPositions = {};
        auto cameraView = StageView<Camera>(stage->Root);
        for (const auto& cameraToy : cameraView)
        {
            if (!cameraToy)
            {
                continue;
            }

            Ref<Transform> cameraTransform;
            if (cameraToy->TryGet(cameraTransform) && cameraTransform)
            {
                cameraPositions.push_back(cameraTransform->Position);
            }
        }

        auto view = StageView<AudioSource>(stage->Root);
        for (const auto& toy : view)
        {
            if (!toy)
            {
                continue;
            }

            Ref<AudioSource> audioBlock;
            if (!toy->Blocks.TryGet<AudioSource>(audioBlock) || !audioBlock)
            {
                continue;
            }

            auto& source = *audioBlock;
            auto& audio = *audioBlock->Audio;
            _mixer->SetLooping(audio, source.Looping);
            if (source.Playing)
            {
                _mixer->SetPitch(audio, source.Pitch);
                _mixer->SetPlaybackSpeed(audio, source.PlaybackSpeed);

                Ref<Transform> audioTransform;
                toy->Blocks.TryGet<Transform>(audioTransform);
                if (audioTransform)
                {
                    Vector3 playPosition = audioTransform->Position;
                    if (!cameraPositions.empty())
                    {
                        Vector3 nearestCamera = cameraPositions.front();
                        float smallestDistanceSquared = std::numeric_limits<float>::max();
                        for (const auto& cameraPosition : cameraPositions)
                        {
                            Vector3 offset = playPosition - cameraPosition;
                            float distanceSquared = (offset.X * offset.X) + (offset.Y * offset.Y) + (offset.Z * offset.Z);
                            if (distanceSquared < smallestDistanceSquared)
                            {
                                smallestDistanceSquared = distanceSquared;
                                nearestCamera = cameraPosition;
                            }
                        }

                        playPosition -= nearestCamera;
                    }

                    _mixer->SetPosition(audio, playPosition);
                }
                _mixer->Play(audio);
            }
            else
            {
                _mixer->Stop(audio);
            }
        }
    }
}
