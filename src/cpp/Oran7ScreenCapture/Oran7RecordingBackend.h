#pragma once
#include "../Oran7Config/Oran7AppConfig.h"
#include "Oran7RecordingFeed.h"
#include "Oran7RecordingSession.h"
#include "Live/Oran7LiveAudioMixer.h"

class Oran7AudioCaptureFeed;
class Oran7RecordingBackend {
public:
    static QString run(const Oran7RecordingConfig& config, std::shared_ptr<Oran7RecordingFeed> feed,
                       const std::atomic_bool& cancelled, const Oran7RecordingCallbacks& callbacks, std::shared_ptr<Oran7LiveAudioControl> audioControl = {}, std::shared_ptr<Oran7AudioCaptureFeed> audioFeed = {});
    static QVariantMap settings(const Oran7RecordingConfig& config);
    static QString apply(const QVariantMap& values, Oran7RecordingConfig* config);
    static QString validate(const Oran7RecordingConfig& config, bool starting);
};
