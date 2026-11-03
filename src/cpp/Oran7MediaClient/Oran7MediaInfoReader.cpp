#include "Oran7MediaInfoReader.h"
#include "Oran7GlobalHelper.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <QProcess>
#include <QSaveFile>
#include <QTemporaryFile>
#include <QUrl>
#include <utility>

bool Oran7MediaInfoReader::isNetworkUrl(const QString& path)
{
    const auto scheme = QUrl(path).scheme();
    return scheme == "http" || scheme == "https";
}

QVariant Oran7MediaInfoReader::getNestedValue(const QStringList& keys, const QJsonObject& obj)
{
    if (keys.isEmpty()) return QVariant();

    QString currentKey = keys.first();
    if (!obj.contains(currentKey))
    {
        return QVariant();
    }

    QJsonValue value = obj.value(currentKey);

    if (keys.size() == 1) {
        // 最后一级键
        if (value.isBool()) return value.toBool();
        if (value.isDouble()) return value.toDouble();
        if (value.isString()) return value.toString();
        if (value.isArray()) return value.toArray().toVariantList();
        if (value.isObject()) return value.toObject().toVariantMap();
        return QVariant();
    }

    if (value.isObject())
    {
        return getNestedValue(keys.mid(1), value.toObject());//继续调用递归
    }

    return QVariant();
}

/**
 * @brief extractMusicInfoTag
 * @param root
 * @param tagName
 * @param defaultValue
 * @return
 */
QString Oran7MediaInfoReader::extractMusicInfoTag(const QJsonObject& root, const QString& tagName, const QString& defaultValue)
{
    // 定义可能的键名变体（按可能性排序）
    QStringList possibleKeys;

    //信息标签灵活推测
    if (tagName.toLower() == "album") {
        possibleKeys = {"album", "ALBUM", "Album"};
    }
    else if (tagName.toLower() == "artist") {
        possibleKeys = {"artist", "ARTIST", "Artist"};
    }
    else if (tagName.toLower() == "title") {
        possibleKeys = {"title", "TITLE", "Title"};
    }
    else {
        possibleKeys = {tagName.toLower(), tagName.toUpper(),
                        tagName.toLower().replace(0, 1, tagName[0].toUpper())};
    }

    // 尝试每种可能的键名
    for (const QString& key : std::as_const(possibleKeys))
    {
        QVariant value = getNestedValue({"format", "tags", key}, root);
        if (value.isValid() && !value.isNull() && !value.toString().isEmpty())
        {
            return value.toString();
        }
    }

    return defaultValue;//返回经过推测的键对应的值
}

// 使用示例

/**
 * @brief 从ffprobe JSON结果中提取媒体信息
 * @param root ffprobe的JSON根对象
 * @return 包含媒体信息的结构体或QMap
 */
QMap<QString, QVariant> Oran7MediaInfoReader::extractMediaInfo(const QJsonObject& root)
{
    QMap<QString, QVariant> mediaInfo;

    // 使用getNestedValue工具函数提取信息

    QVariant duration = getNestedValue({"format", "duration"}, root);
    //应对album,artist,title可能出现不同标准的大小写标签 ，内部仍调用getNestedValue递归遍历提取
    QVariant album = extractMusicInfoTag(root,"album","未知专辑");
    QVariant artist = extractMusicInfoTag(root,"artist","未知歌手");
    QVariant title = extractMusicInfoTag(root,"title","未知歌曲");

    // 处理时长
    if (duration.isValid())
    {
        bool ok;
        double seconds = duration.toString().toDouble(&ok);
        mediaInfo["duration"] = ok ? QString::number(static_cast<int>(seconds)) : "0";
    }
    else
    {
        mediaInfo["duration"] = "0";
    }

    // 处理文本信息
    mediaInfo["album"] = album.isValid() ? album.toString() : "未知专辑";
    mediaInfo["artist"] = artist.isValid() ? artist.toString() : "未知艺术家";
    mediaInfo["title"] = title.isValid() ? title.toString() : "未知标题";

    return mediaInfo;
}

/**
 * @brief Oran7MediaInfoReader::analyzeMediaFileInfo
 * @param filepath
 * @return
 */
QMap<QString, QVariant> Oran7MediaInfoReader::analyzeMediaFileInfo(QString filepath)
{

    QMap<QString, QVariant> mediaInfo;
    mediaInfo["success"] = 0; //设置一个处理成功标志位  0:successed

    // 网络 URL：封面/时长/作者/专辑/歌名无法用 ffprobe 解析（只能通过网络/搜索结果获取）。
    // 直接返回空，由 QML 侧提供真实元数据；无网络时保持空。避免 ffprobe 把 URL 当本地文件报错。
    if (isNetworkUrl(filepath))
        return mediaInfo;

    if (QUrl(filepath).isLocalFile()) filepath = QUrl(filepath).toLocalFile();
    QFileInfo fileInfo(filepath);

    //获取ffmpeg中分析媒体文件处理工具ffprobe
    QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    QString ffprobe_exePath = QDir::toNativeSeparators(appDir + "/ffprobe.exe");
#else
    QString ffprobe_exePath = QDir::toNativeSeparators(appDir + "/ffprobe");
#endif

    //实例化QProcess处理执行ffprobe
    QProcess ffprobe;
    // 网络 URL（网易云播放链接）保留原样传给 ffprobe（ffprobe 支持 http 输入）；
    // QFileInfo.absoluteFilePath() 会把 http:// 破坏成 http:/ 导致找不到文件
    QString absFilePath = isNetworkUrl(filepath)
                        ? filepath
                        : QFileInfo(filepath).absoluteFilePath();
    QString absFFprobePath = QFileInfo(ffprobe_exePath).absoluteFilePath();
    ffprobe.setWorkingDirectory(QCoreApplication::applicationDirPath());

    QStringList ffprobe_args;
    ffprobe_args << "-threads" << "1"
                << "-print_format" << "json"
                << "-show_format"
                << "-i"
                << absFilePath;
    ffprobe.start(absFFprobePath, ffprobe_args);
    //等待进程完成（最多30秒）
    if (!ffprobe.waitForFinished(30000))
    {
        // qDebug() << "Starting FFprobe with arguments:";
        // qDebug() << "  Executable:" << ffprobe_exePath;
        // qDebug() << "  Working dir:" << ffprobe.workingDirectory();
        WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:FFprobe timed out for file:" << fileInfo.absoluteFilePath();
        ffprobe.kill(); // 强制终止进程

        mediaInfo["success"] = 1;
        return mediaInfo;
    }

    if (ffprobe.exitCode() != 0)
    {
        WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:FFprobe ERROR:" << ffprobe.readAllStandardError();
        mediaInfo["success"] = 2;
        return mediaInfo;
    }
    //读取ffprobe输出的Json信息
    QByteArray output = ffprobe.readAllStandardOutput();
    if (output.isEmpty())
    {
        WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:FFprobe returned empty output for file:" << fileInfo.absoluteFilePath();
        mediaInfo["success"] = 3;
        return mediaInfo;
    }
    //解析Json信息
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(output,&parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:JSON parse error:" << parseError.errorString();
        mediaInfo["success"] = 4;
        return mediaInfo;
    }
    QJsonObject root = doc.object();

    /*调用整理好的模块化提取函数*/
    mediaInfo = extractMediaInfo(root);

    if(mediaInfo["duration"].toInt() <=0)
    {
        //媒体时长信息提取失败......
        mediaInfo["success"] = 5;
        return mediaInfo;
    }
    /*开始提取封面图片*/
#ifdef Q_OS_WIN
    QString ffmpegPath = QDir::toNativeSeparators(appDir + "/ffmpeg.exe");
#else
    QString ffmpegPath = QDir::toNativeSeparators(appDir + "/ffmpeg");
#endif
    //创建封面图片输出路径
    QString coverPath = fileInfo.absolutePath() + "/audioCover/" +     //<--------auidoCoverPath
                        fileInfo.completeBaseName() + ".jpg";
    mediaInfo["cover"] = coverPath;
    QDir().mkpath(QFileInfo(coverPath).absolutePath());
    //首先要检测是已经存在已有的封面避免重复拷贝
    if(!QFile::exists(coverPath))
    {
        // 附加图片可能是 png/jpg/bmp；当前 ffmpeg 构建缺 png 解码器（"Oran7FFmpegDecoderCore (codec png) not found"），
        // 因此用 -c:v copy 直接把附加图流拷出来（不解码），再用 Qt 解码并转成 jpg。
        QTemporaryFile rawCover(QFileInfo(coverPath).absolutePath() + "/oran7-cover-XXXXXX.raw");
        if (!rawCover.open()) { mediaInfo["success"] = 7; return mediaInfo; }
        const QString rawCoverPath = rawCover.fileName();
        rawCover.close(); // 让 ffmpeg 在 Windows 上独占写入；离开作用域自动删除。


        QStringList arguments02;
        arguments02 << "-y"
                    << "-i" << absFilePath
                    << "-map" << "0:v?"
                    << "-frames:v" << "1"
                    << "-c:v" << "copy"
                    << "-f" << "image2"
                    << rawCoverPath;
        //ffmpeg.exec()
        QProcess ffmpeg;
        ffmpeg.start(ffmpegPath, arguments02);
        if (!ffmpeg.waitForFinished(30000))
        {
            WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:Getting audioCover time out:" << absFilePath;
            ffmpeg.kill();
            mediaInfo["success"] = 6;
            return mediaInfo;
        }
        if (ffmpeg.exitCode() != 0)
        {
            WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:ffmpeg exit ERROR of" << absFilePath;
            WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:ffmpeg exit ERROR:" << ffmpeg.readAllStandardError();
            mediaInfo["success"] = 7;
            return mediaInfo;
        }
        // 用 Qt 解码原始封面（自动识别 png/jpg/bmp），转成 jpg
        QFile rawFile(rawCoverPath);
        if (rawFile.open(QIODevice::ReadOnly))
        {
            const QByteArray rawData = rawFile.readAll();
            rawFile.close();
            QImage coverImg = QImage::fromData(rawData);
            if (!coverImg.isNull()) {
                QSaveFile output(coverPath);
                if (output.open(QIODevice::WriteOnly) && coverImg.save(&output, "JPG")) output.commit();
            } else
                WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:Qt decode cover failed:" << rawCoverPath;
        }

        // 验证封面文件  小于1KB MayBe无效封面
        if (QFileInfo(coverPath).size() < 1024)
        {
            WARNING_LOG << "Oran7MediaInfoReader::analyzeMediaFileInfo:AudioCover file may be invalid:" << coverPath;
            QFile::remove(coverPath);
        }
    }
    //成功返回
    mediaInfo["success"] = 0;
    return mediaInfo;
}

