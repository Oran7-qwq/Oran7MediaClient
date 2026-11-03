#pragma once
#include <QString>
class NcmSession { public: static NcmSession& instance() { static NcmSession s; return s; } QString cookieString() const { return "test-cookie"; } };
