#include "Oran7CoverNetworkFactory.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkRequest>
#include <QUrl>
#include <utility>

namespace
{
class Oran7CoverNetworkAccessManager final : public QNetworkAccessManager
{
  public:
    explicit Oran7CoverNetworkAccessManager(QObject* parent) : QNetworkAccessManager(parent) {}

  protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& original, QIODevice* outgoing) override
    {
        auto request = original;
        const auto url = request.url();
        const auto host = url.host().toLower();
        const bool cover = operation == GetOperation &&
                           (url.scheme() == "http" || url.scheme() == "https") && url.userInfo().isEmpty() &&
                           (host == "music.126.net" || host.endsWith(".music.126.net"));
        // 仅缓存公开 CDN 封面，不将登录/API 响应或带身份信息的请求写入此缓存。
        request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, cover);
        request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                             cover ? QNetworkRequest::PreferCache : QNetworkRequest::AlwaysNetwork);
        return QNetworkAccessManager::createRequest(operation, request, outgoing);
    }
};
} // namespace

Oran7CoverNetworkFactory::Oran7CoverNetworkFactory(QString cacheDirectory)
    : m_cacheDirectory(std::move(cacheDirectory))
{
}

QNetworkAccessManager* Oran7CoverNetworkFactory::create(QObject* parent)
{
    auto* manager = new Oran7CoverNetworkAccessManager(parent);
    auto* cache = new QNetworkDiskCache(manager);
    cache->setCacheDirectory(m_cacheDirectory);
    cache->setMaximumCacheSize(256LL * 1024 * 1024);
    manager->setCache(cache);
    return manager;
}
