#include "LyricsModel.h"
#include "GlobalHelper.h"

LyricsModel::LyricsModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LyricsModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_parser.lines().size();
}

QVariant LyricsModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_parser.lines().size())
        return QVariant();

    const auto &line = m_parser.lines().at(index.row());

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

QHash<int, QByteArray> LyricsModel::roleNames() const
{
    return {
        {TextRole, "text"},
        {LineIndexRole, "lineIndex"}
    };
}

void LyricsModel::loadFromFile(const QString &filePath)
{
    beginResetModel();

    m_parser.clear();
    m_currentLine = -1;

    bool ok = m_parser.parseFile(filePath);
    if (!ok)
    {
        WARNING_LOG << "LyricsModel::loadFromFile: Failed to parse LRC file:" << filePath;
    }

    endResetModel();

    emit currentLineChanged();
    emit hasLyricsChanged();
    emit countChanged();
}

void LyricsModel::updateTime(double currentTimeSec)
{
    int newLine = m_parser.currentLineIndex(currentTimeSec);

    if (newLine != m_currentLine)
    {
        // 为旧行和新行发射 dataChanged 以更新高亮状态
        QVector<int> changedRoles;

        if (m_currentLine >= 0 && m_currentLine < m_parser.lines().size())
        {
            QModelIndex oldIdx = index(m_currentLine);
            emit dataChanged(oldIdx, oldIdx);
        }

        m_currentLine = newLine;

        if (m_currentLine >= 0 && m_currentLine < m_parser.lines().size())
        {
            QModelIndex newIdx = index(m_currentLine);
            emit dataChanged(newIdx, newIdx);
        }

        emit currentLineChanged();
    }
}

void LyricsModel::clearLyrics()
{
    beginResetModel();
    m_parser.clear();
    m_currentLine = -1;
    endResetModel();

    emit currentLineChanged();
    emit hasLyricsChanged();
    emit countChanged();
}
