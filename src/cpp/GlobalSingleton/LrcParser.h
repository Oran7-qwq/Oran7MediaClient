#ifndef LRCPARSER_H
#define LRCPARSER_H

#include <QObject>
#include <QString>
#include <QVector>

/*! @brief LRC歌词文件解析器，解析.lrc格式文件并支持按时间戳查找当前歌词行*/
class LrcParser
{
public:
    /*! @brief 单行歌词数据结构*/
    struct LyricLine {
        qint64 timestampMs;   //!< 时间戳，毫秒
        QString text;         //!< 歌词文本
    };

    LrcParser() = default;

    /*! @brief 从磁盘文件解析LRC歌词，成功返回true*/
    bool parseFile(const QString &filePath);

    /*! @brief 从字符串内容解析LRC歌词，成功返回true*/
    bool parseContent(const QString &content);

    /*! @brief 获取所有歌词行（按时间戳升序排列）*/
    const QVector<LyricLine>& lines() const { return m_lines; }

    /*! @brief 根据当前播放时间（浮点秒）查找应高亮的歌词行索引，使用二分查找 O(log n)*/
    int currentLineIndex(double currentTimeSec) const;

    /*! @brief 清空所有已解析的歌词数据*/
    void clear();

    /*! @brief 是否已加载歌词*/
    bool hasLyrics() const { return !m_lines.isEmpty(); }

private:
    QVector<LyricLine> m_lines;

    /*! @brief 解析时间标签字符串 [mm:ss.xx] 为毫秒值*/
    qint64 parseTimestamp(const QString &timestampStr) const;
};

#endif // LRCPARSER_H
