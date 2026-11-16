#pragma once
#include <QByteArray>
#include <QString>
class Oran7QQMusicWebProtocol
{
  public:
    static QString signature(const QByteArray& json);
    static QByteArray encrypt(const QByteArray& json);
    static QByteArray decodeResponse(QByteArray response);
};
