#pragma once
#include "NcmRequest.h"
#include <QObject>
#include <QHash>
#include <QPointer>
#include <functional>
class CurlHttpClient;

class NcmUserApi : public QObject
{
    Q_OBJECT
public:
    explicit NcmUserApi(CurlHttpClient* curl, QObject* parent = nullptr);
    void setCookieProvider(std::function<QString()> provider);
    void setCsrfProvider(std::function<QString()> provider);
    void fetchAccount(quint64 requestId = NcmRequest::nextId());
    void verifyLogin(quint64 requestId = NcmRequest::nextId());
signals:
    void accountFetched(quint64 requestId, const QString& nickname);
    void apiError(quint64 requestId, const QString& message);
    void verifyResult(quint64 requestId, int valid);
private:
    struct Oran7Query { QString tag, cookie; };
    void send(NcmRequest request);
    void finish(quint64 id, const QString& tag, int status, const QByteArray& body, const QString& error = {});
    QPointer<CurlHttpClient> m_curl;
    std::function<QString()> m_cookieProvider, m_csrfProvider;
    QHash<quint64, Oran7Query> m_pending;
};
