#include "Oran7GlobalHelper.h"
#include <QCoreApplication>
#include <QDir>
#include <atomic>

namespace {
std::atomic_bool developerModeEnabled{false};
}

bool Oran7GlobalHelper::developerMode()
{
    return developerModeEnabled.load(std::memory_order_relaxed);
}

void Oran7GlobalHelper::setDeveloperMode(bool enabled)
{
    developerModeEnabled.store(enabled, std::memory_order_relaxed);
}

Oran7GlobalHelper::Oran7GlobalHelper()
{

}

QString Oran7GlobalHelper::getConfigDir()
{
    QString appDir;
    if (QCoreApplication::instance() && !QCoreApplication::closingDown()) {
        appDir = QCoreApplication::applicationDirPath();
    } else {
        // 退化路径：使用当前工作目录
        appDir = QDir::currentPath();
    }

    QString configDir = QDir::cleanPath(appDir + "/../Config");

    // 确保文件夹存在
    if (!QDir(configDir).exists()) {
        if (!QDir().mkpath(configDir)) {
            qWarning() << "Failed to create Config directory:" << configDir;
        }
    }

    //INFO_LOG << "ConfigDir:" << configDir;

    static QString cachedConfigDir = configDir;
    return cachedConfigDir;
}
