#pragma once
#include <QString>
class NcmSession {
public:
    static NcmSession& instance() { static NcmSession session; return session; }
    QString ffmpegCookieString() const { return {}; }
};
