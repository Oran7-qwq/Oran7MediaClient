#include "BilibliLiveRoomAddressCatch.h"
#include "GlobalHelper.h"
#include "BilibiliAuthManager.h"

QVariant BilibiliRoomAddressCatch::getRoomInfo(QVariant room_id)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url("https://api.live.bilibili.com/xlive/web-room/v2/index/getRoomPlayInfo");

    QUrlQuery query;
    query.addQueryItem("room_id", QString::number(room_id.toInt()));
    query.addQueryItem("protocol", "0,1");     // 0:http_stream, 1:http_hls
    query.addQueryItem("format", "0,1,2");     // 0:flv, 1:ts, 2:fmp4
    query.addQueryItem("codec", "0,1");        // 0:avc, 1:hevc
    query.addQueryItem("qn", "10000");         // 视频质量
    query.addQueryItem("platform", "web");  // 平台
    query.addQueryItem("ptype", "8");            // 播放类型，通常是8
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    request.setRawHeader("Referer", "https://live.bilibili.com");
    request.setRawHeader("Origin", "https://live.bilibili.com");

    // 注入B站认证Cookie，未登录时只能获取qn=250（高清限免）
    QString cookies = BilibiliAuthManager::instance().cookieString();
    if (!cookies.isEmpty()) {
        request.setRawHeader("Cookie", cookies.toUtf8());
        NETWORK_LOG << "Using authenticated session for stream request";
    } else {
        NETWORK_LOG << "No Bilibili auth cookies - will get limited quality (qn=250)";
    }

    NETWORK_LOG << QString("Request URL:") << (url.toString()).toStdString();

    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, [reply, this]() {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray data = reply->readAll();
            NETWORK_LOG << QString("Origin Response:") << data.left(500).toStdString() << "...";

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject root = doc.object();
                int code = root["code"].toInt();
                QString message = root["message"].toString();
                NETWORK_LOG << QString("RETURN CODE:") << code;
                NETWORK_LOG << QString("MESSAGE:") << message.toStdString();

                if (code == 0)
                {
                    QJsonObject data_obj = root["data"].toObject();
                    QString title = data_obj["title"].toString();
                    int live_status = data_obj["live_status"].toInt();
                    NETWORK_LOG << QString("Room Title:") << title.toStdString();
                    NETWORK_LOG << QString("Living Statue:") << live_status << QString("(1=Living, 0=NotLiving, 2=Changed)");

                    if (live_status == 1)
                    {
                         getAvailableStreams(data_obj);
                    }
                    else
                    {
                        NETWORK_LOG << QString("ATTENTION: Live is not being");
                        emit urlsError();
                    }
                }
                else
                {
                    NETWORK_LOG << QString("API ERROR: Code") << code << "-" << message.toStdString();
                    emit urlsError();
                }
            }
            else
            {
                NETWORK_LOG << QString("ERROR: Cant not analyse JSON response");
                emit urlsError();
            }
        }
        else
        {
            NETWORK_LOG << QString("NectWork ERROR:") << reply->errorString().toStdString();
            emit urlsError();
        }
        reply->deleteLater();
    });
    return QVariant();
}

/**
 * @brief 流候选结构体，用于智能选择最优流
 */
struct StreamCandidate {
    QString url;
    QString formatName;     ///< "flv", "ts", "fmp4"
    QString codecName;      ///< "avc", "hevc"
    QString protocolName;   ///< "http_stream", "http_hls"
};

/**
 * @brief 计算流候选的优先级分数（越高越好）
 *
 * 评分权重设计原则：
 * - 编码（HEVC vs AVC）是最重要的画质决定因素，权重最高（200 vs 100）
 * - 格式和协议只影响延迟和兼容性，权重较低
 * - 确保最差的 HEVC 流（HEVC+FLV+HLS=210）也优于最好的 AVC 流（AVC+fMP4+HTTP=140）
 */
static int streamScore(const StreamCandidate& s)
{
    int score = 0;

    // 编码优先级：HEVC >> AVC（同码率下HEVC清晰度提升30-50%，这是最关键的画质因素）
    if (s.codecName == "hevc") score += 200;
    else if (s.codecName == "avc") score += 100;

    // 格式优先级：fMP4 > TS > FLV
    if (s.formatName == "fmp4") score += 30;
    else if (s.formatName == "ts") score += 20;
    else if (s.formatName == "flv") score += 10;

    // 协议优先级：HTTP-Stream延迟更低，HLS延迟较高但画质相同
    if (s.protocolName == "http_stream") score += 10;

    return score;
}

/**
 * @brief getAvailableStreams  重写：支持多格式多编码智能流选择
 * @param data_obj
 *
 * 改进点：
 * 1. 支持 FLV / TS / fMP4 三种格式（之前只取FLV）
 * 2. 支持 AVC / HEVC 两种编码
 * 3. 自动过滤 minihevc 低码率流（~900kbps）
 * 4. 按优先级评分排序，urls[0] 为最优流
 */
QVariant BilibiliRoomAddressCatch::getAvailableStreams(const QJsonObject &data_obj)
{
    if (!data_obj.contains("playurl_info"))
    {
        NETWORK_LOG << "ERROR: no playurl_info key";
        return QVariant();
    }

    QJsonObject playurl_info = data_obj["playurl_info"].toObject();
    if (!playurl_info.contains("playurl"))
    {
        NETWORK_LOG << "ERROR: no playurl key";
        return QVariant();
    }

    QJsonObject playurl = playurl_info["playurl"].toObject();
    if (!playurl.contains("stream"))
    {
        NETWORK_LOG << "ERROR: no stream key";
        return QVariant();
    }

    QJsonArray streams = playurl["stream"].toArray();
    NETWORK_LOG << "\n=== Available Streams (Enhanced Selection) ===";

    QList<StreamCandidate> candidates;

    for (int i = 0; i < streams.size(); ++i)
    {
        QJsonObject stream = streams[i].toObject();

        // 获取协议名称
        QString protocol_name;
        if (stream.contains("stream_info"))
        {
            QJsonObject stream_info = stream["stream_info"].toObject();
            protocol_name = stream_info["protocol_name"].toString();
            NETWORK_LOG << "\nSTREAM" << (i+1) << "PROTOCOL:" << protocol_name.toStdString();
        }

        if (!stream.contains("format"))
            continue;

        QJsonArray formats = stream["format"].toArray();

        for (int j = 0; j < formats.size(); ++j)
        {
            QJsonObject format = formats[j].toObject();
            QString format_name = format["format_name"].toString();
            NETWORK_LOG << "  FORMAT:" << format_name.toStdString();

            if (!format.contains("codec"))
                continue;

            QJsonArray codecs = format["codec"].toArray();

            for (int k = 0; k < codecs.size(); ++k)
            {
                QJsonObject codec = codecs[k].toObject();
                QString codec_name = codec["codec_name"].toString();
                QString base_url = codec["base_url"].toString();
                NETWORK_LOG << "    CODEC:" << codec_name.toStdString();

                // 过滤 minihevc 低码率流（检查 base_url 是否包含 "mini"）
                bool isMiniStream = base_url.contains("mini", Qt::CaseInsensitive);
                if (isMiniStream)
                {
                    NETWORK_LOG << "    [SKIP] minihevc low-bitrate stream (" << base_url.toStdString() << ")";
                    continue;
                }

                if (!codec.contains("url_info"))
                    continue;

                QJsonArray url_infos = codec["url_info"].toArray();

                // 每个codec只取第一个CDN节点（通常是延迟最低的）
                if (url_infos.size() > 0)
                {
                    QJsonObject url_info = url_infos[0].toObject();
                    QString host = url_info["host"].toString();
                    QString extra = url_info["extra"].toString();
                    QString full_url = host + base_url + extra;

                    StreamCandidate candidate;
                    candidate.url = full_url;
                    candidate.formatName = format_name;
                    candidate.codecName = codec_name;
                    candidate.protocolName = protocol_name;

                    candidates.append(candidate);

                    NETWORK_LOG << "    CANDIDATE: codec=" << codec_name.toStdString()
                                << " format=" << format_name.toStdString()
                                << " score=" << streamScore(candidate);
                }
            }
        }
    }

    if (candidates.isEmpty())
    {
        NETWORK_LOG << "WARNING: No stream candidates found!";
        emit urlsError();
        return QVariant();
    }

    // 按优先级评分排序（降序）
    std::sort(candidates.begin(), candidates.end(),
        [](const StreamCandidate& a, const StreamCandidate& b) {
            return streamScore(a) > streamScore(b);
        });

    // 清空并重新填充 URL 列表
    m_avliStrAdr.clear();
    emit avliStrAdrChanged();

    NETWORK_LOG << "\n=== Sorted Streams (Best First) ===";
    for (int i = 0; i < candidates.size(); ++i)
    {
        const StreamCandidate& c = candidates[i];
        NETWORK_LOG << "  #" << (i+1)
                    << " codec=" << c.codecName.toStdString()
                    << " format=" << c.formatName.toStdString()
                    << " score=" << streamScore(c);
        m_avliStrAdr.append(c.url);
    }

    // 记录选中的编码和格式
    m_selectedCodecName = candidates.first().codecName;
    m_selectedFormatName = candidates.first().formatName;
    NETWORK_LOG << "\n  >> BEST STREAM: codec=" << m_selectedCodecName.toStdString()
                << " format=" << m_selectedFormatName.toStdString();

    emit avliStrAdrChanged();
    emit urlsReady();

    return QVariant();
}
