#include "Oran7LyricsModel.h"
#include "Oran7GlobalHelper.h"
#include <algorithm>
#include <cmath>

Oran7LyricsModel::Oran7LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void Oran7LyricsModel::rebuildLines()
{
    m_lines.clear();
    for (const auto& line : m_parser.lines())
        if (!m_skipEmptyLines || !line.text.trimmed().isEmpty()) m_lines.append(line);
}

void Oran7LyricsModel::setSkipEmptyLines(bool skip)
{
    if (m_skipEmptyLines == skip) return;
    beginResetModel();
    m_skipEmptyLines = skip;
    m_currentLine = -1;
    rebuildLines();
    endResetModel();
    emit skipEmptyLinesChanged();
    emit currentLineChanged();
    emit hasLyricsChanged();
    emit countChanged();
}

QString Oran7LyricsModel::textAt(int line) const
{
    return line >= 0 && line < m_lines.size() ? m_lines.at(line).text : QString();
}

double Oran7LyricsModel::lineTriggerTime(int line) const
{
    if (line < 0 || line >= m_lines.size()) return -1;
    return qMax(qint64(0), m_lines.at(line).timestampMs - Oran7LrcParser::timingCompensationMs) / 1000.0;
}

int Oran7LyricsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_lines.size();
}

QString Oran7LyricsModel::currentText() const
{
    if (m_currentLine >= 0 && m_currentLine < m_lines.size())
        return m_lines.at(m_currentLine).text;
    return QString();
}

QVariant Oran7LyricsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_lines.size())
        return QVariant();

    const auto &line = m_lines.at(index.row());

    switch (role)
    {
    case TextRole:
        return line.text;
    case LineIndexRole:
        return index.row();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> Oran7LyricsModel::roleNames() const
{
    return {
        {TextRole, "text"},
        {LineIndexRole, "lineIndex"}
    };
}

void Oran7LyricsModel::loadFromFile(const QString &filePath)
{
    beginResetModel();

    m_parser.clear();
    m_currentLine = -1;

    bool ok = m_parser.parseFile(filePath);
    if (!ok)
    {
        WARNING_LOG << "Oran7LyricsModel::loadFromFile: Failed to parse LRC file:" << filePath;
    }

    rebuildLines();
    endResetModel();

    emit currentLineChanged();
    emit hasLyricsChanged();
    emit countChanged();
}

void Oran7LyricsModel::updateTime(double currentTimeSec)
{
    if (!std::isfinite(currentTimeSec)) return;
    const double timeMs = currentTimeSec * 1000.0 + Oran7LrcParser::timingCompensationMs;
    const auto next = std::upper_bound(m_lines.cbegin(), m_lines.cend(), timeMs,
        [](double time, const Oran7LrcParser::LyricLine& line) { return time < line.timestampMs; });
    const int newLine = int(next - m_lines.cbegin()) - 1;

    if (newLine != m_currentLine)
    {
        // 为旧行和新行发射 dataChanged 以更新高亮状态
        QVector<int> changedRoles;

        if (m_currentLine >= 0 && m_currentLine < m_lines.size())
        {
            QModelIndex oldIdx = index(m_currentLine);
            emit dataChanged(oldIdx, oldIdx);
        }

        m_currentLine = newLine;

        if (m_currentLine >= 0 && m_currentLine < m_lines.size())
        {
            QModelIndex newIdx = index(m_currentLine);
            emit dataChanged(newIdx, newIdx);
        }

        emit currentLineChanged();
    }
}

void Oran7LyricsModel::clearLyrics()
{
    beginResetModel();
    m_parser.clear();
    m_currentLine = -1;
    rebuildLines();
    endResetModel();

    emit currentLineChanged();
    emit hasLyricsChanged();
    emit countChanged();
}
