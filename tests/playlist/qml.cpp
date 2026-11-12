#include "Oran7TrackLibrary.h"
#include "Oran7PlaybackState.h"
#include "Oran7CloudMusicService.h"
#include "Oran7PlaybackViewModel.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlPropertyMap>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QWheelEvent>
#include <QFontDatabase>
#include <QTest>
#include <cmath>
#include <cstdio>
#include <cstdlib>

class Oran7ClientFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(Oran7TrackLibrary* tracks MEMBER library CONSTANT)
    Q_PROPERTY(Oran7PlaybackState* playback MEMBER state CONSTANT)
    Q_PROPERTY(Oran7CloudMusicService* cloudMusic MEMBER cloud CONSTANT)
    Q_PROPERTY(Oran7PlaybackViewModel* musicQueue MEMBER queue CONSTANT)
public:
    Oran7TrackLibrary* library; Oran7PlaybackState* state; Oran7CloudMusicService* cloud; Oran7PlaybackViewModel* queue;
    Q_INVOKABLE qint64 currentSongId() const { return state->currentTrack().songId; }
    Q_INVOKABLE void playNext() { queue->advance(1); }
};
class Oran7KeyboardFixture : public QObject {
    Q_OBJECT
signals:
    void keyCombinationTriggered(const QString& combination);
};
static int count=0;
static void check(bool ok,const char* name) {if(!ok){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}++count;}
static QQuickItem* find(QQuickItem* item,const QString& name){if(item->objectName()==name)return item;for(auto* c:item->childItems())if(auto* r=find(c,name))return r;return nullptr;}
int main(int argc,char**argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");qputenv("QT_QUICK_BACKEND","software");qputenv("QT_QUICK_CONTROLS_STYLE","Fusion");
    QGuiApplication app(argc,argv);QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");QFont::insertSubstitution(QString::fromUtf8("微软雅黑"),"Microsoft YaHei");
    Oran7TrackLibrary library; Oran7PlaybackState state;
    Oran7CloudMusicService cloud(library,{[]{return QVariantMap{{"isLoggedIn",false}};}, {},[](qint64){},[](qint64,bool){}});
    Oran7PlaybackViewModel queue(library,state,cloud);
    Oran7ClientFixture client;client.library=&library;client.state=&state;client.cloud=&cloud;client.queue=&queue;
    QQmlPropertyMap theme;
    QVariantMap primary;for(const auto* name:{"durationMid","durationVerySlow","durationFast","durationVeryFast","durationSlow"})primary.insert(name,100);
    theme.insert("Primary",primary);
    theme.insert("Oran7CaptionBar", QVariantMap{{"selectedColor", "#fa8c16"},
        {"hoveredColor", "#ffc069"}, {"iconColorBase-6", "#fef2e8"},
        {"iconColorBase-7", "#fef2e8"}, {"textColorBase-6", "#fef2e8"}});
    QVariantMap colors{{"listViewFontPixelSize",16}};
    for(int i=0;i<15;++i){colors.insert("textColorBase-"+QString::number(i),"#cccccc");colors.insert("colorPrimaryBase-"+QString::number(i),"#888888");}
    colors.insert("colorPrimaryBase-5", "#ff9c6e");
    colors.insert("colorPrimaryBase-7", "#f0541a");
    colors.insert("colorPrimaryBase-8", "#d4380d");
    // 与生产语义主题接口一致；此处仅提供隔离界面的测试配色。
    colors.insert("rowBackgroundColor", "#ede7db");
    colors.insert("rowTextColor", "");
    colors.insert("rowAccentColor", "#22d3ee");
    colors.insert("rowSurfaceLight", "#edf0f3");
    colors.insert("rowSurfaceHoverLight", "#e0e5ea");
    colors.insert("rowSurfaceSelectedLight", "#d7dde3");
    colors.insert("rowTextLight", "#242a31");
    colors.insert("rowSecondaryTextLight", "#58616c");
    colors.insert("rowDetailLight", "#b9c1cb");
    colors.insert("rowAccentLight", "#92751b");
    colors.insert("rowSurfaceDark", "#c7353a40");
    colors.insert("rowSurfaceHoverDark", "#e041474f");
    colors.insert("rowSurfaceSelectedDark", "#eb494f57");
    colors.insert("rowTextDark", "#f0f1f3");
    colors.insert("rowSecondaryTextDark", "#bbc1ca");
    colors.insert("rowDetailDark", "#616a75");
    colors.insert("rowAccentDark", "#ead16c");
    theme.insert("Oran7MusicPlaylistView",colors);
    theme.insert("Oran7MainGUI",QVariantMap{{"isDarkMode",true},{"themeColor","#beb7ff"}});
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Theme",&theme);
    qmlRegisterSingletonInstance("Client",1,0,"Client",&client);
    qmlRegisterUncreatableType<Oran7TrackLibrary>("Client",1,0,"Oran7TrackLibrary","fixture");
    qmlRegisterUncreatableType<Oran7TrackListModel>("Client",1,0,"Oran7TrackListModel","fixture");
    qmlRegisterUncreatableType<Oran7PlaybackState>("Client",1,0,"Oran7PlaybackState","fixture");
    qmlRegisterUncreatableType<Oran7CloudMusicService>("Client",1,0,"Oran7CloudMusicService","fixture");
    qmlRegisterUncreatableType<Oran7PlaybackViewModel>("Client",1,0,"Oran7PlaybackViewModel","fixture");
    const auto fakeAuth=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/tests/playlist/FakeAuth.qml");
    for(const auto* name:{"NcmSession","NcmWebLogin","NcmPlaylistApi"}) qmlRegisterSingletonType(fakeAuth,"Oran7NetEaseAuth",1,0,name);
    qmlRegisterSingletonType(fakeAuth,"Oran7Network",1,0,"Oran7NetworkMonitor");
    qmlRegisterSingletonType(fakeAuth,"Oran7Async",1,0,"Oran7AsyncManager");
    QQmlApplicationEngine engine;QStringList warnings;
    QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError>& list){for(const auto&e:list){warnings<<e.toString();std::fprintf(stderr,"%s\n",qPrintable(e.toString()));}});
    Oran7KeyboardFixture keyboard;
    engine.rootContext()->setContextProperty("globalEventFilter",&keyboard);
    QVariantList songs;for(int i=0;i<20;++i)songs.append(QVariantMap{{"songId",i+1},{"title",QString::fromUtf8("歌曲")+QString::number(i)},{"artist","artist"},{"album","album"},{"timesize","60"},{"cover","qrc:/image/Oran7.jpg"}});
    library.replaceSearchResults(songs);library.replaceLikedTracks({songs[1],songs[3]});
    QObject::connect(&queue,&Oran7PlaybackViewModel::trackRequested,[&](const QVariantMap& fields,bool){state.selectTrack(*Oran7Track::fromVariantMap(fields));state.setState(Oran7PlaybackState::Opening);});
    const auto folder=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Components/Oran7GlobalComponents").toString();
    auto source=QString(R"(
import QtQuick
import QtQuick.Controls
import Client 1.0
import "%1"
Window {
    width:1000;height:680;visible:true;color:"#303038"
    Oran7MusicPlaylistView {
        id: list;objectName:"playlist";anchors.fill:parent
        titleModel:["搜索","收藏"]
        pageListModels:[Client.tracks.search,Client.tracks.liked]
        showLoveButton:true;showIntegrateButton:true
        onItemClicked: function(index) {Client.musicQueue.activate(listModel,index);syncIndicesToCurrentMedia();}
        onItemDoubleClicked: function(index) {Client.musicQueue.activate(listModel,index);syncIndicesToCurrentMedia();}
    }
})").arg(folder);
    engine.loadData(source.toUtf8(),QUrl("qrc:/P17PlaylistFixture.qml"));
    check(!engine.rootObjects().isEmpty(),"production playlist loads");QTest::qWait(120);
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* root=find(window->contentItem(),"playlist");auto* list=find(root,"playlistListView");
    check(list && list->property("count").toInt()==20,"real model delegates instantiate");
    auto* row=find(list,"playlistRow");check(row && !row->property("itemTitle").toString().isEmpty(),"extracted delegate resolves roles");
    auto* cover=find(row,"playlistCover");
    auto* coverContent=find(cover,"roundedImageContent");
    check(find(cover,"roundedImageMeasurer")->property("source").toUrl().isEmpty() &&
              coverContent->property("sourceSize").toSize().width()>0 &&
              coverContent->property("sourceSize").toSize().width()<=80,
          "ordinary cover decodes at display size without loading an original-size measurer");
    check(find(cover,"roundedImageTiles")->childItems().first()->childItems().size()==1,
          "ordinary cover creates no hidden full-resolution tiled images");
    check(root->property("playlistColorMode").toString()=="Light" &&
              root->property("playlistSurface").value<QColor>()==QColor("#ede7db"),
          "ivory background selects readable dark text independently of window theme");
    colors.insert("rowBackgroundColor", "#243648");
    theme.insert("Oran7MusicPlaylistView",colors);
    check(root->property("playlistColorMode").toString()=="Dark" &&
              root->property("playlistText").value<QColor>()==QColor("#f0f1f3"),
          "live custom dark background switches text contrast without restart");
    colors.insert("rowBackgroundColor", "#ede7db");
    theme.insert("Oran7MusicPlaylistView",colors);
    const auto originalSecondary=root->property("playlistSecondaryText").value<QColor>();
    colors.insert("rowTextColor", "#315e77");
    theme.insert("Oran7MusicPlaylistView",colors);
    check(root->property("playlistText").value<QColor>()==QColor("#315e77") &&
              find(row,"playlistSongTitle")->property("color").value<QColor>()==QColor("#315e77") &&
              find(root,"playlistTabLabel0")->property("color").value<QColor>()==QColor("#d4380d") &&
              find(root,"playlistTabLabel1")->property("color").value<QColor>()==QColor("#ff9c6e") &&
              find(root,"playlistHeaderSearchField")->property("color").value<QColor>()==QColor("#315e77") &&
              find(root,"playlistColumnTitle")->property("color").value<QColor>()==QColor("#315e77") &&
              root->property("playlistSecondaryText").value<QColor>()!=originalSecondary,
          "custom text updates rows search and column labels while page titles retain original orange palette");
    colors.insert("rowTextColor", "");
    theme.insert("Oran7MusicPlaylistView",colors);
    check(root->property("playlistText").value<QColor>()==QColor("#242a31") &&
              root->property("playlistSecondaryText").value<QColor>()==QColor("#58616c"),
          "original palette restores exact actual title and metadata colors");
    const auto backgroundBeforeAccent=root->property("playlistSurface");
    const auto textBeforeAccent=root->property("playlistText");
    colors.insert("rowAccentColor", "#a78bfa");
    theme.insert("Oran7MusicPlaylistView",colors);
    check(root->property("playlistAccent").value<QColor>()==QColor("#a78bfa") &&
              root->property("playlistSurface")==backgroundBeforeAccent && root->property("playlistText")==textBeforeAccent,
          "independent accent changes live without recoloring background or text");
    colors.insert("rowAccentColor", "#22d3ee");
    theme.insert("Oran7MusicPlaylistView",colors);
    auto* headerColumns=find(root,"playlistColumns");
    auto* headerSelection=find(root,"playlistSelection");
    colors.insert("listViewFontPixelSize",20);
    theme.insert("Oran7MusicPlaylistView",colors);
    check(find(root,"playlistColumnTitle")->property("font").value<QFont>().pixelSize()==20 &&
              find(root,"playlistColumnAlbum")->property("font").value<QFont>().pixelSize()==20 &&
              find(root,"playlistColumnDuration")->property("font").value<QFont>().pixelSize()==20 &&
              find(root,"playlistSelectAllText")->property("font").value<QFont>().pixelSize()==20 &&
              find(root,"playlistTabLabel0")->property("font").value<QFont>().pixelSize()==24,
          "header and multi-select fonts follow list size while page tabs retain original size");
    colors.insert("listViewFontPixelSize",16);
    theme.insert("Oran7MusicPlaylistView",colors);
    QTest::mouseMove(window,QPoint(5,5));
    QTest::qWait(350); // 等字号恢复引起的布局与滚动静默期结束，再检查普通进入动画。
    const auto titleBeforeHighlight=find(row,"playlistSongTitle")->position();
    auto* highlightBase=find(row,"playlistHighlightBase");
    auto* highlightAccent=find(row,"playlistHighlightAccent");
    const auto highlightEdgeWidth=row->property("highlightEdgeWidth").toDouble();
    QTest::mouseMove(window,row->mapToScene(QPointF(260,row->height()/2)).toPoint());
    QTest::qWait(100);
    const auto highlightTime=row->property("highlightProgress").toDouble();
    const auto accentTime=row->property("highlightAccentTime").toDouble();
    check(highlightTime>0 && highlightTime<0.8 &&
              row->property("highlightEasingPower").toInt()==4 &&
              qAbs(row->property("highlightBaseProgress").toDouble()-(1-std::pow(1-highlightTime,4)))<0.00001 &&
              qAbs(row->property("highlightAccentProgress").toDouble()-(accentTime<0.5
                  ? std::pow(2*accentTime,4)/2 : 1-std::pow(2*(1-accentTime),4)/2))<0.00001 &&
              row->property("highlightBaseProgress").toDouble()>row->property("highlightAccentProgress").toDouble(),
          "quartic ease-out underlay leads the more pronounced ease-in-out surface mask");
    check(highlightBase && highlightAccent && highlightAccent->x()-highlightBase->x()>highlightEdgeWidth+10 &&
              highlightAccent->z()>=highlightBase->z() &&
              highlightBase->parentItem()->childItems().indexOf(highlightAccent)>
                  highlightBase->parentItem()->childItems().indexOf(highlightBase),
          "opaque surface mask trails above the accent leaving an expanded band during entry");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/playlist-highlight-enter.png");
    QTest::qWait(300);
    check(row->property("hoverProgress").toDouble()>0.95,"stationary pointer enters normal smooth hover");
    check(highlightBase && highlightAccent && QTest::qWaitFor([&]{
              return highlightBase->x()<0.01 && qAbs(highlightAccent->x()-highlightEdgeWidth)<0.01;
          },600),
          "settled mask covers the accent except for a narrow slanted leading edge");
    check(find(row,"playlistSongTitle")->position()==titleBeforeHighlight,
          "highlight motion leaves song title geometry fixed");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/playlist-hover.png");
    QTest::mouseMove(window,QPoint(5,5));
    check(QTest::qWaitFor([&]{
              return highlightBase->x()>0 && highlightAccent->x()-highlightBase->x()>highlightEdgeWidth+10 &&
                  row->property("highlightProgress").toDouble()>0;
          },250),
          "exit uncovers a widening accent trail as the surface mask retracts first");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/playlist-highlight-exit.png");
    QTest::mouseMove(window,row->mapToScene(QPointF(260,row->height()/2)).toPoint());
    check(QTest::qWaitFor([&]{return qAbs(highlightAccent->x()-highlightEdgeWidth)<0.01;},600),
          "re-entering mid-exit reverses and fully opens the highlight");
    root->setProperty("curSelectingIndex",row->property("itemIndex"));
    QTest::mouseMove(window,QPoint(5,5));
    QTest::qWait(450);
    check(row->property("hoverProgress").toDouble()<0.01 && qAbs(highlightAccent->x()-highlightEdgeWidth)<0.01,
          "selected track retains the accent panel after pointer leaves");
    root->setProperty("curSelectingIndex",-1);
    QTest::qWait(350);
    check(!find(row,"playlistHighlightViewport")->isVisible(),"deselecting retracts and hides both layers");
    root->setProperty("isMultiSelected",true);
    library.search()->setSelected(row->property("itemIndex").toInt(),true);
    QTest::qWait(450);
    check(qAbs(highlightAccent->x()-highlightEdgeWidth)<0.01,"multi-selection uses the same persistent accent panel");
    root->setProperty("isMultiSelected",false);
    QTest::qWait(350);
    QTest::mouseMove(window,row->mapToScene(QPointF(260,row->height()/2)).toPoint());
    QTest::qWait(450);
    const auto staticSurface=row->property("surfaceColor");
    const auto initialContentY=list->property("contentY").toDouble();
    const auto pointerInList=list->mapFromScene(row->mapToScene(QPointF(260,row->height()/2)));
    list->setProperty("contentY",initialContentY+4);
    check(row->property("hovered").toBool() && row->property("hoverProgress").toDouble()>0.99 &&
              row->property("highlightProgress").toDouble()>0.99,
          "small scroll preserves highlight when pointer remains over the same track");
    const auto beforeWheel=list->property("contentY").toDouble();
    const auto wheelPosition=list->mapToScene(pointerInList);
    QWheelEvent wheel(wheelPosition,window->mapToGlobal(wheelPosition),QPoint(),QPoint(0,-8),
        Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false);
    QCoreApplication::sendEvent(window,&wheel);
    QTest::qWait(30);
    check(list->property("contentY").toDouble()>beforeWheel && row->property("hovered").toBool() &&
              row->property("highlightProgress").toDouble()>0.99,
          "real wheel input scrolls without clearing the track still under the mouse");
    QWheelEvent wheelEnd(wheelPosition,window->mapToGlobal(wheelPosition),QPoint(),QPoint(),
        Qt::NoButton,Qt::NoModifier,Qt::ScrollEnd,false,Qt::MouseEventSynthesizedBySystem);
    QCoreApplication::sendEvent(window,&wheelEnd);
    QMetaObject::invokeMethod(list,"cancelFlick");
    list->setProperty("contentY",initialContentY+row->height()+root->property("listColumSpacing").toDouble());
    QQuickItem* nextHoveredRow=nullptr;
    QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,nextHoveredRow),Q_ARG(int,1));
    check(root->property("curHoveredIndex").toInt()==1 && !row->property("hovered").toBool() &&
              nextHoveredRow && nextHoveredRow->property("hovered").toBool() &&
              nextHoveredRow->property("highlightProgress").toDouble()>0.99,
          "stationary pointer follows the actual row as content scrolls without replaying slide-ins");
    check(QMetaObject::invokeMethod(list,"flick",Q_ARG(qreal,0.0),Q_ARG(qreal,-700.0)),"start actual list flick");
    QTest::qWait(60);
    check(list->property("hoverSuspended").toBool(),"flick suspends transitions while preserving hover hit-testing");
    int expectedHovered=-1;
    QMetaObject::invokeMethod(list,"indexAt",Q_RETURN_ARG(int,expectedHovered),
        Q_ARG(qreal,pointerInList.x()+list->property("contentX").toDouble()),
        Q_ARG(qreal,pointerInList.y()+list->property("contentY").toDouble()));
    QQuickItem* flickHoveredRow=nullptr;
    QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,flickHoveredRow),Q_ARG(int,expectedHovered));
    check(expectedHovered>=0 && root->property("curHoveredIndex").toInt()==expectedHovered && flickHoveredRow &&
              flickHoveredRow->property("highlightProgress").toDouble()>0.99 &&
              flickHoveredRow->property("hoverProgress").toDouble()>0.99,
          "actual flick keeps the row under the stationary pointer fully highlighted");
    check(row->property("surfaceColor")==staticSurface,"flick leaves static gradient unchanged");
    QMetaObject::invokeMethod(list,"cancelFlick");
    list->setProperty("contentY",0);
    QTest::qWait(450);
    check(!list->property("hoverSuspended").toBool(),"hover resumes after list settles");
    check(root->property("curHoveredIndex").toInt()==0 && row->property("highlightProgress").toDouble()>0.99,
          "ending scroll leaves the actual hovered track highlighted");
    QTest::mouseMove(window,row->mapToScene(QPointF(260,row->height()+root->property("listColumSpacing").toDouble()/2)).toPoint());
    QTest::qWait(350);
    check(root->property("curHoveredIndex").toInt()==-1,
          "moving pointer into row spacing clears hover instead of sticking to previous track");
    QTest::mouseMove(window,QPoint(5,5));
    QTest::qWait(350);
    check(root->property("curHoveredIndex").toInt()==-1 && row->property("highlightProgress").toDouble()<0.01,
          "leaving playlist clears hover and retracts highlight normally");
    check(QMetaObject::invokeMethod(root,"itemClicked",Q_ARG(int,3)),"play intent emitted");QTest::qWait(10);
    check(state.currentTrack().songId==4 && root->property("curSelectingIndex").toInt()==3,"click synchronizes metadata and selection");
    check(QMetaObject::invokeMethod(root,"switchToPage",Q_ARG(QVariant,1)),"switch page");QTest::qWait(10);
    check(list->property("count").toInt()==2 && root->property("curSelectingIndex").toInt()==1,"page switch preserves identity focus");
    QMetaObject::invokeMethod(root,"switchToPage",Q_ARG(QVariant,0));QTest::qWait(10);
    const auto listYBeforeMulti=list->y();
    root->setProperty("isMultiSelected",true);auto* selection=find(root,"playlistSelection");selection->setProperty("checked",true);
    check(headerSelection->position()==headerColumns->position() && headerSelection->size()==headerColumns->size() &&
              headerSelection->property("radius")==headerColumns->property("radius") &&
              headerSelection->property("color")==headerColumns->property("color") && list->y()==listYBeforeMulti,
          "multi-select header shares exact normal header geometry color radius without shifting rows");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/playlist-multi-header.png");
    check(library.search()->get(10).value("isSelected").toBool(),"extracted select all");
    auto* reorder=find(root,"playlistReorder");
    QQuickItem* sourceRow=nullptr;
    QQuickItem* targetRow=nullptr;
    check(QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,sourceRow),Q_ARG(int,3)) && sourceRow,
          "source row exists for pointer drag");
    check(QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,targetRow),Q_ARG(int,0)) && targetRow,
          "target row exists for pointer drag");
    auto* handle=sourceRow->property("dragHandleItem").value<QQuickItem*>();
    check(handle && handle->isVisible(),"multi-select exposes visible drag handle");
    const auto dragStart=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
    const auto dragEnd=targetRow->mapToScene(QPointF(targetRow->width()-60,3)).toPoint();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,dragStart);
    check(root->property("dragActive").toBool() && root->property("dragSourceIndex").toInt()==3,
          "pressing actual visible handle starts drag");
    QTest::mouseMove(window,dragEnd,30);
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,dragEnd);
    QTest::qWait(10);
    check(!root->property("dragActive").toBool() && list->property("interactive").toBool(),
          "release ends pointer drag and restores scrolling");
    check(library.search()->get(0).value("songId")==4 && root->property("curSelectingIndex").toInt()==0,"reorder keeps playback selection");
    window->setWidth(700);
    list->setProperty("contentY",180);
    QTest::qWait(60);
    sourceRow=nullptr;targetRow=nullptr;
    QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,sourceRow),Q_ARG(int,6));
    QMetaObject::invokeMethod(list,"itemAtIndex",Q_RETURN_ARG(QQuickItem*,targetRow),Q_ARG(int,4));
    check(sourceRow && targetRow,"scrolled rows exist after window resize");
    const auto checkboxPos=sourceRow->mapToScene(QPointF(20,sourceRow->height()/2)).toPoint();
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,checkboxPos);
    check(!library.search()->get(6).value("isSelected").toBool() && !root->property("dragActive").toBool(),
          "checkbox toggles selection without starting drag");
    const auto movedSong=library.search()->get(6).value("songId");
    handle=sourceRow->property("dragHandleItem").value<QQuickItem*>();
    const auto resizedStart=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
    const auto resizedEnd=targetRow->mapToScene(QPointF(targetRow->width()-60,3)).toPoint();
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,resizedStart);
    check(root->property("dragActive").toBool() && root->property("dragSourceIndex").toInt()==6,
          "resized and scrolled handle starts correct row drag");
    QTest::mouseMove(window,resizedEnd,30);
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,resizedEnd);
    QTest::qWait(10);
    check(library.search()->get(4).value("songId")==movedSong &&
              !library.search()->get(4).value("isSelected").toBool(),
          "pointer reorder after resize preserves track and selection");
    window->setWidth(1000);
    list->setProperty("contentY",0);
    root->setProperty("isMultiSelected",false);check(!library.search()->get(10).value("isSelected").toBool(),"clear multi selection");
    QTest::qWait(350);
    row=find(list,"playlistRow");
    check(row && row->opacity()>0.9 && row->height()>0 && row->width()>0,"rows remain visible after reorder animation");
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/playlist.png");
    auto* header=find(root,"playlistHeader");
    auto* tabs=find(root,"playlistHeaderTabs");
    auto* searchRect=find(root,"playlistHeaderSearchRect");
    auto* searchField=find(root,"playlistHeaderSearchField");
    auto* multiButton=find(root,"playlistHeaderMultiSelect");
    check(tabs->y()==0 && multiButton->width()==40 && multiButton->y()==4 &&
              multiButton->x()+multiButton->width()==header->width()-30 && searchRect->width()==60,
          "original tab search and multi-select geometry restored");
    window->setWidth(620);
    QTest::qWait(60);
    check(tabs->y()==0 && multiButton->y()==4 &&
              multiButton->x()+multiButton->width()==header->width()-30,
          "original header remains in its original row after resizing");
    searchField->forceActiveFocus();
    QTest::qWait(300);
    check(searchRect->width()==120,"original focus-triggered search expansion restored");
    searchField->setFocus(false);
    QTest::qWait(300);
    check(searchRect->width()==60,"original empty search collapse restored");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                     multiButton->mapToScene(QPointF(multiButton->width()/2,multiButton->height()/2)).toPoint());
    check(root->property("isMultiSelected").toBool(),"original header multi-select button remains clickable");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,
                     multiButton->mapToScene(QPointF(multiButton->width()/2,multiButton->height()/2)).toPoint());
    check(!root->property("isMultiSelected").toBool(),"original header button exits multi-select");
    window->setWidth(1000);
    const auto pages=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/RightPage/MainStackView").toString();
    auto pagesSource=QString(R"(
import QtQuick
import "%1"
Window { width:1000;height:680;visible:true
    LocalMusicStack { anchors.fill:parent }
    SearchMusicStack { objectName:"cloudSearchPage"; anchors.fill:parent }
})").arg(pages);
    engine.loadData(pagesSource.toUtf8(),QUrl("qrc:/P17PagesFixture.qml"));QTest::qWait(120);
    check(engine.rootObjects().size()==2,"production local/search pages load without playback strategy replicas");
    auto* pagesWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* cloudPage=find(pagesWindow->contentItem(),"cloudSearchPage");
    auto* cloudList=find(cloudPage,"cloudMusicPlaylist");
    check(cloudList && cloudList->property("currentPageIndex").toInt()==0 &&
              cloudList->property("listModel").value<QObject*>()==library.liked(),
          "cloud search page defaults to liked list after tab swap");
    check(QMetaObject::invokeMethod(cloudPage,"searchTriggered",Q_ARG(QString,QStringLiteral("fixture"))),
          "search intent submitted through actual cloud page");
    check(cloudList->property("currentPageIndex").toInt()==1 &&
              cloudList->property("listModel").value<QObject*>()==library.search(),
          "new search switches to search results at second tab");
    QMetaObject::invokeMethod(cloudList,"switchToPage",Q_ARG(QVariant,0));
    check(cloudList->property("listModel").value<QObject*>()==library.liked(),
          "first tab returns to liked model after search");
    // 加载真实导航栏，验证选中/悬停双滑块独立移动、快速改目标与折叠布局。
    const auto navigationFolder=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/LeftPage").toString();
    const auto basicFolder=QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Basic").toString();
    auto navigationSource=QString(R"(
import QtQuick
import "%1"
import "%2"
Window {
    width:360;height:650;visible:true;color:"#20262e"
    property int localPageRequests: 0
    property int qqPageRequests: 0
    function focusNavigation(name) { BasicConfig.focusCurrent_SelectedMenuModel(name) }
    Connections {
        target: BasicConfig
        function onPushLocalMusicStackInto_RightPageMainStackView() { localPageRequests++ }
        function onPushQQMusicStackInto_RightPageMainStackView() { qqPageRequests++ }
    }
    LeftPage { width:simpleMode ? simpleModeWidth : defaultWidth;height:parent.height;color:"transparent" }
})").arg(navigationFolder,basicFolder);
    engine.loadData(navigationSource.toUtf8(),QUrl("qrc:/NavigationFixture.qml"));
    check(engine.rootObjects().size()==3,"production navigation loads in an isolated window");
    auto* navigationWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* navigation=find(navigationWindow->contentItem(),"__CaptionBar__");
    auto* highlight=find(navigation,"navigationSelectedHighlight");
    auto* hoverHighlight=find(navigation,"navigationHoverHighlight");
    auto* firstNavigation=find(navigation,"navigationItem0");
    auto* localNavigation=find(navigation,"navigationItem4");
    auto* searchNavigation=find(navigation,"navigationItem5");
    QTest::mouseMove(navigationWindow,QPoint(300,600));
    QTest::qWait(400);
    const auto navigationY=[&](QQuickItem* item) { return item->mapToItem(navigation,QPointF()).y(); };
    check(highlight && hoverHighlight && highlight!=hoverHighlight &&
              qAbs(highlight->y()-navigationY(firstNavigation))<0.5 && hoverHighlight->opacity()<0.01 &&
              highlight->property("accentColor").value<QColor>().lightnessF()<
                  hoverHighlight->property("accentColor").value<QColor>().lightnessF(),
          "independent selected slider is darker and hover slider starts hidden");
    const auto localCenter=localNavigation->mapToScene(QPointF(80,21)).toPoint();
    QTest::mouseMove(navigationWindow,localCenter);
    QTest::qWait(80);
    check(hoverHighlight->y()>navigationY(firstNavigation) && hoverHighlight->y()<navigationY(localNavigation) &&
              hoverHighlight->opacity()>0 && qAbs(highlight->y()-navigationY(firstNavigation))<0.5 &&
              navigation->property("itemSelected_index").toInt()==0,
          "hover slider animates across groups while the selected slider stays on the current page");
    QTest::mouseMove(navigationWindow,searchNavigation->mapToScene(QPointF(80,21)).toPoint());
    QTest::qWait(400);
    check(qAbs(hoverHighlight->y()-navigationY(searchNavigation))<0.5 &&
              hoverHighlight==find(navigation,"navigationHoverHighlight") &&
              qAbs(highlight->y()-navigationY(firstNavigation))<0.5,
          "rapid hover retargets only the shared hover slider");
    navigationWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/navigation-two-sliders.png");
    QTest::mouseMove(navigationWindow,QPoint(300,600));
    QTest::qWait(400);
    check(navigation->property("itemHovered_index").toInt()==-1 &&
              qAbs(highlight->y()-navigationY(firstNavigation))<0.5 && hoverHighlight->opacity()<0.01 &&
              qAbs(hoverHighlight->y()-navigationY(searchNavigation))<0.5,
          "leaving navigation fades hover at its last position without moving selection");
    QTest::mouseMove(navigationWindow,localCenter);
    QTest::mouseClick(navigationWindow,Qt::LeftButton,Qt::NoModifier,localCenter);
    QTest::qWait(400);
    check(navigationWindow->property("localPageRequests").toInt()==1 &&
              navigation->property("itemSelected_index").toInt()==4 &&
              qAbs(highlight->y()-navigationY(localNavigation))<0.5 && hoverHighlight->opacity()<0.01,
          "click moves selected slider and suppresses overlapping hover without changing the page command");
    QTest::mouseMove(navigationWindow,QPoint(300,600));
    check(QMetaObject::invokeMethod(navigationWindow,"focusNavigation",Q_ARG(QVariant,"SearchMusicPage")),
          "external page focus signal reaches production navigation");
    QTest::qWait(400);
    check(navigation->property("itemSelected_index").toInt()==5 &&
              qAbs(highlight->y()-navigationY(searchNavigation))<0.5,
          "page changes outside navigation also move the selected slider");
    navigationWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/navigation-expanded.png");
    navigation->setProperty("simpleMode",true);
    QTest::qWait(400);
    check(qAbs(highlight->width()-42)<0.5 &&
              qAbs(highlight->x()-searchNavigation->mapToItem(navigation,QPointF()).x())<0.5 &&
              qAbs(highlight->y()-navigationY(searchNavigation))<0.5,
          "compact icon mode keeps the selected slider aligned during width changes");
    QTest::mouseMove(navigationWindow,firstNavigation->mapToScene(QPointF(21,21)).toPoint());
    QTest::qWait(400);
    check(navigation->property("itemHovered_index").toInt()==0 &&
              qAbs(hoverHighlight->width()-42)<0.5 &&
              qAbs(hoverHighlight->y()-navigationY(firstNavigation))<0.5 &&
              qAbs(highlight->y()-navigationY(searchNavigation))<0.5,
          "compact navigation independently aligns both hover and selected sliders");
    navigationWindow->hide();
    QTest::qWait(220);
    check(navigation->property("itemHovered_index").toInt()==-1 &&
              navigation->property("itemSelected_index").toInt()==5 && hoverHighlight->opacity()<0.01,
          "hiding navigation clears transient hover and preserves page selection");
    library.replaceRecommendations({songs[3],songs[7]});
    QMetaObject::invokeMethod(&queue,"recommendationActivated");
    QTest::qWait(40);
    check(cloudList->property("currentPageIndex").toInt()==2 &&
              cloudList->property("listModel").value<QObject*>()==library.recommendations(),
          "recommendation navigation focuses its dedicated model without replacing liked or search");
    const auto choiceSource=QString(R"(
import QtQuick
import Client 1.0
import "%1"
Window { width:500;height:500;visible:true;color:"#20262e"
    Oran7RecommendationButton { x:340;y:450;queue:Client.musicQueue;mode:Client.playback.mode }
})").arg(folder);
    engine.loadData(choiceSource.toUtf8(),QUrl("qrc:/RecommendationFixture.qml"));
    check(engine.rootObjects().size()==4,"production recommendation selector loads outside settings GUI");
    auto* choiceWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* choice=find(choiceWindow->contentItem(),"recommendationButton");
    QTest::mouseClick(choiceWindow,Qt::LeftButton,Qt::NoModifier,QPoint(380,466));
    QTest::qWait(260);
    auto* menu=choice->findChild<QObject*>("recommendationPopup");
    auto* fmOption=find(choiceWindow->contentItem(),"recommendationOption4");
    check(menu && menu->property("visible").toBool() && fmOption,"recommendation menu opens with a personal FM option");
    choiceWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/recommendation-menu.png");
    QTest::mouseClick(choiceWindow,Qt::LeftButton,Qt::NoModifier,fmOption->mapToScene(QPointF(70,25)).toPoint());
    QTest::qWait(30);
    check(menu->property("visible").toBool() && !queue.recommendationError().isEmpty(),
          "unauthenticated recommendation shows feedback without changing mode or closing menu");
    QTest::keyClick(choiceWindow,Qt::Key_Escape);
    QTest::qWait(160);
    check(!menu->property("visible").toBool(),"recommendation menu respects Escape dismissal");
    // QQ 页面先接通界面；空服务不得触发网易云请求或伪造账号/曲库。
    const auto neteaseSearchBeforeQQ=library.search()->serializedTracks();
    const auto neteaseLikedBeforeQQ=library.liked()->serializedTracks();
    const auto qqSource=QString(R"(
import QtQuick
import "%1"
import "%2"
Window {
    width:1000;height:680;visible:true;color:"#303038"
    property int searchRequests: 0
    property int loginRequests: 0
    property string lastQuery: ""
    readonly property var activeAccountPopup: BasicConfig.activeMusicAccountPopup
    Oran7QQMusicStack {
        objectName:"qqMusicPage"; anchors.fill:parent
        onSearchRequested: function(keywords) { searchRequests++; lastQuery=keywords }
        onLoginRequested: loginRequests++
    }
})").arg(pages,basicFolder);
    engine.loadData(qqSource.toUtf8(),QUrl("qrc:/QQMusicUiFixture.qml"));
    check(engine.rootObjects().size()==5,"QQ music page loads without a QQ backend or extra QML module");
    auto* qqWindow=qobject_cast<QQuickWindow*>(engine.rootObjects().last());
    auto* qqPage=find(qqWindow->contentItem(),"qqMusicPage");
    auto* qqList=find(qqPage,"qqMusicPlaylist");
    auto* qqSearch=find(qqPage,"qqMusicSearchField");
    auto* qqAccount=find(qqPage,"qqMusicAccountButton");
    auto* qqPopup=qqPage->findChild<QObject*>("qqMusicLoginPopup");
    QTest::qWait(180);
    check(qqList && qqSearch && qqAccount && qqPopup
        && !qqList->property("listModel").value<QObject*>()
        && qqList->property("currentPageIndex").toInt()==0,
        "QQ page starts on its own empty liked tab without borrowing NetEase data");
    check(!find(qqPage,"qqMusicRefreshButton")->isEnabled(),"unconnected service cannot submit a refresh");
    qqSearch->setProperty("text",QString::fromUtf8("  测试歌曲  "));
    QMetaObject::invokeMethod(qqPage,"submitSearch");
    check(qqList->property("currentPageIndex").toInt()==1
        && qqWindow->property("searchRequests").toInt()==0
        && !qqPage->property("_feedback").toString().isEmpty(),
        "search switches tabs and gives honest feedback while the service is disconnected");
    qqPage->setProperty("serviceConnected",true);
    QMetaObject::invokeMethod(qqPage,"submitSearch");
    check(qqWindow->property("searchRequests").toInt()==1
        && qqWindow->property("lastQuery").toString()==QString::fromUtf8("测试歌曲"),
        "connected UI emits a trimmed search intent through its own boundary");
    qqPage->setProperty("searchBusy",true);
    QMetaObject::invokeMethod(qqPage,"submitSearch");
    check(qqWindow->property("searchRequests").toInt()==1,"busy search ignores duplicate submission");
    qqPage->setProperty("searchBusy",false);
    qqPage->setProperty("serviceConnected",false);
    qqWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/qq-music-page.png");
    QTest::mouseClick(qqWindow,Qt::LeftButton,Qt::NoModifier,qqAccount->mapToScene(QPointF(20,19)).toPoint());
    QTest::qWait(280);
    auto* qqLoginStart=find(qqWindow->contentItem(),"qqMusicLoginStart");
    check(qqPopup->property("visible").toBool() && qqLoginStart && !qqLoginStart->isEnabled()
        && qqWindow->property("activeAccountPopup").value<QObject*>()==qqPopup
        && qqWindow->property("loginRequests").toInt()==0,
        "account button opens official-scan instructions without pretending login is available");
    qqWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/qq-music-login.png");
    qqPage->setProperty("serviceConnected",true);
    QTest::mouseClick(qqWindow,Qt::LeftButton,Qt::NoModifier,qqLoginStart->mapToScene(QPointF(40,20)).toPoint());
    check(qqWindow->property("loginRequests").toInt()==1,"login click reaches QQ-specific intent");
    QTest::keyClick(qqWindow,Qt::Key_Escape);
    QTest::qWait(180);
    check(!qqPopup->property("visible").toBool()
        && !qqWindow->property("activeAccountPopup").value<QObject*>(),"QQ login popup closes on Escape and releases its borrowed handle");
    qqWindow->setWidth(420);
    QTest::qWait(50);
    check(qqSearch->width()>60 && qqAccount->width()>=40
        && qqAccount->mapToScene(QPointF(qqAccount->width(),0)).x()<=qqWindow->width(),
        "QQ search and compact login control stay inside a narrow page");
    qqWindow->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/qq-music-narrow.png");
    Oran7TrackListModel qqFixtureLiked,qqFixtureSearch;
    qqFixtureSearch.replaceTracks({QVariantMap{{"path","qqmusic://song/fixture"},{"title","fixture"}}});
    qqPage->setProperty("likedModel",QVariant::fromValue(&qqFixtureLiked));
    qqPage->setProperty("searchModel",QVariant::fromValue(&qqFixtureSearch));
    QTest::qWait(50);
    check(qqList->property("listModel").value<QObject*>()==&qqFixtureSearch,
        "late-injected QQ search model updates the active tab");
    QMetaObject::invokeMethod(qqList,"switchToPage",Q_ARG(QVariant,0));
    check(qqList->property("listModel").value<QObject*>()==&qqFixtureLiked
        && library.search()->serializedTracks()==neteaseSearchBeforeQQ
        && library.liked()->serializedTracks()==neteaseLikedBeforeQQ,
        "switching QQ tabs keeps independent source bindings and preserves NetEase models");
    navigationWindow->show();
    auto* qqNavigation=find(navigation,"navigationItem6");
    check(qqNavigation,"QQ entry exists in the shared-slider navigation");
    QTest::mouseClick(navigationWindow,Qt::LeftButton,Qt::NoModifier,
        qqNavigation->mapToScene(QPointF(21,21)).toPoint());
    QTest::qWait(400);
    check(navigationWindow->property("qqPageRequests").toInt()==1
        && navigation->property("itemSelected_index").toInt()==6,
        "QQ navigation emits its own page request and moves the selected slider");
    // 页面应先于被借用的模型释放。
    qqPage->setProperty("likedModel",QVariant::fromValue<QObject*>(nullptr));
    qqPage->setProperty("searchModel",QVariant::fromValue<QObject*>(nullptr));
    check(warnings.isEmpty(),"no undefined roles/anchors/binding loops");
    std::printf("PASS: %d production QML checks\n",count);
}
#include "qml.moc"
