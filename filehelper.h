#ifndef FILEHELPER_H
#define FILEHELPER_H

#include <QObject>
#include <QFile>

class FileHelper : public QObject
{
    Q_OBJECT
public:
    explicit FileHelper(QObject *parent = nullptr);

    Q_INVOKABLE bool fileExists(const QString& filePath)const{
        return QFile::exists(filePath);
    }
};

#endif // FILEHELPER_H
