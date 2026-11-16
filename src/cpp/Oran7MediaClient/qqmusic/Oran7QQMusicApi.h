#pragma once
#include "Oran7QQMusicAuth.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
class QNetworkReply;
class Oran7QQMusicApi : public QObject
{
    Q_OBJECT
  public:
    struct Oran7Options
    {
        QUrl endpoint{QStringLiteral("https://u.y.qq.com/cgi-bin/musicu.fcg")};
        QUrl writeEndpoint{QStringLiteral("https://u6.y.qq.com/cgi-bin/musics.fcg")};
        QUrl lyricsEndpoint{QStringLiteral("https://c.y.qq.com/lyric/fcgi-bin/fcg_query_lyric_new.fcg")};
        int timeoutMs = 12000;
    };
    explicit Oran7QQMusicApi(QObject* parent = nullptr);
    Oran7QQMusicApi(Oran7Options options, QObject* parent = nullptr);
    ~Oran7QQMusicApi() override;
    quint64 request(const QString& module, const QString& method, const QJsonObject& parameters,
                    const Oran7QQMusicAuth& auth, bool write = false);
    quint64 lyrics(const QString& mid, const Oran7QQMusicAuth& auth);
    void cancel(quint64 request);
    void cancelAll();
  signals:
    void finished(quint64 request, const QJsonObject& data, const QString& error);

  private:
    quint64 submit(const QUrl& url, const QByteArray& body, const Oran7QQMusicAuth& auth, bool write, bool lyrics,
                   const QString& responseKey = QStringLiteral("req"));
    Oran7Options m_options;
    QNetworkAccessManager m_network;
    QHash<quint64, QPointer<QNetworkReply>> m_pending;
    quint64 m_nextRequest = 0;
};
