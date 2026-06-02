#include "GlobalHelper.h"
#include <QCoreApplication>
#include <QDir>

GlobalHelper::GlobalHelper()
{

}

QString GlobalHelper::getConfigDir()
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
