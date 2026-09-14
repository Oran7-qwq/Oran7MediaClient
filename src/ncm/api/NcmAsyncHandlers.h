#ifndef NCM_ASYNC_HANDLERS_H
#define NCM_ASYNC_HANDLERS_H

#include <QVariant>
#include <QVariantMap>

namespace ncm {

// Oran7AsyncManager 的三个阻塞式网络 handler（在 QThreadPool 工作线程执行）。
// 同步返回 QVariant；失败抛 std::exception，由 Oran7AsyncManager::run 转成 failed 回调。
QVariant musicSearchHandler(const QVariantMap& args);      // "musicSearch"
QVariant fetchLikedSongsHandler(const QVariantMap& args);  // "fetchLikedSongs"
QVariant roamSongsHandler(const QVariantMap& args);        // "roamSongs"

} // namespace ncm

#endif // NCM_ASYNC_HANDLERS_H
