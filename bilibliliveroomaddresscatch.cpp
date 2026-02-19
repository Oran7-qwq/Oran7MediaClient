#include "bilibliliveroomaddresscatch.h"
#include "globalhelper.h"

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

    NECTWORK_LOG << QString("Request URL:").toStdWString() << (url.toString()).toStdString();

    QNetworkReply *reply = manager->get(request);

    connect(reply, &QNetworkReply::finished, [reply, this]() {
        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray data = reply->readAll();
            NECTWORK_LOG << QString("Origin Response:").toStdWString() << data.left(500).toStdString() << "...";

            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isNull()) {
                QJsonObject root = doc.object();
                int code = root["code"].toInt();
                QString message = root["message"].toString();
                NECTWORK_LOG << QString("RETURN CODE:").toStdWString() << code;
                NECTWORK_LOG << QString("MESSAGE:").toStdWString() << message.toStdString();

                if (code == 0)
                {
                    QJsonObject data_obj = root["data"].toObject();
                    QString title = data_obj["title"].toString();
                    int live_status = data_obj["live_status"].toInt();
                    NECTWORK_LOG << QString("Room Title:").toStdWString() << title.toStdString();
                    NECTWORK_LOG << QString("Living Statue:").toStdWString() << live_status << QString("(1=Living, 0=NotLiving, 2=Changed)").toStdWString();

                    if (live_status == 1)
                    {
                         getAvailableStreams(data_obj);
                    }
                    else
                    {
                        NECTWORK_LOG << QString("ATTENTION: Live is not being").toStdWString();
                        emit urlsError();
                    }
                }
                else
                {
                    NECTWORK_LOG << QString("API ERROR: Code").toStdWString() << code << "-" << message.toStdString();
                    emit urlsError();
                }
            }
            else
            {
                NECTWORK_LOG << QString("ERROR: Cant not analyse JSON response").toStdWString();
                emit urlsError();
            }
        }
        else
        {
            NECTWORK_LOG << QString("NectWork ERROR:").toStdWString() << reply->errorString().toStdString();
            emit urlsError();
        }
        reply->deleteLater();
    });
    return QVariant();
}

/**
 * @brief getAvailableStreams
 * @param data_obj
 */
QVariant BilibiliRoomAddressCatch::getAvailableStreams(const QJsonObject &data_obj)
{
    if (data_obj.contains("playurl_info"))
    {
        QJsonObject playurl_info = data_obj["playurl_info"].toObject();
        if (playurl_info.contains("playurl"))
        {
            QJsonObject playurl = playurl_info["playurl"].toObject();

            if (playurl.contains("stream"))
            {
                QJsonArray streams = playurl["stream"].toArray();
                NECTWORK_LOG << QString("\n=== Avaliable Stream ===").toStdWString();

                for (int i = 0; i < streams.size(); ++i)
                {
                    QJsonObject stream = streams[i].toObject();

                    if (stream.contains("stream_info"))
                    {
                        QJsonObject stream_info = stream["stream_info"].toObject();
                        QString protocol_name = stream_info["protocol_name"].toString();
                        NECTWORK_LOG << QString("\nSTREAM").toStdWString() << (i+1) << QString("PROTICAL:").toStdWString() << protocol_name.toStdString();
                    }

                    if (stream.contains("format"))
                    {
                        QJsonArray formats = stream["format"].toArray();

                        for (int j = 0; j < formats.size(); ++j)
                        {
                            QJsonObject format = formats[j].toObject();
                            QString format_name = format["format_name"].toString();
                            NECTWORK_LOG << QString("  FORMAT:").toStdWString() << format_name.toStdString();

                            if (format_name == "flv")
                            {
                                if (format.contains("codec"))
                                {
                                    QJsonArray codecs = format["codec"].toArray();

                                    for (int k = 0; k < codecs.size(); ++k)
                                    {
                                        QJsonObject codec = codecs[k].toObject();
                                        QString codec_name = codec["codec_name"].toString();
                                        QString base_url = codec["base_url"].toString();
                                        NECTWORK_LOG << QString("    CODEC:").toStdWString() << codec_name.toStdString();
                                        NECTWORK_LOG<< QString("    BASE URL:").toStdWString() << base_url.toStdString();

                                        if (codec.contains("url_info"))
                                        {
                                            QJsonArray url_infos = codec["url_info"].toArray();

                                            for (int l = 0; l < url_infos.size(); ++l)
                                            {
                                                QJsonObject url_info = url_infos[l].toObject();
                                                QString host = url_info["host"].toString();
                                                QString extra = url_info["extra"].toString();

                                                QString full_url = host + base_url + extra;
                                                NECTWORK_LOG << QString("    LINK").toStdWString() << (l+1) << ":" << full_url.toStdString();
                                                m_avliStrAdr.append(QString(full_url));
                                                emit avliStrAdrChanged();
                                            }
                                            emit urlsReady();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                NECTWORK_LOG << QString("ERROR: no stream key").toStdWString();
            }
        }
        else
        {
            NECTWORK_LOG << QString("ERROR: no playurl key").toStdWString();
        }
    }
    else
    {
        qDebug() << QString("ERROR: no playurl_info key").toStdWString();
    }
    return QVariant();
}
