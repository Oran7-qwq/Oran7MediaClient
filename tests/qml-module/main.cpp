#include "Oran7Theme.h"
#include <QGuiApplication>
#include <QFontDatabase>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QJSValue>
#include <QTemporaryDir>
#include <QDir>
#include <cstdio>
#include <memory>

int main(int argc, char** argv)
{
    // 实际插件在独立检查目录运行；配置与 profile 只落在 tests/Config。
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName("Oran7QmlModuleChecks");
    if (QFontDatabase::addApplicationFont(qEnvironmentVariable("WINDIR", "C:/Windows") + "/Fonts/msyh.ttc") < 0)
    {
        std::fprintf(stderr, "offline plugin fixture requires Windows Chinese font\n");
        return 1;
    }
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(ORAN7_QML_IMPORT_ROOT));
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQml
        import Oran7UI.Impl 1.0
        QtObject {
            property var theme: Oran7Theme
            property var functions: Oran7ThemeFunctions
            property var api: Oran7Api
            property Oran7FileHelper files: Oran7FileHelper {}
            property var colors: Oran7ColorGenerator
            property Oran7RadiusGenerator radii: Oran7RadiusGenerator {}
            property Oran7SizeGenerator sizes: Oran7SizeGenerator {}
            property var palette: Oran7ThemeFunctions.genColor("#336699", true)
        }
    )",
                      QUrl("qrc:/Oran7PluginFixture.qml"));
    std::unique_ptr<QObject> object(component.create());
    if (!object)
    {
        for (const auto& error : component.errors())
            std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
        return 1;
    }
    const auto asObject = [](const QVariant& value)
    {
        return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toQObject()
                                                                   : value.value<QObject*>();
    };
    const auto fromQml = asObject(object->property("theme"));
    const auto palette = object->property("palette");
    const auto colors = palette.metaType() == QMetaType::fromType<QJSValue>()
                            ? palette.value<QJSValue>().toVariant().toList()
                            : palette.toList();
    if (fromQml != &Oran7Theme::instance() || qmlTypeId("Oran7UI.Impl", 1, 0, "Oran7WindowAgent") < 0 ||
        !asObject(object->property("functions")) || colors.isEmpty() ||
        QQmlEngine::objectOwnership(asObject(object->property("functions"))) != QQmlEngine::CppOwnership ||
        QQmlEngine::objectOwnership(asObject(object->property("api"))) != QQmlEngine::CppOwnership)
    {
        std::fprintf(stderr, "plugin check: identity=%d type=%d functions=%d colors=%lld\n",
                     fromQml == &Oran7Theme::instance(), qmlTypeId("Oran7UI.Impl", 1, 0, "Oran7WindowAgent"),
                     asObject(object->property("functions")) != nullptr, static_cast<long long>(colors.size()));
        return 1;
    }
    std::puts("PASS: real QML plugin import, exported types, generators and shared theme identity");
}
