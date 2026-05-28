#ifndef ORAN7FILEHELPER_H
#define ORAN7FILEHELPER_H

#include <QObject>
#include <QFile>
#include <QRegularExpression>
#include <QtQml/qqml.h>

class Oran7FileHelper : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Oran7FileHelper)
public:
    explicit Oran7FileHelper(QObject *parent = nullptr);

    Q_INVOKABLE bool fileExists(const QString &filePath)
    {
        QString processedPath = filePath;

        // 去掉最左最右两边的引号"
        processedPath = processedPath.remove(QRegularExpression("^\"|\"$"));

        // 去掉头部的file:///
        if (processedPath.startsWith("file:///"))
        {
            processedPath = processedPath.mid(8); // "file:///" 长度为8
        }

        m_lastPath_ready = true;
        m_lastProcessedPath = processedPath;

        return QFile::exists(processedPath);
    }

    Q_INVOKABLE QString lastProcessedPath() const {
        return m_lastProcessedPath;
    }

private:
    bool m_lastPath_ready = false;
    QString m_lastProcessedPath = QString();
};

#endif // ORAN7FILEHELPER_H
