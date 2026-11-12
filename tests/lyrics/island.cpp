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
#include <QScreen>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <memory>
#include <cstdio>
#include <cstdlib>

// 位置持久化仅写隔离内存主题，绝不接触用户 profile。
class Oran7LyricsThemeFixture : public QQmlPropertyMap {
    Q_OBJECT
public:
    Oran7LyricsThemeFixture() : QQmlPropertyMap(this, nullptr) {}
    Q_INVOKABLE void saveComponentToken(const QString& component, const QString& token, const QVariant& tokenValue) {
        auto group=value(component).toMap();
        group.insert(token,tokenValue);
        insert(component,group);
    }
};

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
    check(state->property("idleTimeoutMs")==10000, "default idle threshold retains the current ten-second setting");
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
    Oran7LyricsThemeFixture theme;
    QQmlPropertyMap basic;
    theme.insert("Oran7MusicLyricsIsland",QVariantMap{{"floatingBackdropColor-6","#333333"},{"backgroundImageEnabled",false},
        {"floatingEnabled",true},{"floatingLocked",false},{"floatingClickThrough",false},{"floatingAlwaysOnTop",false},
        {"floatingOpacity",0.9},{"lyricsFontSize",20},{"backgroundImage_url",""},{"floatingX",-1},{"floatingY",-1}});
    theme.insert("Oran7MusicPlayControls",QVariantMap{{"blurEffectEnabled",false},{"saturation",0.0},{"brightness",0.0},{"contrast",0.0},{"iconColorBase-7","#ffffff"}});
    theme.insert("Primary",QVariantMap{{"durationFast",100}});
    basic.insert("currentMediaName","Fixture song"); basic.insert("currentMediaArtistAuthor","Fixture artist");
    basic.insert("currentMediaFilePath","fixture.mp3"); basic.insert("globalPlayingFocus",0);
    basic.insert("globalPlayer_MusicPlayerIndex",0); basic.insert("mainWindow",QVariant::fromValue<QObject*>(nullptr));
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Theme",&theme);
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Api",Oran7Api::instance());
    qmlRegisterType<Oran7LyricsModel>("LyricsModel",1,0,"LyricsModel");
    QQmlComponent clientComponent(&engine);
    clientComponent.setData(R"qml(import QtQml
QtObject {
    property QtObject playback: QtObject {
        property var currentTrack: ({songId: 10})
        property int positionMs: 0
        property int durationMs: 35000
        property bool active: false
    }
    property int toggleCount: 0
    property QtObject musicQueue: QtObject { function toggleCurrent() { client.toggleCount++ } }
    function seekTo(seconds) { playback.positionMs = seconds * 1000 }
    function playPrevious() {}
    function playNext() {}
    id: client
    signal lyricsAvailable(string path)
    signal lyricsUnavailable()
})qml",QUrl());
    std::unique_ptr<QObject> client(clientComponent.create());
    check(bool(client),"isolated Client boundary");
    qmlRegisterSingletonInstance("Client",1,0,"Client",client.get());
    engine.rootContext()->setContextProperty("BasicConfig",&basic);
    const auto components=QStringLiteral(APP_ROOT)+"/src/qml/Components/Oran7GlobalComponents/";
    for (const auto& name : {"Oran7LyricsIsland.qml","Oran7LyricsIslandState.qml","Oran7BlurCard.qml","Oran7RoundedImage.qml","Oran7LyricsFloatingWindow.qml",
                             "Oran7StudioButton.qml","Oran7ValueSlider.qml"}) {
        QFile input(components+name); check(input.open(QIODevice::ReadOnly),"read production island component");
        auto data=input.readAll(); data.replace("import \"../../Basic\"","");
        if (QByteArray(name)=="Oran7LyricsFloatingWindow.qml") {
            data.replace("layer.enabled: true","layer.enabled: false");
            data.replace("qrc:/image/", QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/image/").toEncoded());
        }
        QFile output(dir.filePath(name)); check(output.open(QIODevice::WriteOnly),"copy isolated island component");
        output.write(data);
    }
    QFile mask(dir.filePath("Oran7ShaderTextMask.qml")); check(mask.open(QIODevice::WriteOnly),"isolated shader boundary");
    mask.write("import QtQuick\nItem { property bool effectEnabled:false; property string shaderName }"); mask.close();
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
    checkHorizontalStability(islandItem,900);
    check(island->property("_displayLyrics").toBool() && island->property("displayedLine")==0,"actual island prewarms from playback timestamp");
    check(island->property("_revealDurationMs").toInt()>3800 &&
              island->property("_revealDurationMs").toInt()<=4000,
          "long lyric respects the four-second dissolve limit");
    fakePlayback->setProperty("positionMs",21000);
    check(island->property("displayedLine")==0,"actual island holds previous row across blank timestamp");
    QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName()));
    QTest::qWait(450);
    QList<QQuickItem*> outgoingLabels;
    collectLyricLabels(islandItem,outgoingLabels);
    QQuickItem* outgoingDelegate=nullptr;
    for (auto* label : outgoingLabels) {
        if (label->property("lyricLine").toInt()!=0) continue;
        auto* item=label;
        while (item && !item->property("revealProgress").isValid()) item=item->parentItem();
        outgoingDelegate=item;
        break;
    }
    check(outgoingDelegate,"outgoing lyric delegate exists for dissolve lifetime checks");
    const auto outgoingProgress=outgoingDelegate->property("revealProgress").toDouble();
    auto* synchronizedLyricList=island->findChild<QQuickItem*>("islandLyricList");
    check(synchronizedLyricList,"real lyric list exists for coordinated motion checks");
    const auto shellStart=islandItem->width();
    const auto scrollStart=synchronizedLyricList->property("contentY").toDouble();
    QSignalSpy revealRequests(island.get(),SIGNAL(lyricRevealRequested(int,int)));
    fakePlayback->setProperty("positionMs",29700);
    check(island->property("displayedLine")==1,"actual island advances to next content row");
    const auto shellTarget=island->property("targetWidth").toDouble();
    const auto scrollTarget=1.5*synchronizedLyricList->property("lineH").toDouble()-islandItem->height()/2;
    QTest::qWait(200);
    const auto shellProgress=(islandItem->width()-shellStart)/(shellTarget-shellStart);
    const auto scrollProgress=(synchronizedLyricList->property("contentY").toDouble()-scrollStart)/(scrollTarget-scrollStart);
    if (!(shellProgress>0.01 && shellProgress<0.2 && qAbs(shellProgress-scrollProgress)<0.025))
        std::fprintf(stderr,"Motion sample native=%d shell=%.4f scroll=%.4f width=%.2f from=%.2f to=%.2f\n",
                     native,shellProgress,scrollProgress,islandItem->width(),shellStart,shellTarget);
    check(shellProgress>0.01 && shellProgress<0.2 && qAbs(shellProgress-scrollProgress)<0.025,
          "shell and lyric scroll share a slow curved start and the same animation progress");
    check(revealRequests.isEmpty() && !island->property("_revealStarted").toBool(),
          "lyric dissolve cannot start in the first half of the scroll");
    check(outgoingDelegate->property("revealProgress").toDouble()>outgoingProgress+0.01,
          "outgoing lyric keeps dissolving after the next line starts scrolling");
    QTest::qWait(260);
    check(revealRequests.count()==1 && revealRequests.first().at(0).toInt()==1 &&
              revealRequests.first().at(1).toInt()==900,
          "a very short lyric finishes promptly despite a long remaining song-end display window");
    QVariant minimumDuration;
    check(QMetaObject::invokeMethod(island.get(),"revealDurationForLine",Q_RETURN_ARG(QVariant,minimumDuration),
              Q_ARG(QVariant,1),Q_ARG(QVariant,34900)) && minimumDuration.toInt()==900,
          "very short remaining display time preserves the minimum dissolve duration");
    QVariant mediumBudget, spacedBudget, punctuatedBudget, longBudget;
    const auto textBudget=[&](const QString& text,QVariant& result) {
        return QMetaObject::invokeMethod(island.get(),"revealTextBudget",Q_RETURN_ARG(QVariant,result),Q_ARG(QVariant,text));
    };
    check(textBudget(QString::fromUtf8("风吹过远方的山川"),mediumBudget) &&
              textBudget(QString::fromUtf8("风 吹 过 远 方 的 山 川"),spacedBudget) &&
              textBudget(QString::fromUtf8("风吹过远方的山川……"),punctuatedBudget) &&
              textBudget(QString::fromUtf8("风吹过远方的山川，我们依然等待明日的星光照亮归途"),longBudget),
          "length budgets support Unicode lyric text");
    check(mediumBudget.toDouble()>900 && mediumBudget.toDouble()<2000 &&
              spacedBudget.toDouble()==mediumBudget.toDouble() &&
              punctuatedBudget.toDouble()-mediumBudget.toDouble()<100 &&
              longBudget.toDouble()>mediumBudget.toDouble(),
          "medium phrases get more time while spaces and punctuation cannot inflate the dissolve");
    QVariant hurriedLongDuration;
    check(QMetaObject::invokeMethod(island.get(),"revealDurationForLine",Q_RETURN_ARG(QVariant,hurriedLongDuration),
              Q_ARG(QVariant,0),Q_ARG(QVariant,28000)) &&
              hurriedLongDuration.toInt()>900 && hurriedLongDuration.toInt()<1200,
          "next lyric deadline still shortens long-text dissolve below its length budget");
    checkHorizontalStability(islandItem,900);
    const auto outgoingSettledProgress=outgoingDelegate->property("revealProgress").toDouble();
    QTest::qWait(100);
    check(outgoingSettledProgress<0.99 &&
              qAbs(outgoingDelegate->property("revealProgress").toDouble()-outgoingSettledProgress)<0.001,
          "only a completed scroll stops the unfinished dissolve of an outgoing lyric");
    const auto suffixWidth=island->property("targetWidth").toDouble();
    island->setProperty("cu_suffixStep",island->property("suffixStep"));
    check(qAbs(suffixWidth-island->property("targetWidth").toDouble())<0.01,"suffix completion never changes capsule target width");
    checkHorizontalStability(islandItem,150);
    auto* lyricView=island->findChild<QObject*>("islandLyricList");
    auto* idleLabel=island->findChild<QObject*>("islandIdleLabel");
    check(lyricView && idleLabel,"actual animated title and lyric layers exist");
    QTest::qWait(900);
    check(lyricView->property("opacity").toDouble()>0.99 && idleLabel->property("opacity").toDouble()<0.01,
          "shared transition settles on lyrics without overlapping title");
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"clear actual lyric layer");
    QTest::qWait(160);
    const auto midway=island->property("lyricsBlend").toDouble();
    check(midway>0.0 && midway<1.0,"idle transition has a continuous intermediate state");
    check(QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName())),"reverse transition while still in flight");
    QTest::qWait(900);
    check(lyricView->property("opacity").toDouble()>0.99 && idleLabel->property("opacity").toDouble()<0.01,
          "interrupted transition smoothly returns to lyrics without a stale delayed fade");
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"return to idle after interrupted transition");
    QTest::qWait(900);
    check(idleLabel->property("opacity").toDouble()>0.99 && lyricView->property("opacity").toDouble()<0.01,
          "reverse transition settles on idle song information");
    fakePlayback->setProperty("positionMs",21000);
    revealRequests.clear();
    QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName()));
    QTest::qWait(120);
    QMetaObject::invokeMethod(client.get(),"lyricsUnavailable");
    QTest::qWait(450);
    check(revealRequests.isEmpty(),"clearing lyrics before scroll midpoint cancels the pending dissolve");
    fakePlayback->setProperty("positionMs",29700);

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
    basic.insert("currentMediaName","Short");
    basic.insert("currentMediaArtistAuthor","A");
    QTest::qWait(900);
    const auto initialWidth=floatingCapsule->width();
    basic.insert("currentMediaName","Medium length title");
    basic.insert("currentMediaArtistAuthor","Artist");
    basic.insert("currentMediaFilePath","next-title-song.mp3");
    check(qAbs(floatingCapsule->width()-initialWidth)<0.5,"new title starts shell motion from the displayed width without snapping");
    QTest::qWait(200);
    const auto movingWidth=floatingCapsule->width();
    const auto previousTargetWidth=floatingCapsule->property("targetWidth").toDouble();
    check(movingWidth>initialWidth+1 && movingWidth<floatingCapsule->property("targetWidth").toDouble()-1,
          "shell responds promptly and passes through intermediate widths");
    basic.insert("currentMediaName","A very long song title used to exercise a large title width change with extra metadata");
    check(floatingCapsule->property("targetWidth").toDouble()>previousTargetWidth+1 &&
              qAbs(floatingCapsule->width()-movingWidth)<0.5,
          "metadata arriving during shell motion retargets continuously without resetting geometry");
    checkTitleAndHost(floatingWindow,floatingCapsule,900,native);
    check(qAbs(floatingCapsule->width()-floatingCapsule->property("targetWidth").toDouble())<0.5,
          "retargeted shell settles at its measured title width");
    basic.insert("currentMediaName","Short"); basic.insert("currentMediaArtistAuthor","A");
    basic.insert("currentMediaFilePath","short-title-song.mp3");
    checkTitleAndHost(floatingWindow,floatingCapsule,900,native);
    check(qAbs(floatingCapsule->width()-floatingCapsule->property("targetWidth").toDouble())<0.5,
          "shell contraction settles without a lingering rebound");
    check(QMetaObject::invokeMethod(client.get(),"lyricsAvailable",Q_ARG(QString,file.fileName())),"load lyrics into actual floating host");
    QTest::qWait(2200);
    checkTitleAndHost(floatingWindow,floatingCapsule,900,native);
    check(floatingCapsule->property("_displayLyrics").toBool(),"floating host actually enters lyric state after introduction");
    floating->setProperty("lyricsHeight",82);
    QTest::qWait(100);
    check(floatingWindow->height()==106,"configured lyric height applies in lyric state, plus shadow padding");
    auto* floatingList=findVisual(floatingWindow->contentItem(),"islandLyricList");
    check(floatingList,"floating lyric viewport exists");
    const auto scrollPosition=floatingList->property("contentY");
    const auto restingY=floatingWindow->y();
    const auto collapsedHeight=floatingWindow->height();
    const auto panelHeight=floating->property("controlsHeight").toInt();
    const auto lyricGeometry=QRectF(floatingCapsule->mapToGlobal(QPointF()),floatingCapsule->size());
    auto* controls=findVisual(floatingWindow->contentItem(),"floatingPlaybackControls");
    auto* controlsContent=findVisual(floatingWindow->contentItem(),"floatingPlaybackContent");
    check(controls && controlsContent,"floating playback panel and its content exist");
    auto* capsuleSurface=findVisual(floatingWindow->contentItem(),"floatingCapsuleSurface");
    auto* shadowSilhouette=findVisual(floatingWindow->contentItem(),"floatingShadowSilhouette");
    check(capsuleSurface && controls->parentItem()==capsuleSurface && floatingCapsule->parentItem()==capsuleSurface,
          "lyrics and transport share one expandable capsule surface");
    check(shadowSilhouette,"floating capsule has a dedicated shadow silhouette");
    const auto checkLyricGeometry=[&] {
        const auto rect=QRectF(floatingCapsule->mapToGlobal(QPointF()),floatingCapsule->size());
        check(rect==lyricGeometry && floatingList->property("contentY")==scrollPosition,
              "hover animation does not move or resize the lyrics");
        check(shadowSilhouette->size()==capsuleSurface->size() &&
                  shadowSilhouette->position()==capsuleSurface->position(),
              "shadow silhouette follows every frame of the capsule animation");
    };
    floating->setProperty("controlsExpanded",true);
    bool expandedBeforeFadeIn=false;
    bool heightOvershot=false;
    QElapsedTimer hoverAnimation;
    hoverAnimation.start();
    while (hoverAnimation.elapsed()<400) {
        QTest::qWait(10);
        checkLyricGeometry();
        heightOvershot |= floatingWindow->height()>collapsedHeight+panelHeight;
        expandedBeforeFadeIn |= controls->height()>0 && controlsContent->opacity()==0;
        if (controlsContent->opacity()>0)
            check(controls->height()>=panelHeight*0.55,"controls fade in after the panel has partly expanded");
    }
    check(expandedBeforeFadeIn,"panel height starts expanding before the controls fade in");
    check(heightOvershot,"the existing window height makes a small overshoot before settling");
    check(controls->isVisible() && floatingWindow->height()==collapsedHeight+panelHeight,
          "expanded host exposes playback controls");
    check(floatingWindow->y()==restingY && floatingList->property("contentY")==scrollPosition,
          "downward expansion preserves the window top and the lyric scroll coordinate");
    auto* transportRow=findVisual(floatingWindow->contentItem(),"floatingTransportRow");
    auto* progress=findVisual(floatingWindow->contentItem(),"floatingPlaybackProgress");
    check(transportRow && progress,"floating transport row and progress slider exist");
    const auto transportCenter=transportRow->mapToScene(QPointF(transportRow->width()/2,transportRow->height()/2));
    const auto progressCenter=progress->mapToScene(QPointF(progress->width()/2,progress->height()/2));
    check(progress->mapToScene(QPointF()).x()>transportRow->mapToScene(QPointF(transportRow->width(),0)).x() &&
              qAbs(transportCenter.y()-progressCenter.y())<1,
          "transport buttons are to the left of the slider on the same row");
    floating->setProperty("controlsExpanded",false);
    bool fadedBeforeCollapse=false;
    hoverAnimation.restart();
    while (hoverAnimation.elapsed()<440) {
        QTest::qWait(10);
        checkLyricGeometry();
        fadedBeforeCollapse |= controls->height()==panelHeight && controlsContent->opacity()<1;
        if (controls->height()<panelHeight)
            check(controlsContent->opacity()==0,"controls finish fading out before the panel shrinks");
    }
    check(fadedBeforeCollapse,"controls fade out while the panel holds its full height");
    check(!controls->isVisible() && floatingWindow->height()==106 && floatingWindow->y()==restingY,
          "collapse restores native geometry");
    if (!native) {
        const auto previousPosition=floatingWindow->position();
        floatingWindow->setPosition(0,0);
        floating->setProperty("desktopAreas",QVariantList{QRectF(0,0,1440,900)});
        QTest::mouseMove(floatingWindow,QPoint(floatingWindow->width()/2,30));
        QTest::qWait(320);
        check(controls->isVisible() && floatingWindow->y()==0,"hover opens downward when the screen has space");
        const auto playPoint=transportRow->mapToScene(QPointF(transportRow->width()/2,transportRow->height()/2)).toPoint();
        QTest::mouseMove(floatingWindow,playPoint);
        QTest::qWait(240);
        check(controls->isVisible(),"moving into playback controls keeps the panel open");
        QTest::mouseClick(floatingWindow,Qt::LeftButton,Qt::NoModifier,playPoint);
        check(client->property("toggleCount").toInt()==1,"play button uses the music queue action");
        const auto sliderPoint=progress->mapToScene(QPointF(progress->width()/2,progress->height()/2)).toPoint();
        QTest::mouseClick(floatingWindow,Qt::LeftButton,Qt::NoModifier,sliderPoint);
        check(fakePlayback->property("positionMs").toInt()==17000,"releasing the progress slider seeks to the selected second");
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(floatingWindow,&leave);
        QTest::qWait(650);
        check(!controls->isVisible(),"leaving the window collapses playback controls");
        floatingWindow->setPosition(previousPosition);
        floating->setProperty("desktopAreas",Oran7Api::instance()->screenGeometries());
    }
    check(floatingWindow->height()==collapsedHeight,"floating host has the configured height before switching tracks");
    basic.insert("currentMediaName","New track");
    basic.insert("currentMediaFilePath","new-track-awaiting-lyrics.mp3");
    fakePlayback->setProperty("positionMs",0);
    check(QMetaObject::invokeMethod(client.get(),"lyricsUnavailable"),"clear the previous track's lyrics");
    checkTitleAndHost(floatingWindow,floatingCapsule,900,native);
    check(!floatingCapsule->property("_displayLyrics").toBool() && floatingWindow->height()==collapsedHeight,
          "the new track title keeps the configured height while waiting for lyrics");
    check(!floatingWindow->mask().contains(QPoint(0,floatingWindow->height()/2)),"transparent native host margin cannot intercept desktop clicks");
    const auto realAreas=Oran7Api::instance()->screenGeometries();
    check(!realAreas.isEmpty() && realAreas.first().toRectF()==QRectF(QGuiApplication::primaryScreen()->geometry()),
          "desktop geometry includes taskbar areas in logical coordinates");
    const QRectF primaryArea(0,0,1440,900), negativeArea(-1600,-900,1600,900);
    floating->setProperty("desktopAreas",QVariantList{primaryArea,negativeArea});
    auto positionTheme=theme.value("Oran7MusicLyricsIsland").toMap();
    positionTheme.insert("floatingPositionVersion",2);
    positionTheme.insert("floatingCenterX",-800);
    positionTheme.insert("floatingY",-880);
    theme.insert("Oran7MusicLyricsIsland",positionTheme);
    check(QMetaObject::invokeMethod(floating.get(),"restorePosition"),"restore real floating position from isolated theme");
    QTest::qWait(30);
    check(qAbs(floatingWindow->x()+floatingWindow->width()/2.0+800)<1 && floatingWindow->y()==-880 &&
              negativeArea.contains(QRectF(floatingWindow->geometry())),
          "negative coordinates restore on the upper-left monitor without being mistaken for unset values");
    check(QMetaObject::invokeMethod(floating.get(),"savePosition"),"persist floating position to isolated theme");
    const auto savedPosition=theme.value("Oran7MusicLyricsIsland").toMap();
    check(savedPosition.value("floatingPositionVersion").toInt()==2 &&
              qAbs(savedPosition.value("floatingCenterX").toDouble()+800)<1 &&
              savedPosition.value("floatingY").toInt()==-880,
          "position persistence stores a stable center and retains signed vertical coordinates");
    basic.insert("currentMediaName","A different and much longer title after saving the floating position");
    QTest::qWait(900);
    floatingWindow->setPosition(7000,7000);
    QMetaObject::invokeMethod(floating.get(),"restorePosition");
    QTest::qWait(30);
    check(qAbs(floatingWindow->x()+floatingWindow->width()/2.0+800)<1 && floatingWindow->y()==-880,
          "restoring saved center is independent of current title and animated capsule width");
    positionTheme=savedPosition;
    positionTheme.insert("floatingPositionVersion",0);
    positionTheme.insert("floatingX",-1000);
    positionTheme.insert("floatingY",-700);
    theme.insert("Oran7MusicLyricsIsland",positionTheme);
    QMetaObject::invokeMethod(floating.get(),"restorePosition");
    QTest::qWait(30);
    check(floatingWindow->x()<0 && floatingWindow->y()==-700 && negativeArea.contains(QPointF(floatingWindow->position())),
          "legacy left-edge coordinates on a negative monitor remain compatible");
    // 屏幕拓扑变化只更新数据；重新启用前不能主动挪动用户窗口。
    const auto positionBeforeRemoval=floatingWindow->position();
    floating->setProperty("desktopAreas",QVariantList{primaryArea});
    QTest::qWait(50);
    check(floatingWindow->position()==positionBeforeRemoval,"monitor removal alone never relocates an enabled floating window");
    floating->setProperty("enabled",false); floating->setProperty("enabled",true);
    check(primaryArea.contains(QPointF(floatingWindow->position())),"re-enabling recovers an offscreen top-left corner onto a remaining desktop");
    floatingWindow->setPosition(1439,899);
    floating->setProperty("enabled",false); floating->setProperty("enabled",true);
    check(floatingWindow->position()==QPoint(1439,899),"a corner in the taskbar area is valid even when most of the window extends offscreen");
    positionTheme.insert("floatingX",300);
    positionTheme.insert("floatingY",-1);
    theme.insert("Oran7MusicLyricsIsland",positionTheme);
    QMetaObject::invokeMethod(floating.get(),"restorePosition");
    check(floatingWindow->y()==primaryArea.bottom()-floatingWindow->height()-40,
          "an unset legacy vertical coordinate uses the selected desktop's bottom inset");
    floatingWindow->setPosition(5000,5000);
    QMetaObject::invokeMethod(floating.get(),"savePosition");
    check(floatingWindow->position()==QPoint(5000,5000),"position persistence never clamps freely dragged coordinates");
    floating->setProperty("displayAllowed",false); floating->setProperty("displayAllowed",true);
    floating->setProperty("desktopAreas",QVariantList{QRectF(0,0,620,500)});
    QTest::qWait(900);
    check(floatingWindow->position()==QPoint(5000,5000) && floatingWindow->width()==1024,
          "ordinary show/hide and desktop resizing do not relocate or shrink the floating canvas");
    floating->setProperty("enabled",false); floating->setProperty("enabled",true);
    check(floatingWindow->position()==QPoint(619,499),"only re-enabling clamps the top-left point rather than the entire canvas");
    floatingWindow->setPosition(620,500);
    floating->setProperty("enabled",false); floating->setProperty("enabled",true);
    check(floatingWindow->position()==QPoint(619,499),"right and bottom desktop limits are exclusive");
    if (!native) {
        floatingWindow->setPosition(100,100);
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(floatingWindow,&leave);
        QTest::qWait(500);
        const auto press=floatingList->mapToScene(QPointF(floatingList->width()/2,floatingList->height()/2)).toPoint();
        const auto outside=press+QPoint(1000,1000);
        QTest::mousePress(floatingWindow,Qt::LeftButton,Qt::NoModifier,press);
        QTest::mouseMove(floatingWindow,outside,30);
        QTest::mouseRelease(floatingWindow,Qt::LeftButton,Qt::NoModifier,outside);
        const auto releasedPosition=floatingWindow->position();
        QTest::qWait(300);
        check(releasedPosition.x()>620 && releasedPosition.y()>500 && floatingWindow->position()==releasedPosition,
              "real drag release and delayed save both preserve an out-of-desktop position until re-enabled");
    }
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
#include "island.moc"
