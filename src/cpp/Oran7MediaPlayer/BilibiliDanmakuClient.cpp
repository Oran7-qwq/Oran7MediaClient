#include "BilibiliDanmakuClient.h"
#include "Oran7GlobalHelper.h"
#include "Oran7BilibiliAuthManager.h"

#include <zlib.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

// 使用项目内置的 brotli 解码器（3rdparty/brotli，官方 v1.1.0 解码器）
#include <brotli/decode.h>

namespace {

// 解压 brotli 流（B站弹幕 protover=3，web 客户端用它能拿到满速弹幕；zlib 的 protover=2 会被抽样）
// 流式解码：brotli 不内嵌解压后长度，循环 BrotliDecoderDecompressStream 边解边收
QByteArray brotliInflate(const QByteArray &data)
{
    if (data.isEmpty()) return {};

    BrotliDecoderState *state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
    if (!state) return {};

    QByteArray out;
    const uint8_t *nextIn = reinterpret_cast<const uint8_t *>(data.constData());
    size_t availIn = size_t(data.size());
    uint8_t buf[16384];

    while (true) {
        uint8_t *nextOut = buf;
        size_t availOut = sizeof(buf);
        const BrotliDecoderResult r = BrotliDecoderDecompressStream(
            state, &availIn, &nextIn, &availOut, &nextOut, nullptr);
        out.append(reinterpret_cast<char *>(buf), sizeof(buf) - availOut);
        if (r == BROTLI_DECODER_RESULT_SUCCESS || r == BROTLI_DECODER_RESULT_ERROR) {
            BrotliDecoderDestroyInstance(state);
            return (r == BROTLI_DECODER_RESULT_SUCCESS) ? out : QByteArray();
        }
        // BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT → 继续读下一块
    }
}

// WBI 签名用 mixin key 置换表（固定，来自 B 站前端）
static const unsigned char kWbiTab[64] = {
    46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35,
    27, 43, 5, 49, 33, 9, 42, 19, 29, 28, 14, 39, 12, 38, 41, 13,
    37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60, 51, 30, 4,
    22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52
};

QString wbiMixinKey(const QString &imgKey, const QString &subKey)
{
    const QString src = imgKey + subKey;
    QString mixin;
    mixin.reserve(32);
    for (int i = 0; i < 32; ++i)
        mixin += src.at(kWbiTab[i]);
    return mixin;
}

// 计算 getDanmuInfo 的 wts/w_rid 签名（参数按 key 字典序）
QString wbiSign(const QString &roomId, const QString &mixinKey, QByteArray *wtsOut = nullptr)
{
    const QString wts = QString::number(QDateTime::currentSecsSinceEpoch());
    // id < type < wts（字典序）
    const QString query = QString("id=%1&type=0&wts=%2").arg(roomId, wts);
    const QByteArray wrid = QCryptographicHash::hash((query + mixinKey).toUtf8(),
                                                     QCryptographicHash::Md5).toHex();
    if (wtsOut) *wtsOut = wts.toUtf8();
    return QString("%1&w_rid=%2").arg(query, QString::fromLatin1(wrid));
}

// 解压裸 zlib 流（B站弹幕 protover=2 数据）。Qt 的 qUncompress 需要 qCompress 的
// 4 字节长度前缀，无法解这种流，必须直接用 zlib。
QByteArray zlibInflate(const QByteArray &data)
{
    if (data.isEmpty()) return {};

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK)
        return {};

    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    zs.avail_in = uInt(data.size());

    QByteArray out;
    char buf[16384];
    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef *>(buf);
        zs.avail_out = uInt(sizeof(buf));
        ret = inflate(&zs, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            inflateEnd(&zs);
            return {};
        }
        out.append(buf, int(sizeof(buf)) - int(zs.avail_out));
    } while (ret != Z_STREAM_END);

    inflateEnd(&zs);
    return out;
}


// ------------------- 大端序字节读写 -------------------
quint32 readBe32(const QByteArray &d, int off)
{
    if (off + 4 > d.size()) return 0;
    const uchar *p = reinterpret_cast<const uchar *>(d.constData()) + off;
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16)
         | (quint32(p[2]) << 8) | quint32(p[3]);
}

quint16 readBe16(const QByteArray &d, int off)
{
    if (off + 2 > d.size()) return 0;
    const uchar *p = reinterpret_cast<const uchar *>(d.constData()) + off;
    return quint16((quint16(p[0]) << 8) | quint16(p[1]));
}

void appendBe16(QByteArray &d, quint16 v)
{
    d.append(char(v >> 8));
    d.append(char(v & 0xff));
}

void appendBe32(QByteArray &d, quint32 v)
{
    d.append(char(v >> 24));
    d.append(char((v >> 16) & 0xff));
    d.append(char((v >> 8) & 0xff));
    d.append(char(v & 0xff));
}

// 把 body 包装成 B 站弹幕协议帧（16 字节头 + body）
QByteArray buildPacket(quint16 protover, quint32 operation, const QByteArray &body)
{
    QByteArray pkt;
    appendBe32(pkt, 16 + body.size());
    appendBe16(pkt, 16);          // 头部长度
    appendBe16(pkt, protover);    // 协议版本：0=json, 1=json+整型心跳, 2=zlib, 3=brotli
    appendBe32(pkt, operation);   // 操作码：2=心跳 5=通知 7=认证 8=认证回包
    appendBe32(pkt, 1);           // 序号
    pkt.append(body);
    return pkt;
}

// 切分一个 body 中连续拼接的多个独立 JSON 对象（B 站偶发一个帧塞多个 JSON）
QList<QByteArray> splitJsonObjects(const QByteArray &body)
{
    QList<QByteArray> out;
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    int start = -1;
    for (int i = 0; i < body.size(); ++i) {
        const char c = body.at(i);
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') {
            if (depth == 0) start = i;
            ++depth;
        } else if (c == '}') {
            --depth;
            if (depth == 0 && start >= 0) {
                out.append(body.mid(start, i - start + 1));
                start = -1;
            }
        }
    }
    return out;
}

// 从数组路径取值（防御性：路径越界返回默认）
QVariant nestedValue(const QJsonValue &root, const QList<int> &path, const QVariant &def = QVariant())
{
    QJsonValue cur = root;
    for (int idx : path) {
        if (!cur.isArray()) return def;
        const QJsonArray arr = cur.toArray();
        if (idx < 0 || idx >= arr.size()) return def;
        cur = arr.at(idx);
    }
    return cur.toVariant();
}

} // namespace

BilibiliDanmakuClient::BilibiliDanmakuClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QWebSocket)
    , m_nam(new QNetworkAccessManager(this))
    , m_heartbeatTimer(new QTimer(this))
    , m_watchdogTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    // 必须共享同一个 cookie jar：先访问 bilibili.com 主页收集 buvid3/buvid4 等，
    // 后续 getDanmuInfo 请求带上这些 cookie 才能过风控
    m_nam->setCookieJar(new QNetworkCookieJar(m_nam));

    m_socket->setParent(this);
    connect(m_socket, &QWebSocket::connected,
            this, &BilibiliDanmakuClient::onSocketConnected);
    connect(m_socket, &QWebSocket::binaryMessageReceived,
            this, &BilibiliDanmakuClient::onBinaryMessageReceived);
    connect(m_socket, &QWebSocket::disconnected,
            this, &BilibiliDanmakuClient::onSocketDisconnected);
    connect(m_socket, &QWebSocket::errorOccurred,
            this, &BilibiliDanmakuClient::onSocketError);

    m_heartbeatTimer->setInterval(30 * 1000);
    connect(m_heartbeatTimer, &QTimer::timeout,
            this, &BilibiliDanmakuClient::sendHeartbeat);

    m_watchdogTimer->setInterval(15 * 1000);
    connect(m_watchdogTimer, &QTimer::timeout,
            this, &BilibiliDanmakuClient::watchdogTimeout);

    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &BilibiliDanmakuClient::tryReconnect);
}

void BilibiliDanmakuClient::start(const QString &roomId)
{
    if (roomId.trimmed().isEmpty()) {
        emit errorOccurred(QStringLiteral("弹幕连接失败：房间号为空"));
        return;
    }

    ++m_session;                    // 作废在途回调
    m_inputRoomId = roomId.trimmed();
    m_roomId = m_inputRoomId;
    m_active = true;
    m_reconnectAttempt = 0;
    m_token.clear();
    m_hosts.clear();
    m_hostIndex = 0;
    m_connectFailures = 0;
    m_danmuReceived = 0;
    m_danmuEmitted = 0;
    m_danmuLastReceived = 0;
    m_danmuLastEmitted = 0;
    // 登录态优先用真实 uid 拉全量弹幕；失败会回退匿名
    m_realUidFailed = false;
    m_useRealUid = !Oran7BilibiliAuthManager::instance().dedeUserId().isEmpty();
    m_heartbeatTimer->stop();
    m_watchdogTimer->stop();
    m_reconnectTimer->stop();
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();          // 断开旧连接，重连流程由 fetchDanmuInfo 接管
    setConnected(false);
    emit activeChanged(true);
    // 关键：先用 room_init 把输入的房间号解析成真实 room_id，
    // 弹幕认证/getDanmuInfo 必须用真实号，短号只会挂到空频道（viewers=1、无弹幕）
    m_wbiReady = false;
    m_useLegacyEndpoint = false;
    resolveRoomId();
}

void BilibiliDanmakuClient::resolveRoomId()
{
    const int session = m_session;
    QNetworkRequest request(QUrl("https://api.live.bilibili.com/room/v1/Room/room_init?id=" + m_inputRoomId));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    request.setRawHeader("Referer", "https://live.bilibili.com");
    const QString cookie = cookieHeaderFor(request.url());
    if (!cookie.isEmpty())
        request.setRawHeader("Cookie", cookie.toUtf8());

    NETWORK_LOG << "BilibiliDanmaku: resolving room id" << m_inputRoomId;
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, session]() {
        reply->deleteLater();
        if (session != m_session || !m_active) return;
        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull()) {
            const QJsonObject d = doc.object()["data"].toObject();
            const qint64 realId = qint64(d["room_id"].toDouble());
            if (realId > 0) {
                m_roomId = QString::number(realId);
                NETWORK_LOG << "BilibiliDanmaku: room" << m_inputRoomId << "->" << m_roomId;
            }
        }
        // 预热 cookie jar → WBI key → getDanmuInfo → 连接
        warmUp();
    });
}

void BilibiliDanmakuClient::stop()
{
    ++m_session;
    m_active = false;
    m_heartbeatTimer->stop();
    m_watchdogTimer->stop();
    m_reconnectTimer->stop();
    if (m_socket && m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->abort();
    setConnected(false);
    emit activeChanged(false);
}

void BilibiliDanmakuClient::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    emit connectionChanged(connected);
}

// 组装 Cookie 头：cookie jar 里的（buvid3/buvid4/b_nut 等）+ B站登录 Cookie
QString BilibiliDanmakuClient::cookieHeaderFor(const QUrl &url) const
{
    QStringList parts;
    if (m_nam->cookieJar()) {
        const QList<QNetworkCookie> cookies = m_nam->cookieJar()->cookiesForUrl(url);
        for (const QNetworkCookie &ck : cookies) {
            if (ck.name().isEmpty() || ck.value().isEmpty()) continue;
            parts << QString::fromLatin1(ck.name()) + "=" + QString::fromUtf8(ck.value());
        }
    }
    const QString auth = Oran7BilibiliAuthManager::instance().cookieString();
    if (!auth.isEmpty()) parts << auth;
    return parts.join("; ");
}

void BilibiliDanmakuClient::warmUp()
{
    const int session = m_session;
    QNetworkRequest request(QUrl("https://www.bilibili.com/"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    NETWORK_LOG << "BilibiliDanmaku: warming up cookie jar (bilibili.com)";
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, session]() {
        reply->deleteLater();
        if (session != m_session || !m_active) return;
        // cookie jar 已自动收集 Set-Cookie（buvid3/buvid4/b_nut/b_lsid）
        NETWORK_LOG << "BilibiliDanmaku: warm-up done, fetching wbi keys";
        fetchWbiKeys();
    });
}

void BilibiliDanmakuClient::fetchWbiKeys()
{
    const int session = m_session;
    QNetworkRequest request(QUrl("https://api.bilibili.com/x/web-interface/nav"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    request.setRawHeader("Referer", "https://live.bilibili.com");
    const QString cookie = cookieHeaderFor(QUrl("https://api.bilibili.com"));
    if (!cookie.isEmpty())
        request.setRawHeader("Cookie", cookie.toUtf8());

    NETWORK_LOG << "BilibiliDanmaku: fetching wbi keys";
    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, session]() {
        reply->deleteLater();
        if (session != m_session || !m_active) return;
        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            NETWORK_LOG << "BilibiliDanmaku: nav parse failed, proceed without wbi";
            m_wbiMixinKey.clear();
            m_wbiReady = false;
            fetchDanmuInfo();
            return;
        }
        const QJsonObject wbi = doc.object()["data"].toObject()["wbi_img"].toObject();
        const QString imgUrl = wbi["img_url"].toString();
        const QString subUrl = wbi["sub_url"].toString();
        auto baseName = [](const QString &u) {
            QString fn = QUrl(u).fileName();
            const int dot = fn.lastIndexOf('.');
            return dot > 0 ? fn.left(dot) : fn;
        };
        const QString imgKey = baseName(imgUrl);
        const QString subKey = baseName(subUrl);
        m_wbiMixinKey = wbiMixinKey(imgKey, subKey);
        m_wbiReady = !m_wbiMixinKey.isEmpty();
        NETWORK_LOG << "BilibiliDanmaku: wbi mixin ready";
        fetchDanmuInfo();
    });
}

void BilibiliDanmakuClient::fetchDanmuInfo()
{
    if (m_fetching) return;
    m_fetching = true;
    m_reconnectTimer->stop();

    const int session = m_session;

    // 新接口 getDanmuInfo（WBI 签名 + 预热 cookie 过风控）；失败回退旧接口 getConf
    QUrl url;
    if (m_useLegacyEndpoint) {
        url = QUrl(QString("https://api.live.bilibili.com/room/v1/Danmu/getConf?room_id=%1&platform=pc&player=web").arg(m_roomId));
    } else {
        const QString signedQuery = m_wbiReady
            ? wbiSign(m_roomId, m_wbiMixinKey)
            : QString("id=%1&type=0").arg(m_roomId);
        url = QUrl("https://api.live.bilibili.com/xlive/web-room/v1/index/getDanmuInfo?" + signedQuery);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/125.0.0.0 Safari/537.36");
    request.setRawHeader("Referer", "https://live.bilibili.com");
    request.setRawHeader("Origin", "https://live.bilibili.com");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    request.setRawHeader("Sec-Fetch-Dest", "empty");
    request.setRawHeader("Sec-Fetch-Mode", "cors");
    request.setRawHeader("Sec-Fetch-Site", "same-site");

    // Cookie：预热得到的 buvid3/buvid4/b_nut/b_lsid 等 + 登录 Cookie（过风控关键）
    const QString cookie = cookieHeaderFor(url);
    if (!cookie.isEmpty())
        request.setRawHeader("Cookie", cookie.toUtf8());

    NETWORK_LOG << "BilibiliDanmaku: fetching danmu info for room" << m_roomId;

    QNetworkReply *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, session]() {
        reply->deleteLater();
        m_fetching = false;
        if (session != m_session || !m_active) return;

        if (reply->error() != QNetworkReply::NoError) {
            NETWORK_LOG << "BilibiliDanmaku: danmu info network error" << reply->errorString();
            emit errorOccurred(QStringLiteral("弹幕服务器信息获取失败"));
            scheduleReconnect();
            return;
        }

        const QByteArray data = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull()) {
            emit errorOccurred(QStringLiteral("弹幕服务器信息解析失败"));
            scheduleReconnect();
            return;
        }

        const QJsonObject root = doc.object();
        const int code = root["code"].toInt();
        if (code != 0) {
            NETWORK_LOG << "BilibiliDanmaku: danmu info code" << code << root["message"].toString();
            if (code == -352 && !m_useLegacyEndpoint) {
                // 风控拦截：先回退到旧接口 getConf 再试一次
                NETWORK_LOG << "BilibiliDanmaku: risk-control(-352), falling back to legacy getConf";
                m_useLegacyEndpoint = true;
                fetchDanmuInfo();
                return;
            }
            if (code == -352) {
                // 新旧接口都被风控拦截：这是网络/IP 层面的硬拦截，自动重连无意义
                emit errorOccurred(QStringLiteral("弹幕服务器被风控拦截，请更换网络后重试"));
                return;
            }
            emit errorOccurred(QStringLiteral("弹幕服务器信息获取失败(code %1)").arg(code));
            scheduleReconnect();
            return;
        }

        const QJsonObject dataObj = root["data"].toObject();
        m_token = dataObj["token"].toString();
        // 兼容两种返回结构：新接口 host_list / 旧接口 host_server_list
        QJsonArray hosts = dataObj["host_list"].toArray();
        if (hosts.isEmpty())
            hosts = dataObj["host_server_list"].toArray();
        if (hosts.isEmpty()) {
            emit errorOccurred(QStringLiteral("弹幕服务器列表为空"));
            scheduleReconnect();
            return;
        }

        // 缓存所有主机，连接失败时轮换（不同服务器 IP 风控状态不同）
        m_hosts.clear();
        for (const QJsonValue &v : hosts) {
            const QJsonObject h = v.toObject();
            const QString host = h["host"].toString();
            const int wssPort = h["wss_port"].toInt(443);
            if (!host.isEmpty())
                m_hosts.append(qMakePair(host, wssPort));
        }
        if (m_hosts.isEmpty()) {
            emit errorOccurred(QStringLiteral("弹幕服务器列表为空"));
            scheduleReconnect();
            return;
        }
        m_hostIndex = 0;
        m_connectFailures = 0;
        openSocketAt(0);
    });
}

void BilibiliDanmakuClient::openSocketAt(int index)
{
    if (index < 0 || index >= m_hosts.size()) return;
    openSocket(m_hosts.at(index).first, quint16(m_hosts.at(index).second));
}

void BilibiliDanmakuClient::openSocket(const QString &host, quint16 wssPort)
{
    QUrl url;
    url.setScheme("wss");
    url.setHost(host);
    url.setPort(wssPort);
    url.setPath("/sub");
    NETWORK_LOG << "BilibiliDanmaku: connecting" << url.toString();
    m_socket->open(url);
}

void BilibiliDanmakuClient::onSocketConnected()
{
    NETWORK_LOG << "BilibiliDanmaku: socket connected, sending auth";
    sendAuth();
}

void BilibiliDanmakuClient::sendAuth()
{
    QJsonObject auth;
    // 登录态用真实 uid（全量弹幕）；匿名(0) 会被服务器抽样，只发极少弹幕。
    // 若真实 uid 被掐断，onSocketDisconnected 会回退匿名重连。
    if (m_useRealUid) {
        const QString uid = Oran7BilibiliAuthManager::instance().dedeUserId();
        auth["uid"] = uid.toLongLong();
    } else {
        auth["uid"] = 0;
    }
    auth["roomid"] = m_roomId.toInt();
    auth["protover"] = 3;                // 3=brotli 满速弹幕流（web 客户端同款）；2=zlib 会被抽样限流
    auth["platform"] = "web";
    auth["type"] = 2;
    auth["key"] = m_token;
    // web 客户端认证会带 buvid3，服务器用它把连接与会话绑定，缺失可能导致真实 uid 被掐
    if (m_nam->cookieJar()) {
        const QList<QNetworkCookie> cookies =
            m_nam->cookieJar()->cookiesForUrl(QUrl("https://api.bilibili.com"));
        for (const QNetworkCookie &ck : cookies) {
            if (QString::fromLatin1(ck.name()) == "buvid3") {
                auth["buvid"] = QString::fromUtf8(ck.value());
                break;
            }
        }
    }
    NETWORK_LOG << "BilibiliDanmaku: sending auth uid=" << auth["uid"].toDouble()
                << (m_useRealUid ? " (REAL)" : " (anonymous)");
    sendPacket(0, 7, QJsonDocument(auth).toJson(QJsonDocument::Compact));
}

void BilibiliDanmakuClient::sendPacket(quint16 protover, quint32 operation, const QByteArray &body)
{
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    m_socket->sendBinaryMessage(buildPacket(protover, operation, body));
}

void BilibiliDanmakuClient::sendHeartbeat()
{
    sendPacket(0, 2, QByteArray("{}"));
    // 诊断：每 30s 心跳时打印本窗口收/发弹幕数，定位"被过滤"在哪一层
    NETWORK_LOG << "BilibiliDanmaku: viewers=" << m_viewerCount
                << " 30s danmu received+=" << (m_danmuReceived - m_danmuLastReceived)
                << " emitted+=" << (m_danmuEmitted - m_danmuLastEmitted);
    m_danmuLastReceived = m_danmuReceived;
    m_danmuLastEmitted = m_danmuEmitted;
}

void BilibiliDanmakuClient::scheduleReconnect()
{
    if (!m_active) return;
    if (m_reconnectTimer->isActive()) return;
    // 温和退避：避免频繁重试触发/维持 B 站风控限流
    const int delay = qMin(3000 + m_reconnectAttempt * 7000, 60000);
    ++m_reconnectAttempt;
    NETWORK_LOG << "BilibiliDanmaku: schedule reconnect in" << delay << "ms";
    m_reconnectTimer->start(delay);
}

void BilibiliDanmakuClient::tryReconnect()
{
    if (!m_active) return;

    // 先轮换到下一个弹幕服务器（不同服务器 IP 风控状态不同，可能有的能通）
    if (m_hosts.size() > 1 && m_connectFailures < 4) {
        ++m_connectFailures;
        m_hostIndex = (m_hostIndex + 1) % m_hosts.size();
        NETWORK_LOG << "BilibiliDanmaku: reconnecting, trying host[" << m_hostIndex << "]"
                    << m_hosts.at(m_hostIndex).first;
        openSocketAt(m_hostIndex);
        return;
    }

    // 所有主机都试过一轮仍未连上 → 重新拉 token 再从第一个主机开始
    m_connectFailures = 0;
    m_hostIndex = 0;
    NETWORK_LOG << "BilibiliDanmaku: reconnecting (attempt" << m_reconnectAttempt << ")";
    fetchDanmuInfo();
}

void BilibiliDanmakuClient::onSocketDisconnected()
{
    NETWORK_LOG << "BilibiliDanmaku: disconnected";
    m_heartbeatTimer->stop();
    m_watchdogTimer->stop();
    setConnected(false);

    // 真实 uid 连接被服务器掐断(TLS close，认证未成功) → 回退匿名重连，避免一直失败
    if (!m_connected && m_useRealUid && !m_realUidFailed && !m_hosts.isEmpty()) {
        m_realUidFailed = true;
        m_useRealUid = false;
        m_hostIndex = 0;
        m_connectFailures = 0;
        NETWORK_LOG << "BilibiliDanmaku: real uid rejected, fallback to anonymous";
        openSocketAt(0);
        return;
    }

    scheduleReconnect();
}

void BilibiliDanmakuClient::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    // 具体失败路径统一走 disconnected → scheduleReconnect，这里只记录
    if (m_socket)
        NETWORK_LOG << "BilibiliDanmaku: socket error" << m_socket->errorString();
}

void BilibiliDanmakuClient::watchdogTimeout()
{
    // 长时间没有任何数据包 → 判定连接假死，主动断开触发重连
    if (!m_active || !m_connected) return;
    if (m_lastPacketMs > 0 && (QDateTime::currentMSecsSinceEpoch() - m_lastPacketMs) > 60 * 1000) {
        NETWORK_LOG << "BilibiliDanmaku: watchdog timeout, reconnecting";
        m_socket->abort();
    }
}

void BilibiliDanmakuClient::onBinaryMessageReceived(const QByteArray &message)
{
    m_lastPacketMs = QDateTime::currentMSecsSinceEpoch();
    int off = 0;
    while (off + 16 <= message.size()) {
        const quint32 totalLen = readBe32(message, off);
        if (totalLen < 16 || off + int(totalLen) > message.size())
            break;                       // 坏包/残留，终止解析
        handlePacket(message.mid(off, int(totalLen)));
        off += int(totalLen);
    }
}

void BilibiliDanmakuClient::handlePacket(const QByteArray &packet)
{
    // 帧头：0-3 总长 | 4-5 头长(16) | 6-7 协议版本 | 8-11 操作码 | 12-15 序号
    const quint16 protover = readBe16(packet, 6);
    const quint32 operation = readBe32(packet, 8);
    const QByteArray body = packet.mid(16);

    switch (operation) {
    case 8: {   // 认证回包
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        const int code = doc.object()["code"].toInt(-1);
        if (code == 0) {
            NETWORK_LOG << "BilibiliDanmaku: auth ok"
                        << (m_useRealUid ? " (REAL uid)" : " (anonymous uid=0)");
            m_reconnectAttempt = 0;
            setConnected(true);
            m_heartbeatTimer->start();
            m_watchdogTimer->start();
            sendHeartbeat();
        } else {
            NETWORK_LOG << "BilibiliDanmaku: auth failed code" << code;
            emit errorOccurred(QStringLiteral("弹幕认证失败(code %1)").arg(code));
            stop();
        }
        break;
    }
    case 3: {   // 心跳回包：4 字节大端在线人数
        if (body.size() >= 4) {
            const int viewers = int(readBe32(body, 0));
            if (viewers != m_viewerCount) {
                m_viewerCount = viewers;
                emit viewerCountChanged(viewers);
            }
        }
        break;
    }
    case 5: {   // 通知（弹幕等）
        if (protover == 2) {
            // 服务器下发 zlib 压缩包
            const QByteArray inflated = zlibInflate(body);
            if (!inflated.isEmpty()) {
                int off = 0;
                while (off + 16 <= inflated.size()) {
                    const quint32 subLen = readBe32(inflated, off);
                    if (subLen < 16 || off + int(subLen) > inflated.size()) break;
                    handlePacket(inflated.mid(off, int(subLen)));
                    off += int(subLen);
                }
            } else {
                NETWORK_LOG << "BilibiliDanmaku: zlib inflate failed, skipping packet";
            }
        } else if (protover == 3) {
            // brotli 压缩包（满速弹幕流，官方 brotli 解码器）
            const QByteArray inflated = brotliInflate(body);
            if (!inflated.isEmpty()) {
                int off = 0;
                while (off + 16 <= inflated.size()) {
                    const quint32 subLen = readBe32(inflated, off);
                    if (subLen < 16 || off + int(subLen) > inflated.size()) break;
                    handlePacket(inflated.mid(off, int(subLen)));
                    off += int(subLen);
                }
            } else {
                NETWORK_LOG << "BilibiliDanmaku: brotli inflate failed, skipping packet";
            }
        } else if (protover == 0 || protover == 1) {
            parseJsonBody(body);
        } else {
            NETWORK_LOG << "BilibiliDanmaku: unsupported op5 protover=" << protover << ", skipping";
        }
        break;
    }
    default:
        break;
    }
}

void BilibiliDanmakuClient::parseJsonBody(const QByteArray &body)
{
    const QList<QByteArray> objs = splitJsonObjects(body);
    for (const QByteArray &o : objs) {
        const QJsonDocument doc = QJsonDocument::fromJson(o);
        if (doc.isNull()) continue;
        const QJsonObject obj = doc.object();
        QString cmd = obj["cmd"].toString();
        // 去除 "DANMU_MSG:4:0:2:2:2:0" 这类子命令后缀
        const int colon = cmd.indexOf(':');
        if (colon > 0) cmd = cmd.left(colon);

        if (cmd == "DANMU_MSG") {
            ++m_danmuReceived;
            handleDanmuMsg(obj);
        }
        else if (cmd == "SUPER_CHAT_MESSAGE")
            handleSuperChat(obj);
    }
}

// 从任意 JSON 里递归找弹幕文本：优先 extra 的 JSON 字符串里的 content 字段，
// 其次任意对象的 content 字段。兼容 B 站各种 info 布局（extra 位置不固定）。
static void findContentRecursive(const QJsonValue &v, QString &out)
{
    if (!out.isEmpty()) return;
    if (v.isString()) return;
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        if (o.contains("extra") && o["extra"].isString()) {
            const QJsonObject extra = QJsonDocument::fromJson(o["extra"].toString().toUtf8()).object();
            out = extra["content"].toString();
            if (!out.isEmpty()) return;
        }
        if (o.contains("content") && o["content"].isString()) {
            const QString c = o["content"].toString();
            if (!c.isEmpty() && c != "{}" && c != "[]") { out = c; return; }
        }
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            findContentRecursive(it.value(), out);
    } else if (v.isArray()) {
        const QJsonArray a = v.toArray();
        for (const QJsonValue &el : a)
            findContentRecursive(el, out);
    }
}

void BilibiliDanmakuClient::handleDanmuMsg(const QJsonObject &obj)
{
    const QJsonValue infoV = obj["info"];
    if (!infoV.isArray()) return;
    const QJsonArray info = infoV.toArray();

    // ---- 文本：兼容多种历史布局 ----
    QString text;
    // 布局1：info[2] 直接是文本字符串
    if (info.size() > 2 && info.at(2).isString())
        text = info.at(2).toString();
    // 布局2：info[1] 是内容数组，文本在 [1][1]
    else if (info.size() > 1 && info.at(1).isArray() && info.at(1).toArray().size() > 1)
        text = info.at(1).toArray().at(1).toString();
    // 布局3：info[0][1] 是内容数组
    else if (info.size() > 0 && info.at(0).isArray() && info.at(0).toArray().size() > 1) {
        const QJsonArray outer = info.at(0).toArray();
        if (outer.at(1).isArray() && outer.at(1).toArray().size() > 1)
            text = outer.at(1).toArray().at(1).toString();
        else if (outer.at(1).isString())
            text = outer.at(1).toString();
    }
    // 布局4：info[1] 是内容数组，文本在 [1][0]
    if (text.isEmpty() && info.size() > 1 && info.at(1).isArray() && info.at(1).toArray().size() > 0)
        text = info.at(1).toArray().at(0).toString();
    // 兜底：递归全结构找 extra.content / content（extra 位置随版本变化）
    if (text.isEmpty())
        findContentRecursive(infoV, text);
    if (text.isEmpty()) return;

    // ---- 用户名 ----
    QString uname;
    if (info.size() > 1 && info.at(1).isArray() && info.at(1).toArray().size() > 1)
        uname = info.at(1).toArray().at(1).toString();
    else if (info.size() > 2 && info.at(2).isArray() && !info.at(2).toArray().isEmpty())
        uname = info.at(2).toArray().at(0).toString();
    else if (info.size() > 1 && info.at(1).isArray() && !info.at(1).toArray().isEmpty())
        uname = info.at(1).toArray().at(0).toString();

    // ---- 颜色 / 时间戳：info[0]=[0,mode,fontsize,color,ts,...] 或 info[1]=[text,mode,fontsize,color,ts,...] ----
    int color = 0xffffff;
    qint64 timestamp = 0;
    if (info.size() > 0 && info.at(0).isArray() && info.at(0).toArray().size() > 4) {
        const QJsonArray outer = info.at(0).toArray();
        color = outer.at(3).toInt(0xffffff);
        timestamp = qint64(outer.at(4).toDouble());
    }
    if (info.size() > 1 && info.at(1).isArray() && info.at(1).toArray().size() > 5) {
        const QJsonArray inner = info.at(1).toArray();
        color = inner.at(4).toInt(0xffffff);
        timestamp = qint64(inner.at(5).toDouble());
    }
    if (color == 0) color = 0xffffff;   // B 站语义：0 表示默认白色

    ++m_danmuEmitted;
    emit danmakuReceived(text, uname, color, timestamp);
}

void BilibiliDanmakuClient::handleSuperChat(const QJsonObject &obj)
{
    const QJsonObject data = obj["data"].toObject();
    const QString text = data["message"].toString();
    if (text.isEmpty()) return;

    const QString uname = data["user_info"].toObject()["uname"].toString();
    const int price = data["price"].toInt(0);

    int color = 0xff5c6a;
    const QString bg = data["background_color"].toString();
    if (bg.startsWith('#')) {
        bool ok = false;
        const uint rgb = bg.mid(1).toUInt(&ok, 16);
        if (ok) color = int(rgb);
    }

    emit superChatReceived(text, uname, price, color);
}