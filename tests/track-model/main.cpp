#include "Oran7TrackLibrary.h"
#include "Oran7PlaybackState.h"
#include <QAbstractItemModelTester>
#include <QGuiApplication>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QSignalSpy>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlExpression>
#include <QFile>
#include <QUrl>
#include <QElapsedTimer>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool condition, const char* text) {
    ++checks;
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1); }
}
static QVariantMap song(qint64 id, QString name = "歌曲", QString path = {}, QString url = {}) {
    return {{"songId",id},{"title",name},{"artist","歌手"},{"album","专辑"},{"cover","cover.png"},
        {"timesize","123.5"},{"path",path},{"url",url},{"source",id > 0 ? "netease" : "local"}};
}
static QVariant evaluate(QQmlEngine& engine, const QString& code) {
    QQmlExpression expression(engine.rootContext(),nullptr,code);
    auto result = expression.evaluate();
    if (expression.hasError()) qWarning() << expression.error();
    check(!expression.hasError(), "QML 模型读写命令无异常");
    return result;
}
int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");
    QGuiApplication app(argc,argv);
    auto library = std::make_unique<Oran7TrackLibrary>();
    QQmlEngine::setObjectOwnership(library.get(), QQmlEngine::CppOwnership);
    auto* local = library->local(); auto* integrated = library->integrated();
    auto* liked = library->liked(); auto* search = library->search();
    QAbstractItemModelTester t1(local,QAbstractItemModelTester::FailureReportingMode::Fatal);
    QAbstractItemModelTester t2(integrated,QAbstractItemModelTester::FailureReportingMode::Fatal);
    QAbstractItemModelTester t3(liked,QAbstractItemModelTester::FailureReportingMode::Fatal);
    QAbstractItemModelTester t4(search,QAbstractItemModelTester::FailureReportingMode::Fatal);
    for (auto* model : {local,integrated,liked,search}) {
        check(!model->parent(),"模型不叠加 QObject parent 所有权");
        check(QQmlEngine::objectOwnership(model)==QQmlEngine::CppOwnership,"模型只由 C++ 持有");
        check(model->rowCount()==0 && !model->data(QModelIndex(),Qt::DisplayRole).isValid(),"空模型与无效索引");
    }
    Oran7TrackListModel precise;
    precise.replaceTracks({QVariantMap{{"songId",100},{"durationMs",123456},{"timesize","123"}}});
    check(precise.get(0).value("durationMs")==123456 && precise.get(0).value("timesize")=="123", "标准毫秒角色不被旧秒数字符串截断");
    const QString path = "C:/音乐/歌曲.mp3";
    QSignalSpy reset(local,&QAbstractItemModel::modelReset);
    library->replaceLocalTracks({song(0,"本地 A",path),song(0,"重复",QUrl::fromLocalFile(path.toUpper()).toString()),
        song(0,"本地 B","C:/音乐/b.mp3"), QVariantMap{{"title","没有身份"}}});
    check(local->count()==2 && reset.count()==1,"单快照发布，文件 URL/大小写去重，无效行过滤");
    auto row=local->get(0);
    check(row.value("title")=="本地 A" && row.value("music_name")==row.value("title"),"标准字段与旧界面别名同值");
    check(row.value("path")==path && row.value("filepath")==path && row.value("durationMs")==123500,"路径/毫秒时长一致");
    check(row.value("cover")==row.value("icon") && row.value("isLiked")==row.value("isLoved"),"封面与喜欢别名一致");
    check(!local->setData(local->index(0),"bad",Oran7TrackListModel::TitleRole),"元数据禁止通用 setData 修改");
    check(!(local->flags(local->index(0)) & Qt::ItemIsEditable),"角色只读");
    check(local->get(-1).isEmpty() && local->get(2).isEmpty(),"越界查询安全");
    check(!local->setSelected(-1,true) && !local->moveTrack(0,4),"越界命令拒绝");
    check(local->setSelected(0,true),"多选命令");
    auto stable=QPersistentModelIndex(local->index(0));
    const auto identity=row.value("trackId").toString();
    check(local->moveTrack(0,1),"向后拖拽成功");
    check(stable.isValid() && stable.row()==1 && stable.data(Oran7TrackListModel::TitleRole)=="本地 A","持久索引跟随拖拽");
    check(local->get(1).value("isSelected").toBool() && local->indexOfTrack(identity)==1,"选择与身份跟随拖拽");
    check(local->moveTrack(1,0) && stable.row()==0,"向前拖拽成功");
    library->replaceLocalTracks({song(0,"插入新行","C:/音乐/new.mp3"),song(0,"本地 A 更新",path),song(0,"本地 B","C:/音乐/b.mp3")});
    check(local->get(1).value("isSelected").toBool(),"重建快照按身份保留多选");
    check(local->indexOfTrack(identity)==1 && !stable.isValid(),"模型重建使 Qt 旧索引失效，身份仍可定位");
    QSignalSpy persisted(library.get(),&Oran7TrackLibrary::integratedMusicChanged);
    library->restoreIntegratedTracks({song(1,"已整合","","https://example/1")});
    check(integrated->count()==1 && persisted.isEmpty(),"启动恢复不反写配置");
    auto vip=song(2,"VIP","https://expired/a","https://expired/b");vip["isVip"]=true;
    QSignalSpy inserted(integrated,&QAbstractItemModel::rowsInserted);
    library->addIntegratedTracks({vip,song(3,"新曲目","","https://example/3"),vip,song(1)});
    check(integrated->count()==3 && inserted.count()==1 && persisted.count()==1,"批量整合单次通知/持久化，跨批去重");
    check(integrated->get(0).value("songId")==2 && integrated->get(1).value("songId")==3,"批量顶部插入保持输入顺序");
    check(integrated->get(0).value("path").toString().isEmpty() && integrated->get(0).value("url").toString().isEmpty(),"VIP 过期 URL 不入库");
    check(library->isIntegrated(2,"","") && integrated->get(0).value("isIntegrated").toBool(),"空 URL VIP 按 ID 判断成员关系");
    const int writes=persisted.count();
    integrated->setSelected(0,true);integrated->setAnimationPending(0,true);
    check(persisted.count()==writes,"选择/动画不触发配置保存");
    integrated->moveTrack(0,2);
    check(persisted.count()==writes+1 && library->integratedTracks().last().toMap().value("songId")==2,"整合拖拽后持久化新顺序");
    const auto stored=library->integratedTracks();
    check(!stored[0].toMap().contains("isSelected") && !stored[0].toMap().contains("isLiked"),"配置不保存临时界面状态");
    library->restoreIntegratedTracks(stored);
    check(library->integratedTracks()==stored,"旧配置字段与顺序可往返恢复");
    library->replaceSearchResults({song(1),song(2),song(3),song(3,"重复")});
    check(search->count()==3 && search->get(0).value("isIntegrated").toBool(),"搜索快照去重并同步整合标记");
    QSignalSpy changed(search,&QAbstractItemModel::dataChanged);
    library->addLikedTrack(song(2));
    check(library->isLiked(2) && search->get(1).value("isLiked").toBool() && search->get(1).value("isLoved").toBool(),"喜欢状态从集合同步搜索模型");
    check(!changed.isEmpty() && liked->get(0).value("isLiked").toBool(),"喜欢变化发 dataChanged，收藏行状态一致");
    library->addLikedTrack(song(2));check(liked->count()==1,"重复喜欢不增加行");
    library->replaceLikedTracks({song(1),song(3)});
    check(search->get(0).value("isLiked").toBool() && !search->get(1).value("isLiked").toBool(),"收藏整批刷新替换成员关系");
    check(library->removeLikedTrack(1) && !search->get(0).value("isLiked").toBool(),"取消喜欢同步其他模型");
    library->clearLikedTracks();check(!library->isLiked(3) && !search->get(2).value("isLiked").toBool(),"登出清空喜欢标记");
    check(library->removeIntegratedTrack(2,"","") && !library->isIntegrated(2,"","") && !search->get(1).value("isIntegrated").toBool(),"取消 VIP 整合精确匹配 ID 并同步标记");
    check(!library->removeIntegratedTrack(0,"","") && !library->removeLikedTrack(0),"空身份不误删");
    bool instant=false;
    auto connection=QObject::connect(integrated,&Oran7TrackListModel::countChanged,[&]{instant=library->isIntegrated(9,"","");});
    library->addIntegratedTrack(song(9));check(instant,"countChanged 时去重索引已可查询");QObject::disconnect(connection);
    {
        QQmlEngine engine;
        engine.rootContext()->setContextProperty("tracks",library.get());
        check(evaluate(engine,"tracks.search.count").toInt()==3,"QML 读取 C++ 模型 count");
        check(evaluate(engine,"tracks.search.get(1).title").toString()=="歌曲","QML get 返回完整字段");
        check(evaluate(engine,"typeof tracks.search.setProperty").toString()=="undefined","QML 无通用角色写入入口");
        evaluate(engine,"var snapshot = tracks.search.get(0); snapshot.title = '仅修改副本'");
        check(search->get(0).value("title")=="歌曲","修改 JS 快照不会改模型");
        evaluate(engine,"tracks.search.setSelected(1, true)");
        check(search->get(1).value("isSelected").toBool(),"QML 多选命令落到 C++");
        QQmlComponent view(&engine);
        view.setData(R"(import QtQuick
Item {
 width: 400; height: 400
 property alias list: list
 ListView {
  id: list; anchors.fill: parent; model: tracks.search
  delegate: Rectangle {
   width: 300; height: 30
   property string rowTitle: model.title
   property string legacyTitle: model.music_name
   property bool liked: model.isLiked
   property bool integrated: model.isIntegrated
   property bool selected: model.isSelected
  }
 }
 function title() { list.forceLayout(); var row = list.itemAtIndex(0); return row ? row.rowTitle : "missing" }
})",QUrl("qrc:/model_view.qml"));
        std::unique_ptr<QObject> object(view.create());
        if (!object) qWarning() << view.errors();
        check(bool(object),"真实 ListView 接受 QAbstractListModel 角色");
        QVariant title;
        QMetaObject::invokeMethod(object.get(),"title",Q_RETURN_ARG(QVariant,title));
        check(title.toString()=="歌曲","真实 delegate 读取标准角色，无 undefined");
        engine.collectGarbage();check(search->count()==3,"QML GC 不回收 C++ 模型");
        // 加载生产 BasicConfig，仅去掉应用注册模块导入；底层列表使用真实 C++ 实例。
        Oran7PlaybackState playback;
        engine.rootContext()->setContextProperty("Client",QVariantMap{
            {"tracks",QVariant::fromValue(library.get())},{"playback",QVariant::fromValue(&playback)}});
        QQmlComponent dependencies(&engine);
        dependencies.setData(R"(import QtQml
QtObject { property real likedPlaylistId: 0; property bool isLoggedIn: false; property string cookieString: ""; property string csrfToken: ""; signal loginStatusChanged(); signal keyCombinationTriggered(string combination) })",QUrl("qrc:/dependencies.qml"));
        std::unique_ptr<QObject> deps(dependencies.create());
        check(bool(deps),"网络/快捷键接收者替身创建");
        engine.rootContext()->setContextProperty("NcmPlaylistApi",deps.get());
        engine.rootContext()->setContextProperty("NcmSession",deps.get());
        engine.rootContext()->setContextProperty("globalEventFilter",deps.get());
        QFile file(QStringLiteral(APP_ROOT)+"/src/qml/Basic/BasicConfig.qml");
        check(file.open(QIODevice::ReadOnly),"读取生产 BasicConfig");
        auto basicSource=QString::fromUtf8(file.readAll());
        for(const auto& text:{"pragma Singleton","import Client 1.0","import Oran7NetEaseAuth 1.0","import Oran7Async 1.0"}) basicSource.remove(text);
        basicSource.replace("target: BasicConfig","target: root");
        QQmlComponent basicComponent(&engine);basicComponent.setData(basicSource.toUtf8(),QUrl("qrc:/BasicConfig.qml"));
        std::unique_ptr<QObject> basic(basicComponent.create());
        if(!basic) qWarning()<<basicComponent.errors();
        check(bool(basic),"生产 BasicConfig 可绑定统一模型");
        engine.rootContext()->setContextProperty("BasicConfig",basic.get());
        check(evaluate(engine,"BasicConfig.localMusicListModel === tracks.local && BasicConfig.myIntegratedMusicListModel === tracks.integrated && BasicConfig.neteaseLikedListModel === tracks.liked").toBool(),"三个全局列表直接借用 C++ 模型");
        evaluate(engine,"tracks.addLikedTrack({songId:55,cover:'cover',title:'新喜欢',artist:'歌手',album:'专辑',timesize:'90',isVip:true})");
        check(library->isLiked(55)&&liked->get(0).value("title")=="新喜欢"&&liked->get(0).value("isVip").toBool(),"真实喜欢入口保存元数据与 VIP");
        evaluate(engine,"tracks.removeLikedTrack(55)");check(!library->isLiked(55),"真实取消喜欢入口");
        evaluate(engine,"BasicConfig.batchAddIntegratedItems([{songId:77,icon:'cover',music_name:'批次',music_artist:'歌手',music_album:'专辑',timesize:'80',isVip:true,url:'https://expired',filepath:'https://expired'}])");
        check(integrated->get(0).value("title")=="批次"&&integrated->get(0).value("path").toString().isEmpty(),"真实批量整合入口映射旧字段并移除 VIP 链接");
        evaluate(engine,"BasicConfig.addIntegratedMusicItem('cover','URL 曲目','歌手','专辑','70','https://example/only-url','',0,false,'local')");
        check(library->isIntegrated(0,"https://example/only-url",""),"只有 URL 的整合可查询");
        check(evaluate(engine,"BasicConfig.removeIntegratedMusic(0,'https://example/only-url','')").toBool(),"只有 URL 的整合可删除");
        // P1-7 已将收藏请求与 playlistId 归属移入 CloudMusicService；此处只验证模型发布。
        evaluate(engine,"tracks.replaceLikedTracks([{songId:42,title:'云端快照',artist:'歌手',album:'专辑',cover:'cover',durationMs:60123,timesize:'60',isVip:false}])");
        check(liked->count()==1 && liked->get(0).value("title")=="云端快照","云端快照整批替换模型；服务归属由 policy_checks 覆盖");
        check(evaluate(engine,"(function(){try { BasicConfig.localMusicListModel=null; return false } catch(e) { return true }})()").toBool(),"QML 无法替换全局只读模型引用");

    }
    QVariantList many;for(int i=0;i<2000;++i)many.append(song(10000+i));
    QSignalSpy oneReset(search,&QAbstractItemModel::modelReset);
    library->replaceSearchResults(many);check(search->count()==2000 && oneReset.count()==1,"2000 首搜索结果一次模型重建");
    check(search->indexOfSongId(11999)==1999 && search->indexOfSongId(0)==-1 &&
        search->indexOfSongId(999)==-1 && search->indexOfSource("")==-1,
        "模型直接查询长列表尾部及缺失身份，空路径不误命中云端歌");
    check(search->moveTrack(1999,0) && search->indexOfSongId(11999)==0,
        "直接查询跟随拖拽后顺序");
    {
        QQmlEngine lookupEngine;
        lookupEngine.rootContext()->setContextProperty("lookupModel",search);
        QElapsedTimer elapsed; elapsed.start();
        check(evaluate(lookupEngine,"(function(){for(var i=0;i<lookupModel.count;++i)if(lookupModel.get(i).songId===11998)return i;return -1})()").toInt()==1999,
            "隔离旧 QML 扫描基线");
        const auto legacyMs=elapsed.elapsed();elapsed.restart();
        check(evaluate(lookupEngine,"lookupModel.indexOfSongId(11998)").toInt()==1999,
            "QML 单次调用直接查找等价身份");
        std::printf("LOOKUP: 2000 tracks, legacy QML=%lldms, direct=%lldms\n",
            static_cast<long long>(legacyMs),static_cast<long long>(elapsed.elapsed()));
    }
    check(local->indexOfSource(path)==1 && local->indexOfSource("missing")==-1,
        "本地路径直接查询仍按当前顺序及精确路径匹配");
    QSignalSpy oneInsertion(integrated,&QAbstractItemModel::rowsInserted);
    library->addIntegratedTracks(many);check(oneInsertion.count()==1,"2000 首整合一次范围插入");
    library->clearSearchResults();check(search->count()==0,"清空搜索结果");
    QPointer<Oran7TrackListModel> watched(search);library.reset();check(!watched,"集合独占销毁所有模型");
    std::printf("PASS: %d track-model checks, Qt model tester, QML/ListView, membership, batch publication, order, persistence and ownership\n",checks);
}

