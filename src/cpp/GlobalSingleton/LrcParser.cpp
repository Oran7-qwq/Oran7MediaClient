#include "LrcParser.h"
#include "GlobalHelper.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>

bool LrcParser::parseFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        WARNING_LOG << "LrcParser::parseFile: Cannot open file:" << filePath;
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif

    QString content = in.readAll();
    file.close();

    return parseContent(content);
}

bool LrcParser::parseContent(const QString &content)
{
    m_lines.clear();

    // 匹配 [mm:ss.xx] 或 [mm:ss.xxx] 后面跟歌词文本
    QRegularExpression re(
        R"(\[(\d{2}):(\d{2})\.(\d{2,3})\](.*))");

    const QStringList lineList = content.split('\n');
    for (const QString &line : lineList)
    {
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty()) continue;

        QRegularExpressionMatch match = re.match(trimmedLine);
        if (match.hasMatch())
        {
            qint64 ms = parseTimestamp(match.captured(0));
            if (ms < 0) continue;

            QString text = match.captured(4).trimmed();

            // 跳过元数据行（如 [00:00.00] 作词 : xxx）
            // 这些行也保留显示，作为歌曲信息展示
            LyricLine lyric{ms, text};
            m_lines.append(lyric);
        }
    }

    // 按时间戳升序排序
    std::sort(m_lines.begin(), m_lines.end(),
              [](const LyricLine &a, const LyricLine &b) {
                  return a.timestampMs < b.timestampMs;
              });

    return hasLyrics();
}

int LrcParser::currentLineIndex(double currentTimeSec) const
{
    if (m_lines.isEmpty()) return -1;

    // 浮点秒转毫秒，+300ms补偿播放器/信号传输链路的固有延迟
    qint64 currentTimeMs = static_cast<qint64>(currentTimeSec * 1000.0) + 300;

    // 二分查找：找到最后一个 timestampMs <= currentTimeMs 的行
    int left = 0;
    int right = m_lines.size() - 1;
    int result = -1;

    while (left <= right)
    {
        int mid = left + (right - left) / 2;
        if (m_lines[mid].timestampMs <= currentTimeMs)
        {
            result = mid;
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return result;
}

void LrcParser::clear()
{
    m_lines.clear();
}

qint64 LrcParser::parseTimestamp(const QString &timestampStr) const
{
    // 从 [mm:ss.xx] 格式中提取 mm, ss, xx
    QRegularExpression re(R"(\[(\d{2}):(\d{2})\.(\d{2,3})\])");
    QRegularExpressionMatch match = re.match(timestampStr);
    if (!match.hasMatch()) return -1;

    int minutes = match.captured(1).toInt();
    int seconds = match.captured(2).toInt();
    QString msStr = match.captured(3);
    int ms = msStr.toInt();

    // 两位毫秒 (xx) 表示百分之一秒，三位 (xxx) 表示毫秒
    if (msStr.length() == 2)
        ms *= 10;  // 转换为毫秒

    return static_cast<qint64>(minutes) * 60000 +
           static_cast<qint64>(seconds) * 1000 +
           static_cast<qint64>(ms);
}
