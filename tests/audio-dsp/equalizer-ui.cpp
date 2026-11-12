#include "Oran7AudioEffects.h"
#include "Oran7Api.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QFile>
#include <QPointer>
#include <QSignalSpy>
#include <QFontDatabase>
#include <QTest>
#include <cstdio>
#include <cstdlib>

static void check(bool ok,const char* text) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",text); std::exit(1); } }
static QQuickItem* find(QQuickItem* item,const QString& name) {
    if (item->objectName()==name) return item;
    for (auto* child:item->childItems()) if (auto* result=find(child,name)) return result;
    return nullptr;
}
int main(int argc,char** argv) {
    QGuiApplication app(argc,argv);
    QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR","C:/Windows")+"/Fonts/msyh.ttc");
    app.setFont(QFont("Microsoft YaHei"));
    Oran7AudioEffects effects;
    int publications=0;
    effects.setPublisher([&](const Oran7AudioParameters&){++publications;return true;});
    const QString presetId=effects.saveCurrentAsPreset(QStringLiteral("测试预设"));
    QQmlPropertyMap settings, theme;
    settings.insert("textColor",QColor("#e4e6ed")); settings.insert("backColor",QColor("#202535"));
    settings.insert("itemBackColor",QColor("#2c3040")); settings.insert("fontFamily","Microsoft YaHei");
    settings.insert("isDarkMode",true); settings.insert("itemHeight",30);
    settings.insert("activeSettingsPopup",QVariant::fromValue<QObject*>(nullptr)); settings.insert("activeOtherItemCount",0);
    theme.insert("Oran7MainGUI",QVariantMap{{"themeColor","#beb7ff"}});
    theme.insert("Primary",QVariantMap{{"durationMid",100}});
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Theme",&theme);
    qmlRegisterSingletonInstance("Oran7UI.Impl",1,0,"Oran7Api",Oran7Api::instance());
    QTemporaryDir temp;
    for (const auto* name:{"Oran7EqualizerWindow.qml","Oran7EqualizerBand.qml","Oran7StudioButton.qml",
                          "Oran7PreferenceAction.qml","Oran7PreferenceChoice.qml","Oran7SwitchToggleItem.qml"}) {
        const auto directory=QString(name).startsWith("Oran7Preference") || QString(name)=="Oran7SwitchToggleItem.qml"
            ? "Oran7SettingUiWindowItems/" : "Oran7GlobalComponents/";
        QFile file(QStringLiteral(APP_ROOT)+"/src/qml/Components/"+directory+name);
        check(file.open(QIODevice::ReadOnly),"read production equalizer QML");
        auto data=file.readAll(); data.replace("import \"../../Settings/GlobalSettings\"","");
        data.replace("import \"../Oran7SettingUiWindowItems\"","");
        data.replace("import \"../Oran7GlobalComponents\"","");
        QFile output(temp.filePath(name)); check(output.open(QIODevice::WriteOnly),"copy isolated QML"); output.write(data);
    }
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("Oran7MainUiSetting",&settings);
    engine.rootContext()->setContextProperty("effectsFixture",&effects);
    QStringList warnings;
    QObject::connect(&engine,&QQmlEngine::warnings,[&](const QList<QQmlError>& errors){for(const auto& e:errors) warnings<<e.toString();});
    const auto url=QUrl::fromLocalFile(temp.filePath("Oran7EqualizerWindow.qml")).toString();
    engine.loadData(QString("import QtQuick\nQtObject { property var editor; Component.onCompleted: { editor=Qt.createComponent('%1').createObject(this,{effects:effectsFixture}); editor.showEditor() } }").arg(url).toUtf8());
    check(!engine.rootObjects().isEmpty(),"isolated editor owner loads");
    QPointer<QQuickWindow> window=qobject_cast<QQuickWindow*>(engine.rootObjects().first()->property("editor").value<QObject*>());
    check(window,"production editor creates an independent window"); QTest::qWait(240);
    auto* first=find(window->contentItem(),"equalizerBand0"); auto* last=find(window->contentItem(),"equalizerBand9");
    check(first && last && first->width()>50 && last->mapToScene(QPointF(last->width(),0)).x()<window->width(),"ten readable columns fit without horizontal clipping");
    auto* slider=find(first,"equalizerSlider");
    auto* handle=slider->property("handle").value<QQuickItem*>(); check(handle,"actual vertical handle exists");
    const auto start=handle->mapToScene(QPointF(handle->width()/2,handle->height()/2)).toPoint();
    const auto initialY=handle->y(); const auto before=publications;
    QTest::mousePress(window,Qt::LeftButton,Qt::NoModifier,start);
    QTest::mouseMove(window,start-QPoint(0,50),30);
    check(effects.equalizer()->gain(0)>0 && handle->y()<initialY && publications>before,"upward drag moves handle up and publishes positive gain before release");
    QTest::mouseMove(window,start+QPoint(0,50),30);
    check(effects.equalizer()->gain(0)<0 && handle->y()>initialY,"downward drag moves handle down and publishes negative gain");
    QTest::mouseRelease(window,Qt::LeftButton,Qt::NoModifier,start+QPoint(0,50));
    check(effects.toConfig().bandGainsDb[0]<0,"edited gain immediately reaches the configuration snapshot");
    effects.applyPreset(presetId); QTest::qWait(30);
    check(first->property("bandGain").toDouble()==effects.equalizer()->gain(0),"preset changes synchronize the open editor from the same model");
    auto* choice=find(window->contentItem(),"equalizerPresetChoice");
    check(choice,"editor reuses the language/font preference choice component");
    auto* popup=choice->property("popup").value<QObject*>();
    check(popup && QMetaObject::invokeMethod(popup,"open"),"open the reusable preset popup");
    QTest::qWait(230);
    check(window && window->isVisible() && !window->property("closeInProgress").toBool() &&
              settings.value("activeOtherItemCount").toInt()==2,
          "opening a nested preference popup preserves its editor and registers nested focus");
    auto* presetOption=find(window->contentItem(),"equalizerPresetChoiceOption0");
    check(presetOption,"real preset option is visible in the reused choice popup");
    QTest::mouseClick(window,Qt::LeftButton,Qt::NoModifier,presetOption->mapToScene(QPointF(70,20)).toPoint());
    QTest::qWait(180);
    check(window && effects.presetId()==presetId && effects.equalizer()->gain(0)==0.0 && choice->property("currentIndex").toInt()==0,
          "clicking the actual preset menu updates shared audio state and selected item");
    QMetaObject::invokeMethod(popup,"open"); QTest::qWait(230);
    QTest::keyClick(window,Qt::Key_Escape); QTest::qWait(180);
    check(window && !popup->property("visible").toBool() && settings.value("activeOtherItemCount").toInt()==1 &&
              settings.value("activeSettingsPopup").value<QObject*>()==window,
          "Escape closes only the preset popup and returns focus ownership to the editor");
    effects.applyPreset(presetId); QTest::qWait(30);
    window->grabWindow().save(QStringLiteral(ORAN7_TEST_ARTIFACT_DIR)+"/equalizer-window.png");
    QSignalSpy commits(window,&QObject::destroyed); QSignalSpy requested(window,SIGNAL(commitRequested()));
    check(settings.value("activeOtherItemCount").toInt()==1,"editor registers one settings focus owner");
    QMetaObject::invokeMethod(popup,"open"); QTest::qWait(230);
    window->close(); QTest::qWait(200);
    check(!window && commits.count()==1 && requested.count()==1 && settings.value("activeOtherItemCount").toInt()==0,"close requests a final save and releases focus only after fade-out");
    for (const auto& w:warnings) std::fprintf(stderr,"%s\n",qPrintable(w));
    check(warnings.isEmpty(),"editor has no QML binding or lifetime warnings");
    std::puts("PASS: independent equalizer editor, drag direction, hot publication, snapshot and lifecycle");
}
