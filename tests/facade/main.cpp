#include "Oran7ConfigBinding.h"
#include "Oran7AppConfigManager.h"
#include "Oran7GlobalHelper.h"
#include "Oran7MediaInfoReader.h"
#include "Oran7TrackMetadataService.h"
#include "Oran7LegacyServerClient.h"
#include "NcmSongApi.h"
#include "CurlHttpClient.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTcpServer>
#include <QSqlQuery>
#include <QSqlError>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QThread>
#include <QTimer>
#include <QFileInfo>
#include <cstdio>
#include <cstdlib>
#include <cstring>

CurlHttpClient::CurlHttpClient(QObject* parent, const QString&, const QString&) : QObject(parent) {}
CurlHttpClient::~CurlHttpClient() = default;
void CurlHttpClient::send(const NcmRequest&) {}
QMap<QString, QVariant> Oran7MediaInfoReader::analyzeMediaFileInfo(QString) { return {{"success", -1}}; }
static int checks = 0;
static void check(bool ok, const char* message) {
    if (!ok) { std::fprintf(stderr,"FAIL: %s\n", message); std::exit(1); }
    ++checks;
}
static void wait(int milliseconds) { QEventLoop loop; QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit); loop.exec(); }
template<class Predicate> static void until(Predicate predicate, const char* description) {
    QElapsedTimer timer; timer.start();
    while (!predicate() && timer.elapsed() < 3000) { QCoreApplication::processEvents(); QThread::msleep(1); }
    check(predicate(), description);
}
class Oran7Reply : public QNetworkReply {
public:
    QByteArray body;
    qint64 offset = 0;
    bool aborted = false;
    Oran7Reply(const QNetworkRequest& request, QObject* parent) : QNetworkReply(parent) { setRequest(request); setUrl(request.url()); open(ReadOnly); }
    void abort() override { aborted = true; setError(OperationCanceledError, "cancelled"); setFinished(true); emit finished(); }
    void finish(QByteArray data, bool failed = false) { body = data; if (failed) setError(ContentNotFoundError,"fixture"); setFinished(true); emit readyRead(); emit finished(); }
    qint64 bytesAvailable() const override { return body.size() - offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char* target, qint64 maximum) override {
        const qint64 size = qMin(maximum, qint64(body.size()) - offset);
        if (size <= 0) return -1;
        std::memcpy(target, body.constData() + offset, size_t(size)); offset += size; return size;
    }
};
class Oran7Manager : public QNetworkAccessManager {
public:
    QList<QPointer<Oran7Reply>> replies;
protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override {
        auto* reply = new Oran7Reply(request, this); replies.append(reply); return reply;
    }
};
static QByteArray detail(qint64 id, const QString& album, int milliseconds) {
    return QJsonDocument(QJsonObject{{"songs", QJsonArray{QJsonObject{{"id", id}, {"album", QJsonObject{{"name", album}}}, {"duration", milliseconds}}}}}).toJson();
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext&, const QString&) {});
    QTemporaryDir directory;
    check(directory.isValid(), "测试目录可用");
    fixtureRoot = directory.path();
    Oran7AppConfigManager storage;
    int probes = 0;
    Oran7ConfigBinding config(storage, [&](QString path) -> QMap<QString,QVariant> {
        ++probes;
        if (path.endsWith("bad")) return {{"success", -1}};
        return {{"success", 0}, {"title", "本地歌"}, {"artist", "歌手"}, {"album", "专辑"}, {"cover", directory.path()+"/封面 #1.jpg"}, {"duration", "123"}};
    });
    check(!config.restoredTrack(), "空配置不产生伪造曲目");
    Oran7Track track{"https://example.test/1", "标题", "歌手", "专辑", "封面", "123", 12345678901LL, true};
    config.rememberTrack(track);
    config.rememberMusicSource(track.path, true);
    const auto restored = config.restoredTrack();
    check(restored && restored->songId == track.songId && restored->title == track.title && restored->isVip && probes == 0, "网络恢复值/64位ID/VIP 且不探测文件");
    storage.values["last_used_file"] = "old-local.mp3";
    check(config.restoredTrack()->path.isEmpty(), "网络元数据和旧本地路径错配时清空路径");
    storage.values["last_media_songid"] = 0;
    storage.values["last_used_file"] = "https://cdn.test/old";
    check(config.restoredTrack()->path == "https://cdn.test/old", "无ID旧网络配置仍可恢复");
    config.rememberMusicSource("local.mp3", false);
    check(storage.values.value("last_media_songid").toLongLong() == 0, "播放本地清理持久化网络身份");
    auto local = config.restoredTrack();
    check(local && local->title == "本地歌" && local->songId == -1 && local->cover.contains("%20"), "本地恢复元数据和封面 URL 编码兼容");
    check(!config.localTrack("bad"), "探测失败不覆盖原曲目");
    config.setFocusedMusicSource("relative.mp3");
    config.saveLastFocused(false);
    check(QFileInfo(storage.getLastUsedFile()).isAbsolute(), "本地聚焦路径按原规则转为绝对路径");
    config.rememberTrack(track);
    auto obsolete = track; obsolete.songId = 9; obsolete.album = "错误专辑";
    config.completePersistedDetails(obsolete);
    check(storage.values.value("last_media_album") == track.album, "旧ID不能覆盖当前持久化详情");
    track.album = "补全专辑"; track.timesize = "456";
    config.completePersistedDetails(track);
    check(storage.values.value("last_media_album") == track.album && storage.values.value("last_media_timesize") == "456", "详情补全使用原持久化键");
    config.saveLocalOrder({"b", "a"}); check(config.localOrder() == QVariantList({"b", "a"}), "曲库顺序按原配置接口保存");
    config.saveLocalOrder({}); check(config.localOrder().isEmpty(), "空顺序也保存");
    config.saveIntegratedMusic({QVariantMap{{"id", "song"}}});
    check(config.integratedMusic().size() == 1 && storage.loads == 0, "整合列表使用内存配置，读写不重复加载磁盘");
    config.saveVolume(87); check(config.volume() == 87, "音量仅映射值，不持有副本");
    // 不创建窗口；实际 QML QObject 根对象验证几何配置映射。
    QQmlApplicationEngine engine;
    storage.values["window.width"] = 900; storage.values["window.height"] = 600;
    storage.values["window.position.x"] = 32; storage.values["window.position.y"] = 48;
    engine.loadData("import QtQml\nQtObject { objectName: \"__Oran7Window__\"; property int width: 0; property int height: 0; property int x: 0; property int y: 0 }", QUrl("qrc:/GeometryFixture.qml"));
    check(!engine.rootObjects().isEmpty(), "无窗口 QML 根对象可用");
    config.loadWindowSize(engine); config.loadWindowPosition(engine);
    check(engine.rootObjects().first()->property("width") == 900 && engine.rootObjects().first()->property("y") == 48, "窗口几何读取与原属性名兼容");
    {
        auto manager = std::make_unique<Oran7Manager>(); auto* transport = manager.get();
        Oran7TrackMetadataService service(std::move(manager));
        Oran7PlaybackState state;
        state.setCurrentTrack({"", "", "", "", "", "", 11, true});
        int delivered = 0;
        QObject::connect(&service, &Oran7TrackMetadataService::detailsReady, [&](qint64 id, quint64 revision, const QString& album, const QString& duration) {
            if (state.currentTrack().songId == id && state.completeTrackDetails(revision, album, duration)) ++delivered;
        });
        service.fetch(11, state.trackRevision());
        auto first = transport->replies.last();
        check(first->request().transferTimeout() == 15000 && first->request().rawHeader("Referer") == "https://music.163.com", "保留详情请求超时与请求头");
        first->finish(detail(11, "第一专辑", 123456));
        check(delivered == 1 && state.currentTrack().timesize == "123", "详情值只填空字段并按秒返回");
        service.fetch(11, state.trackRevision()); auto stale = transport->replies.last();
        service.fetch(22, state.trackRevision());
        check(stale->aborted, "新请求取消旧请求，同步 abort 不串回调");
        stale->finish(detail(11, "迟到", 9));
        check(delivered == 1, "被取消回复不能补全详情");
        auto current = transport->replies.last();
        current->finish(detail(22, "不是当前歌", 100));
        check(delivered == 1, "接收方核对当前歌曲身份");
        state.setCurrentTrack({"", "", "", "", "", "", 11, false});
        service.fetch(11, state.trackRevision()); auto oldRevision = transport->replies.last();
        state.setCurrentTrack({"", "", "", "", "", "", 22, false});
        state.setCurrentTrack({"", "新选择", "", "", "", "", 11, false});
        oldRevision->finish(detail(11,"旧版本",50000));
        check(delivered == 1 && state.currentTrack().album.isEmpty(), "离开再返回同ID也不接受旧快照结果");
        service.fetch(11,state.trackRevision()); transport->replies.last()->finish("not json");
        check(delivered == 1, "坏 JSON 不补全");
        service.fetch(11,state.trackRevision()); transport->replies.last()->finish(detail(999,"错歌",100));
        check(delivered == 1, "响应中无目标ID不补全");
        service.fetch(11,state.trackRevision()); transport->replies.last()->finish(detail(11,"",0));
        check(delivered == 1, "空详情不发成功");
        service.fetch(11,state.trackRevision()); transport->replies.last()->finish({},true);
        check(delivered == 1, "网络失败不改状态");
        service.fetch(11,state.trackRevision()); service.cancel();
        check(transport->replies.last()->aborted, "退出取消在途请求");
    }
    {
        QTcpServer server; check(server.listen(QHostAddress::LocalHost,0), "本机测试服务器启动");
        const QString path=directory.path()+"/legacy.db";
        { auto db=QSqlDatabase::addDatabase("QSQLITE","fixture_setup"); db.setDatabaseName(path); check(db.open(),"临时SQLite打开"); QSqlQuery query(db); check(query.exec("CREATE TABLE user_favorite(music_id INTEGER PRIMARY KEY,music_name TEXT,music_artist TEXT,music_album TEXT,music_timesize TEXT,music_file_path TEXT,music_icon_path TEXT,add_date TEXT,show_order TEXT)"),"临时旧表结构就绪"); db.close(); }
        QSqlDatabase::removeDatabase("fixture_setup");
        const auto initialConnections=QSqlDatabase::connectionNames();
        {
            // 默认停用不会触碰 Socket/数据库；只有本测试显式启用旧协议。
            {
                Oran7LegacyServerClient disabled("127.0.0.1",server.serverPort(),directory.path()+"/must-not-exist.db");
                disabled.validateLogin("12345678901","abc_1234");
                wait(20);
                check(!disabled.enabled() && !server.hasPendingConnections()
                    && !QFileInfo::exists(directory.path()+"/must-not-exist.db")
                    && QSqlDatabase::connectionNames()==initialConnections,"默认停用无网络/数据库副作用且旧入口不会激活");
            }
            Oran7LegacyServerClient legacy("127.0.0.1",server.serverPort(),path,true);
            int code=-1, songs=0; QString loggedName;
            QObject::connect(&legacy,&Oran7LegacyServerClient::loginInputFormatValid,[&](int value){code=value;});
            QObject::connect(&legacy,&Oran7LegacyServerClient::loginSuccess,[&](const QString& value){loggedName=value;});
            QObject::connect(&legacy,&Oran7LegacyServerClient::integratedMusicElementReady,[&](const QString&,const QString& title,const QString&,const QString&,const QString&,const QString& id){if(title=="Song" && id=="77")++songs;});
            until([&]{return server.hasPendingConnections();},"客户端保留原自动连接行为");
            std::unique_ptr<QTcpSocket> peer(server.nextPendingConnection());
            until([&]{return legacy.findChild<QTcpSocket*>()->state()==QAbstractSocket::ConnectedState;},"仅测试显式启用的旧会话已完成连接");
            legacy.validateLogin("short","abcdefgh"); check(code==1,"原账号长度校验保留");
            legacy.validateLogin("12345678901","bad!"); check(code==2,"原密码校验保留");
            legacy.validateLogin("12345678901","abc_1234");
            until([&]{return peer->bytesAvailable()>0;},"有效登录按原协议发包");
            const auto login=peer->readAll(); check(code==0 && quint8(login[4])==1 && login.mid(7)=="12345678901abc_1234","登录字段顺序和编码保留");
            QByteArray response; QDataStream out(&response,QIODevice::WriteOnly); out.setByteOrder(QDataStream::BigEndian);
            out<<quint32(22)<<quint8(1)<<quint8(0)<<quint32(0x12F080AA)<<quint8(11); response.append("12345678901");
            peer->write(response.left(3)); peer->flush(); wait(10); check(loggedName.isEmpty(),"半包不提前解析");
            peer->write(response.mid(3)); peer->flush();
            until([&]{return !loggedName.isEmpty();},"完整登录回应转发成功");
            until([&]{return peer->bytesAvailable()>=9;},"成功登录继续请求旧收藏数据");
            const auto query=peer->readAll(); check(loggedName=="123--8901" && query.mid(5,4)==QByteArray::fromHex("12F080AA"),"UID高位字节与掩码用户名正确");
            QByteArray payload; QDataStream song(&payload,QIODevice::WriteOnly); song.setByteOrder(QDataStream::BigEndian);
            for(const QByteArray value:{QByteArray("Song"),QByteArray("Artist"),QByteArray("Album")}){song<<quint16(value.size());song.writeRawData(value.constData(),value.size());}
            song<<quint32(0)<<quint32(123)<<quint32(77);
            QByteArray favorites; QDataStream list(&favorites,QIODevice::WriteOnly); list.setByteOrder(QDataStream::BigEndian); list<<quint32(13+payload.size())<<quint8(2)<<quint32(1)<<quint32(4+payload.size()); favorites.append(payload);
            peer->write(favorites); peer->flush(); until([&]{return songs==1;},"旧收藏解析写SQLite并转发原信号");
            peer->write(QByteArray::fromHex("0000000001")); peer->flush(); wait(10);
            peer->write(favorites); peer->flush(); until([&]{return songs==2;},"无效长度拒绝后仍可处理正常帧，重复曲目不重复插入");
        }
        check(QSqlDatabase::connectionNames()==initialConnections,"会话析构释放SQL连接名");
        { auto db=QSqlDatabase::addDatabase("QSQLITE","fixture_verify");db.setDatabaseName(path);check(db.open(),"验证临时数据库");QSqlQuery query(db);query.exec("SELECT COUNT(*) FROM user_favorite");check(query.next()&&query.value(0).toInt()==1,"旧缓存去重保持");db.close(); }
        QSqlDatabase::removeDatabase("fixture_verify");
    }
    std::printf("PASS: %d facade closing checks\n",checks);
}
