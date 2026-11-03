#pragma once
#include <QString>
class Oran7BilibiliAuthManager {
public:
    static Oran7BilibiliAuthManager& instance() { static Oran7BilibiliAuthManager manager; return manager; }
    QString ffmpegCookieString() const { return {}; }
};
