#include "Oran7CaptureSceneStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QSaveFile>
#include <QSet>
#include <cmath>

namespace
{
constexpr int version = 1;

QVariantMap failure(const QString& message) { return {{"ok", false}, {"message", message}}; }

bool validRectangle(const QJsonValue& value)
{
    const auto numbers = value.toArray();
    if (numbers.size() != 4)
        return false;
    for (const auto number : numbers)
        if (!number.isDouble() || !std::isfinite(number.toDouble()))
            return false;
    return numbers[2].toDouble() > 0 && numbers[3].toDouble() > 0;
}

bool validSource(const QJsonObject& source)
{
    const auto type = source.value("kind").toString();
    if (source.value("name").toString().trimmed().isEmpty() || source.value("width").toInt() <= 0 ||
        source.value("height").toInt() <= 0)
        return false;
    if (type == "display")
    {
        if (source.value("monitorPath").toString().isEmpty() && source.value("deviceName").toString().isEmpty())
            return false;
    }
    else if (type == "window")
    {
        if (source.value("windowClass").toString().isEmpty())
            return false;
    }
    else if (type == "image")
    {
        if (!QFileInfo(source.value("file").toString()).isAbsolute())
            return false;
    }
    else
        return false;

    for (const auto* key : {"hidden", "locked"})
        if (source.contains(key) && !source.value(key).isBool())
            return false;

    const auto transform = source.value("transform").toObject();
    const auto scale = transform.value("scale").toDouble();
    const int turns = transform.value("quarterTurns").toInt(-1);
    return validRectangle(transform.value("geometry")) && validRectangle(transform.value("crop")) &&
           scale > 0 && std::isfinite(scale) && turns >= 0 && turns <= 3 &&
           transform.value("fit").isBool() && transform.value("mirrored").isBool();
}

bool validLibrary(const QJsonObject& library)
{
    const auto scenes = library.value("scenes").toArray();
    QSet<QString> ids;
    for (const auto value : scenes)
    {
        const auto scene = value.toObject();
        const auto id = scene.value("id").toString();
        if (id.isEmpty() || ids.contains(id) || scene.value("name").toString().trimmed().isEmpty() ||
            !scene.value("sources").isArray())
            return false;
        ids.insert(id);
        for (const auto source : scene.value("sources").toArray())
            if (!validSource(source.toObject()))
                return false;
    }
    return ids.contains(library.value("active").toString());
}
}

Oran7CaptureSceneStore::Oran7CaptureSceneStore(QString filePath) : m_path(std::move(filePath)) {}

QVariantMap Oran7CaptureSceneStore::load()
{
    m_writable = false;
    QFile file(m_path);
    m_existed = file.exists();
    if (!m_existed)
    {
        m_committed.clear();
        m_writable = true;
        return {{"ok", true}, {"library", QVariantMap{}}};
    }
    if (!file.open(QIODevice::ReadOnly))
        return failure(QStringLiteral("无法读取场景文件"));
    m_committed = file.readAll();
    if (file.error() != QFileDevice::NoError)
        return failure(QStringLiteral("无法读取场景文件"));
    const auto document = QJsonDocument::fromJson(m_committed);
    const auto object = document.object();
    if (!document.isObject() || object.value("version").toInt(-1) != version || !validLibrary(object))
        return failure(QStringLiteral("场景文件格式或版本不受支持"));
    m_writable = true;
    return {{"ok", true}, {"library", object.toVariantMap()}};
}

QVariantMap Oran7CaptureSceneStore::save(const QVariantMap& library)
{
    if (!m_writable)
        return failure(QStringLiteral("场景文件不可写"));
    auto object = QJsonObject::fromVariantMap(library);
    if (!validLibrary(object))
        return failure(QStringLiteral("场景数据无效"));
    object["version"] = version;
    const auto bytes = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (m_existed && bytes == m_committed)
        return {{"ok", true}};
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath()))
        return failure(QStringLiteral("无法保存场景文件"));
    QLockFile transaction(m_path + QStringLiteral(".lock"));
    if (!transaction.tryLock(0))
        return failure(QStringLiteral("场景文件正在被写入"));
    QFile current(m_path);
    if (current.exists() != m_existed ||
        (m_existed && (!current.open(QIODevice::ReadOnly) || current.readAll() != m_committed)))
        return failure(QStringLiteral("场景文件已被其他程序修改"));
    current.close();
    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return failure(QStringLiteral("无法保存场景文件"));
    m_committed = bytes;
    m_existed = true;
    return {{"ok", true}};
}
