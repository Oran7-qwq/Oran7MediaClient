#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlAbstractUrlInterceptor>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFontDatabase>
#include <QSignalSpy>
#include <QTest>
#include <cstdio>
#include <cstdlib>

class Oran7WindowAgentFixture : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE void setup(QObject*) {}
    Q_INVOKABLE void setWindowAttribute(const QString&, const QVariant&) {}
};
class Oran7SoundFixture : public QObject {
    Q_OBJECT
    Q_PROPERTY(qreal volume MEMBER m_volume)
public:
    qreal m_volume = 0;
    Q_INVOKABLE void playOpen() {}
    Q_INVOKABLE void playClose() {}
};
class Oran7KeyboardFixture : public QObject {
    Q_OBJECT
signals:
    void escapeKeyPressed();
    void upKeyPressed(); void upKeyReleased();
    void downKeyPressed(); void downKeyReleased();
    void leftKeyPressed(); void leftKeyReleased();
    void rightKeyPressed(); void rightKeyReleased();
};
class Oran7PanelInterceptor : public QQmlAbstractUrlInterceptor {
public:
    QUrl intercept(const QUrl& url, DataType type) override {
        if (type == QmlFile && (url.path().endsWith("/Oran7MainUiSettingWindow.qml")
            || url.path().endsWith("/Oran7MusicPlayerSettingWindow.qml")
            || url.path().endsWith("/Oran7VideoPlayerSettingWindow.qml")))
            return QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/tests/settings-animation/StubPanel.qml");
        return url;
    }
};
static int checks = 0;
static void check(bool ok, const char* label) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1); } ++checks; }
template<class F> static void until(F condition) {
    QElapsedTimer elapsed; elapsed.start();
    while (!condition() && elapsed.elapsed() < 4500) QTest::qWait(3);
    check(condition(),"transition reached expected phase");
}
static QString phase(QObject* object) { return object->property("transitionState").toString(); }
static void invoke(QObject* object, const char* method) { check(QMetaObject::invokeMethod(object,method),method); }

int main(int argc,char**argv) {
    qputenv("QT_QPA_PLATFORM","offscreen");qputenv("QT_QUICK_BACKEND","software");qputenv("QT_QUICK_CONTROLS_STYLE","Fusion");
    QGuiApplication app(argc,argv);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc");QFont::insertSubstitution(QString::fromUtf8("微软雅黑"),"Microsoft YaHei");
    QQmlPropertyMap theme;
    theme.insert("isDark",true);
    theme.insert("Primary",QVariantMap{{"durationMid",100}});
    theme.insert("Oran7MainGUI",QVariantMap{{"themeColor","#beb7ff"},{"isDarkMode",true},
        {"settingWinItemDefalutWidth",200},{"soundEffectVolume",0}});
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Theme",&theme);
    qmlRegisterType<Oran7WindowAgentFixture>("Oran7UI.Impl",1,0,"Oran7WindowAgent");
    qmlRegisterType<Oran7SoundFixture>("Oran7Sound",1,0,"Oran7SoundEffect");
    const auto settingsId=qmlRegisterSingletonType(QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Settings/GlobalSettings/Oran7MainUiSetting.qml"),"Oran7MainUiSetting",1,0,"Oran7MainUiSetting");
    Oran7PanelInterceptor interceptor; QQmlApplicationEngine engine; engine.addUrlInterceptor(&interceptor);
    Oran7KeyboardFixture keyboard;engine.rootContext()->setContextProperty("globalEventFilter",&keyboard);
    QStringList warnings;
    QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError>& list){for(const auto& e:list){warnings<<e.toString();std::fprintf(stderr,"%s\n",qPrintable(e.toString()));}});
    engine.load(QUrl::fromLocalFile(QStringLiteral(APP_ROOT)+"/src/qml/Settings/SettingWindows/Oran7SettingsContainerWindow.qml"));
    check(!engine.rootObjects().isEmpty(),"production container loads with native/audio fixtures");
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* settings=engine.singletonInstance<QObject*>(settingsId);check(window&&settings,"window and settings singleton");
    window->setProperty("enable__dwmBlur__",false); settings->setProperty("toggleOpenAniDuration",100);
    QSignalSpy restored(window,SIGNAL(restoreMainWindowFocusRequested()));
    check(!window->isVisible()&&phase(window)=="Closed","initial window hidden");
    invoke(settings,"callOpenSettingWindow");
    until([&]{return phase(window)=="Preparing"&&window->isVisible();});
    check(window->opacity()==0 && !settings->property("settingContentVisible").toBool(),"preparation never exposes stale native frame");
    auto panels=window->property("panels").value<QJSValue>().toVariant().toList();
    check(panels.size()==3,"three loaded panels");
    QList<QQuickItem*> items;
    for(const auto& entry:panels) {
        auto* item=qobject_cast<QQuickItem*>(entry.value<QObject*>());check(item,"panel uses production transition base");items.append(item);
        check(item->height()==0&&!item->isVisible(),"panel reset while container is transparent");
    }
    invoke(window,"handle_OpenSettingWin_signal");check(phase(window)=="Preparing","preparation rejects repeated toggle");
    until([&]{return phase(items[0])=="Open";});
    check(phase(window)=="Opening"&&settings->property("settingWindow_isOpening_Or_isClosing").toBool(),"first opened panel does not unlock other animations");
    until([&]{return phase(window)=="Open";});
    check(window->opacity()==1&&window->property("blankFrameReady").toBool(),"opening starts after actual blank frame");
    check(!settings->property("settingWindow_isOpening_Or_isClosing").toBool(),"all-open completion unlocks input");
    QQmlComponent popupFixture(&engine);
    popupFixture.setData("import QtQml\nQtObject { property bool dismissed: false; function close() { dismissed = true } }",QUrl());
    auto* activePopup=popupFixture.create();
    check(activePopup,"active settings popup fixture");
    settings->setProperty("activeSettingsPopup",QVariant::fromValue(activePopup));
    keyboard.escapeKeyPressed();QTest::qWait(240);
    check(phase(window)=="Open"&&activePopup->property("dismissed").toBool(),"ESC dismisses active color menu without toggling container");
    settings->setProperty("activeSettingsPopup",QVariant::fromValue(static_cast<QObject*>(nullptr)));
    delete activePopup;
    items[0]->setProperty("preferencesPopupOpen",true);
    keyboard.escapeKeyPressed();QTest::qWait(240);
    check(phase(window)=="Open"&&!items[0]->property("preferencesPopupOpen").toBool(),"ESC dismisses preference menu without scheduling container toggle");
    keyboard.escapeKeyPressed();
    until([&]{return phase(window)=="Closing";});
    until([&]{return phase(items[0])=="Closed";});
    check(window->isVisible()&&phase(window)=="Closing"&&phase(items[1])!="Closed","fast panel cannot hide slower panels");
    until([&]{return phase(window)=="Closed";});
    check(!window->isVisible()&&restored.count()==1,"container hides once after all panels close");
    for(auto* panel:items)check(panel->height()==0&&panel->opacity()==0&&!panel->isVisible(),"all panels fully reset before hiding");
    items[0]->setProperty("savedNormalHeight",520);
    check(items[0]->height()==0,"closed content size cannot restore old height");
    // 重开期间每个真实渲染帧都检查打开进度不倒退，覆盖先大后归零的现象。
    bool backwards=false; qreal previous=0;
    auto sample=QObject::connect(window,&QQuickWindow::frameSwapped,[&]{
        if(phase(window)=="Opening") { const auto current=items[0]->height(); if(current+0.1<previous)backwards=true;previous=current; }
    });
    invoke(settings,"callOpenSettingWindow");until([&]{return phase(window)=="Preparing";});
    check(window->opacity()==0&&items[0]->height()==0,"reopen does not reveal retained height");
    until([&]{return phase(window)=="Open";});
    QObject::disconnect(sample);check(!backwards&&items[0]->height()==520,"reopen height grows without resetting backwards");
    invoke(window,"closeAllPanels");until([&]{return phase(window)=="Closed";});
    check(restored.count()==2,"second close restores focus exactly once");
    settings->setProperty("toggleOpenAniDuration",0);
    invoke(window,"openAllPanels");until([&]{return phase(window)=="Open";});
    invoke(window,"closeAllPanels");until([&]{return phase(window)=="Closed";});
    check(!settings->property("settingWindow_isOpening_Or_isClosing").toBool(),"zero duration has no stuck busy flag");
    invoke(window,"openAllPanels");settings->setProperty("openSettingWin",false);
    until([&]{return phase(window)=="Closed";});
    QTest::qWait(160);check(!window->isVisible()&&phase(window)=="Closed","cancel preparation cannot be reopened by stale timers");
    check(warnings.isEmpty(),"no QML warnings or binding loops");
    std::printf("PASS: %d settings container and animation checks\n",checks);
}
#include "main.moc"
