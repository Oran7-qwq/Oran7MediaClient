#include "Oran7LyricsModel.h"
#include "Oran7Api.h"
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQmlComponent>
#include <QQmlPropertyMap>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QTest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegion>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <memory>
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::fflush(stderr); std::_Exit(1); }
    ++checks;
}
static void collectLyricLabels(QQuickItem* item, QList<QQuickItem*>& labels)
{
    if (item->objectName()=="islandLyricText") labels.append(item);
    for (auto* child : item->childItems()) collectLyricLabels(child,labels);
}
static QQuickItem* findVisual(QQuickItem* item, const QString& name)
{
    if (item->objectName()==name) return item;
    for (auto* child : item->childItems()) if (auto* found=findVisual(child,name)) return found;
    return nullptr;
}
static void checkHorizontalStability(QQuickItem* island, int durationMs)
{
    QElapsedTimer elapsed; elapsed.start();
    QMap<int,qreal> widths;
    int samples=0;
    while (elapsed.elapsed()<durationMs) {
        QTest::qWait(10);
        QList<QQuickItem*> labels; collectLyricLabels(island,labels);
        for (auto* label : labels) {
            const int row=label->property("lyricLine").toInt();
            const auto center=label->mapToItem(island,QPointF(label->width()/2,label->height()/2));
            check(qAbs(center.x()-island->width()/2)<0.75,"lyric text stays horizontally centered during shell animation and scrolling");
            if (widths.contains(row)) check(qAbs(widths[row]-label->width())<0.01,"each lyric keeps a fixed layout width throughout animation");
            widths.insert(row,label->width()); ++samples;
        }
    }
    check(samples>0,"horizontal stability sampled real instantiated lyric delegates");
}
static void checkTitleAndHost(QQuickWindow* host, QQuickItem* capsule, int durationMs, bool native)
{
    auto* title=capsule->findChild<QQuickItem*>("islandIdleLabel");
    check(title,"actual title label exists");
    const auto geometry=host->geometry();
    const auto textWidth=title->width();
    QElapsedTimer elapsed; elapsed.start();
    while (elapsed.elapsed()<durationMs) {
        QTest::qWait(10);
        check(host->geometry()==geometry,"native host geometry stays fixed during title and lyric transitions");
        check(qAbs(title->width()-textWidth)<0.01,"title layout width never changes when song text changes");
        const auto titleCenter=title->mapToScene(QPointF(title->width()/2,title->height()/2));
        check(qAbs(titleCenter.x()-host->width()/2.0)<0.75,"title scene center remains fixed throughout transition");
        const auto wanted=host->property("contentMaskRect").toRectF().toAlignedRect().intersected(QRect(QPoint(),host->size()));
        check(host->mask()==QRegion(wanted),"input mask follows only capsule and shadow padding");
        if (native) {
#ifdef Q_OS_WIN
            HRGN region=CreateRectRgn(0,0,0,0);
            RECT actual{};
            const auto kind=GetWindowRgn(reinterpret_cast<HWND>(host->winId()),region);
            GetRgnBox(region,&actual); DeleteObject(region);
            check(kind!=ERROR,"Windows host really applies the input region");
            const qreal dpr=host->devicePixelRatio();
            check(qAbs(actual.left-qRound(wanted.left()*dpr))<=2
                      && qAbs(actual.right-qRound((wanted.right()+1)*dpr))<=2,
                  "native window region matches scaled content and excludes transparent host margins");
#endif
        }
    }
}
int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    const bool native=app.arguments().contains("--native-host");
    QTemporaryDir dir;
    QFile file(dir.filePath("fixture.lrc"));
    check(file.open(QIODevice::WriteOnly), "open isolated LRC fixture");
    file.write("[00:01.00]   \n[00:20.00]First verse\n[00:21.00] \t \n[00:30.00]Second verse\n");
    file.close();
    Oran7LyricsModel model;
    model.loadFromFile(file.fileName());
    check(model.count()==4, "regular lyrics view retains original blank rows");
    model.setSkipEmptyLines(true);
    check(model.count()==2 && model.textAt(0)=="First verse", "island removes leading and intermediate whitespace rows");
    check(qAbs(model.lineTriggerTime(0)-19.7)<0.0001, "trigger time shares parser's 300ms compensation");
    QQmlEngine engine;
    engine.rootContext()->setContextProperty("fixtureModel", &model);
    QQmlComponent component(&engine);
    component.loadUrl(QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Components/Oran7GlobalComponents/Oran7LyricsIslandState.qml"));
    std::unique_ptr<QObject> state(component.create());
    if (!state) for (const auto& e : component.errors()) std::fprintf(stderr,"%s\n",qPrintable(e.toString()));
    check(bool(state), "production island presentation loads in isolation");
    state->setProperty("model", QVariant::fromValue<QObject*>(&model));
    state->setProperty("songTitle", "Song"); state->setProperty("artist", "Artist");
    check(state->property("titleText")=="Song - Artist", "title includes song and artist");
    check(state->property("idleTimeoutMs")==7000, "default idle threshold is seven seconds");
    check(state->property("minimumIntroMs")==3000, "song introduction has a three-second minimum");
    state->setProperty("minimumIntroMs",20);
    QTest::qWait(40);
    QSignalSpy animations(state.get(), SIGNAL(lyricActivated()));
    state->setProperty("positionSeconds",16.69);
    check(!state->property("lyricsVisible").toBool(), "long intro retains song info before first lyric minus three seconds");
    state->setProperty("positionSeconds",16.7);
    check(state->property("lyricsVisible").toBool() && state->property("displayedLine")==0 && animations.count()==1,
          "first lyric is presented exactly three seconds before its compensated trigger");
    QTest::qWait(70);
    check(state->property("lyricsVisible").toBool(), "first lyric preview is not a fixed wall-clock intro timer");
    state->setProperty("idleTimeoutMs", 40);
    state->setProperty("positionSeconds",19.7);
    check(model.currentLine()==0 && animations.count()==1, "actual first timestamp does not duplicate preview animation");
    state->setProperty("positionSeconds",21.0);
    check(model.currentLine()==0 && state->property("displayedLine")==0 && animations.count()==1,
          "blank timestamp keeps previous lyric and triggers no scroll or dissolve");
    QTest::qWait(70);
    check(!state->property("lyricsVisible").toBool() && state->property("titleText")=="Song - Artist",
          "no new valid lyric returns to song information at idle timeout");
    state->setProperty("positionSeconds",29.7);
    check(state->property("lyricsVisible").toBool() && state->property("displayedLine")==1 && animations.count()==2,
          "next nonblank line exits idle and animates once");
    state->setProperty("artist", "Updated artist");
    check(state->property("titleText")=="Song - Updated artist" && animations.count()==2,
          "late metadata updates title without restarting lyric intro");
    state->setProperty("positionSeconds",0.0);
    check(!state->property("lyricsVisible").toBool(), "backward seek into intro restores song information");
    state->setProperty("positionSeconds",16.7);
    check(state->property("lyricsVisible").toBool(), "seeking forward observes the same preview threshold");
    state->setProperty("positionSeconds",0.0);
    state->setProperty("positionSeconds",17.0);
    state->setProperty("positionSeconds",10.0);
    check(!state->property("lyricsVisible").toBool(), "seek before threshold cancels preview even while model row stays minus one");
    state->setProperty("trackKey", "next-song");
    check(!model.hasLyrics() && !state->property("lyricsVisible").toBool(), "song identity change clears old lyrics and idle timer");
    state->setProperty("positionSeconds",30.0);
    model.loadFromFile(file.fileName());
    check(!state->property("lyricsVisible").toBool(),"early or late-loaded lyric cannot bypass minimum song introduction");
    QTest::qWait(40);
    check(state->property("displayedLine")==1 && state->property("lyricsVisible").toBool(),
          "late lyrics load aligns directly to current playback timestamp");
    state->setProperty("artist", " ");
    check(state->property("titleText")=="Song", "missing artist leaves no dangling separator");
    check(file.open(QIODevice::WriteOnly|QIODevice::Truncate), "rewrite isolated blank fixture");
    file.write("[00:01.00] \t \n"); file.close(); model.loadFromFile(file.fileName());
    check(!model.hasLyrics() && !state->property("lyricsVisible").toBool(), "all-blank lyrics retain song information");

    // 实际胶囊布局及信号接线；仅替换应用入口和 GPU 遮罩，不加载主程序/真实配置。
    QQmlPropertyMap theme, basic;
    theme.insert("Oran7MusicLyricsIsland",QVariantMap{{"floatingBackdropColor-6","#333333"},{"backgroundImageEnabled",false},
        {"floatingEnabled",true},{"floatingLocked",false},{"floatingClickThrough",false},{"floatingAlwaysOnTop",false},
        {"floatingOpacity",0.9},{"lyricsFontSize",20},{"backgroundImage_url",""},{"floatingX",-1},{"floatingY",-1}});
    theme.insert("Oran7MusicPlayControls",QVariantMap{{"blurEffectEnabled",false},{"saturation",0.0},{"brightness",0.0},{"contrast",0.0},{"iconColorBase-7","#ffffff"}});
    basic.insert("currentMediaName","Fixture song"); basic.insert("currentMediaArtistAuthor","Fixture artist");
    basic.insert("currentMediaFilePath","fixture.mp3"); basic.insert("globalPlayingFocus",0);
    basic.insert("globalPlayer_MusicPlayerIndex",0); basic.insert("mainWindow",QVariant::fromValue<QObject*>(nullptr));
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Theme",&theme);
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Api",Oran7Api::instance());
    qmlRegisterType<Oran7LyricsModel>("LyricsModel",1,0,"LyricsModel");
    QQmlComponent clientComponent(&engine);
    clientComponent.setData("import QtQml\nQtObject { property QtObject playback: QtObject { property var currentTrack: ({songId:10}); property int positionMs:0 }; signal lyricsAvailable(string path); signal lyricsUnavailable() }",QUrl());
    std::unique_ptr<QObject> client(clientComponent.create());
    check(bool(client),"isolated Client boundary");
    qmlRegisterSingletonInstance("Client",1,0,"Client",client.get());
    engine.rootContext()->setContextProperty("BasicConfig",&basic);
    const auto components=QStringLiteral(APP_ROOT)+"/src/qml/Components/Oran7GlobalComponents/";
    for (const auto& name : {"Oran7LyricsIsland.qml","Oran7LyricsIslandState.qml","Oran7BlurCard.qml","Oran7RoundedImage.qml","Oran7LyricsFloatingWindow.qml"}) {
        QFile input(components+name); check(input.open(QIODevice::ReadOnly),"read production island component");
        auto data=input.readAll(); data.replace("import \"../../Basic\"","");
        if (QByteArray(name)=="Oran7LyricsFloatingWindow.qml") data.replace("layer.enabled: true","layer.enabled: false");
        QFile output(dir.filePath(name)); check(output.open(QIODevice::WriteOnly),"copy isolated island component");
        output.write(data);
    }
    QFile mask(dir.filePath("Oran7ShaderTextMask.qml")); check(mask.open(QIODevice::WriteOnly),"isolated shader boundary");
    mask.write("import QtQuick\nItem { property bool effectEnabled:false }"); mask.close();
    QQuickWindow islandWindow;
    islandWindow.setFlags(Qt::Tool|Qt::FramelessWindowHint|Qt::WindowDoesNotAcceptFocus);
    if (native) islandWindow.setOpacity(0);
    islandWindow.resize(1200,80);
    QQmlComponent islandComponent(&engine,QUrl::fromLocalFile(dir.filePath("Oran7LyricsIsland.qml")));
    std::unique_ptr<QObject> island(islandComponent.create());
    if (!island) for (const auto& e : islandComponent.errors()) std::fprintf(stderr,"%s\n",qPrintable(e.toString()));
    check(bool(island),"actual island layout and presentation connections instantiate");
    auto* islandItem=qobject_cast<QQuickItem*>(island.get());
    check(islandItem,"actual island is a visual item");
    islandItem->setParentItem(islandWindow.contentItem());
    island->setProperty("islandVisible",true);
    islandWindow.show();
    check(island->property("titleText")=="Fixture song - Fixture artist","actual island uses combined title");
    check(file.open(QIODevice::WriteOnly|QIODevice::Truncate),"rewrite visual fixture");
    file.write("[00:20.00]A very long lyric line that exceeds the initial title width and keeps its layout stable while the capsule grows around it\n[00:21.00] \n[00:30.00]Hi\n");file.close();
    check(QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName())),"actual Client lyric signal delivered");
    check(!island->property("_displayLyrics").toBool(),"actual island waits through long intro");
    auto* fakePlayback=client->property("playback").value<QObject*>();
    fakePlayback->setProperty("positionMs",16700);
    check(!island->property("_displayLyrics").toBool(),"actual island preserves three-second intro even after lyric preview deadline");
    fakePlayback->setProperty("positionMs",21000);
    QTest::qWait(2800);
    checkHorizontalStability(islandItem,650);
    check(island->property("_displayLyrics").toBool() && island->property("displayedLine")==0,"actual island prewarms from playback timestamp");
    fakePlayback->setProperty("positionMs",21000);
    check(island->property("displayedLine")==0,"actual island holds previous row across blank timestamp");
    fakePlayback->setProperty("positionMs",29700);
    check(island->property("displayedLine")==1,"actual island advances to next content row");
    checkHorizontalStability(islandItem,650);
    const auto suffixWidth=island->property("targetWidth").toDouble();
    island->setProperty("cu_suffixStep",island->property("suffixStep"));
    check(qAbs(suffixWidth-island->property("targetWidth").toDouble())<0.01,"suffix completion never changes capsule target width");
    checkHorizontalStability(islandItem,150);
    auto* lyricView=island->findChild<QObject*>("islandLyricList");
    auto* idleLabel=island->findChild<QObject*>("islandIdleLabel");
    check(lyricView && idleLabel,"actual animated title and lyric layers exist");
    QTest::qWait(650);
    check(lyricView->property("opacity").toDouble()>0.99 && idleLabel->property("opacity").toDouble()<0.01,
          "shared transition settles on lyrics without overlapping title");
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"clear actual lyric layer");
    QTest::qWait(160);
    const auto midway=island->property("lyricsBlend").toDouble();
    check(midway>0.0 && midway<1.0,"idle transition has a continuous intermediate state");
    check(QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName())),"reverse transition while still in flight");
    QTest::qWait(650);
    check(lyricView->property("opacity").toDouble()>0.99 && idleLabel->property("opacity").toDouble()<0.01,
          "interrupted transition smoothly returns to lyrics without a stale delayed fade");
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"return to idle after interrupted transition");
    QTest::qWait(650);
    check(idleLabel->property("opacity").toDouble()>0.99 && lyricView->property("opacity").toDouble()<0.01,
          "reverse transition settles on idle song information");

    // 覆盖完整生产浮窗，而不只检查固定窗口中的歌词 delegate。
    QQmlComponent floatingComponent(&engine,QUrl::fromLocalFile(dir.filePath("Oran7LyricsFloatingWindow.qml")));
    std::unique_ptr<QObject> floating(floatingComponent.createWithInitialProperties({{"displayAllowed",false},{"opacity",0.0}}));
    if (!floating) for (const auto& e : floatingComponent.errors()) std::fprintf(stderr,"%s\n",qPrintable(e.toString()));
    auto* floatingWindow=qobject_cast<QQuickWindow*>(floating.get());
    check(floatingWindow,"production floating window loads without main app or user configuration");
    floatingWindow->setFlag(Qt::WindowDoesNotAcceptFocus,true);
    floating->setProperty("displayAllowed",true);
    QTest::qWait(50);
    auto* floatingCapsule=findVisual(floatingWindow->contentItem(),"islandTextStage");
    check(floatingCapsule,"production floating capsule exists");
    while (floatingCapsule && !floatingCapsule->property("islandMaxWidth").isValid())
        floatingCapsule=floatingCapsule->parentItem();
    check(floatingCapsule,"found production capsule through blur card's content container");
    check(qAbs(floatingWindow->width()-floatingCapsule->property("islandMaxWidth").toDouble()-24)<0.01,
          "host uses fixed maximum canvas rather than animated capsule width");
    basic.insert("currentMediaName","A very long song title used to exercise a large title width change");
    basic.insert("currentMediaArtistAuthor","A different artist");
    basic.insert("currentMediaFilePath","next-title-song.mp3");
    checkTitleAndHost(floatingWindow,floatingCapsule,650,native);
    basic.insert("currentMediaName","Short"); basic.insert("currentMediaArtistAuthor","A");
    basic.insert("currentMediaFilePath","short-title-song.mp3");
    checkTitleAndHost(floatingWindow,floatingCapsule,650,native);
    check(QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName())),"load lyrics into actual floating host");
    QTest::qWait(2200);
    checkTitleAndHost(floatingWindow,floatingCapsule,650,native);
    check(floatingCapsule->property("_displayLyrics").toBool(),"floating host actually enters lyric state after introduction");
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"floating host returns to idle");
    checkTitleAndHost(floatingWindow,floatingCapsule,650,native);
    check(!floatingWindow->mask().contains(QPoint(0,floatingWindow->height()/2)),"transparent native host margin cannot intercept desktop clicks");
    floatingWindow->hide();
#ifdef Q_OS_WIN
    if (!native) {
        QProcess child;
        auto environment=QProcessEnvironment::systemEnvironment();
        environment.insert("QT_QPA_PLATFORM","windows"); environment.insert("QT_QUICK_BACKEND","software");
        environment.insert("QT_SCALE_FACTOR","1.25");
        child.setProcessEnvironment(environment);
        child.start(app.applicationFilePath(),{"--native-host"});
        check(child.waitForFinished(55000),"isolated zero-opacity native host checks finish");
        if (child.exitCode()!=0) std::fprintf(stderr,"%s\n",child.readAllStandardError().constData());
        check(child.exitStatus()==QProcess::NormalExit && child.exitCode()==0,"native Windows geometry and mask checks pass at fractional DPI");
    }
#endif
    std::printf("PASS: %d lyrics island checks\n", checks);
}
