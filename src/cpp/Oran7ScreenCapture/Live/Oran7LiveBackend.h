#pragma once
#include "Oran7LiveSession.h"
#include "../Oran7RecordingFeed.h"
class Oran7AudioCaptureFeed;
class Oran7LiveBackend
{
  public:
    static QString run(const Oran7LiveConfig&, std::shared_ptr<Oran7RecordingFeed>, const std::atomic_bool&,
                       const Oran7RecordingCallbacks&, std::shared_ptr<Oran7LiveAudioControl> = {}, std::shared_ptr<Oran7AudioCaptureFeed> = {});
};
