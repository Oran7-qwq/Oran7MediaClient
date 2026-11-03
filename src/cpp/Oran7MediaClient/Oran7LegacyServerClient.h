#pragma once
#include <QObject>
#include <QTcpSocket>
#include <QSqlDatabase>
#include <QMap>
#include <functional>

// 陈旧、默认未启用的 TCP/SQLite 会话。保留至 v1.0 之后评估官方服务端时再启用。
class Oran7LegacyServerClient : public QObject
{
    Q_OBJECT
public:
    using PacketHandler = std::function<void(quint32, quint8, const QByteArray&)>;
    Oran7LegacyServerClient(const QString& host, quint16 port,
                           const QString& databasePath = QStringLiteral("SQLite/userLocal.db"), bool enabled = false);
    ~Oran7LegacyServerClient() override;
    bool enabled() const { return Client_socket != nullptr; }
    void validateLogin(QString username, QString password);
    void registerHandler_(quint8 opcode, PacketHandler handler);
    void loginResponse(quint32 totalLength, quint8 opcode, const QByteArray& packet);
    void loadingSeverUserDataQuery();
    void explainSeverUserData(quint32 totalLength, quint8 opcode, const QByteArray& packet);
signals:
    void loginSuccess(QString username);
    void loginInputFormatValid(int code);
    void integratedMusicElementReady(QString icon, QString title, QString artist, QString album, QString timesize, QString musicId);
private:
    enum ProtocolHandlerCMD_ { Login_ = 0x01, LoadingData_ = 0x02 };
    void onConnected();
    void onDisconnected();
    void onReadReady();
    QTcpSocket* Client_socket = nullptr; // 仅显式启用时创建；QObject 子对象自动释放。
    QMap<quint8, PacketHandler> Client_handler;
    QByteArray responseFromServer;
    quint32 UID = 0;
    QString m_connectionName;
    QSqlDatabase localdb;
};
