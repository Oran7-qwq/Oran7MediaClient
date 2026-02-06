#ifndef BILIBLILIVEROOMADDRESSCATCH_H
#define BILIBLILIVEROOMADDRESSCATCH_H

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QTimer>

class BilibiliRoomAddressCatch : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList avliStrAdr READ avliStrAdr NOTIFY avliStrAdrChanged FINAL)
public:
    explicit BilibiliRoomAddressCatch(QObject *parent = nullptr) : QObject(parent) {}

    // 获取直播信息
    Q_INVOKABLE QVariant getRoomInfo(QVariant room_id);

    QVariantList& avliStrAdr(){return m_avliStrAdr;}
signals:
    void urlsReady();
    void avliStrAdrChanged();

private:
    // 获取可用的流
    QVariant getAvailableStreams(const QJsonObject &data_obj);

    QVariantList m_avliStrAdr;
};

#endif // BILIBLILIVEROOMADDRESSCATCH_H
