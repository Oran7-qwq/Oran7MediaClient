#ifndef ORAN7LYRICSMODEL_H
#define ORAN7LYRICSMODEL_H

#include <QAbstractListModel>
#include "Oran7LrcParser.h"

/*! @brief 歌词数据模型，为QML ListView提供歌词数据，并追踪当前高亮行*/
class Oran7LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentLine READ currentLine NOTIFY currentLineChanged)
    Q_PROPERTY(QString currentText READ currentText NOTIFY currentLineChanged)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY hasLyricsChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool skipEmptyLines READ skipEmptyLines WRITE setSkipEmptyLines NOTIFY skipEmptyLinesChanged)

public:
    /*! @brief QML ListView 数据角色枚举*/
    enum Roles {
        TextRole = Qt::UserRole + 1,   //!< 歌词文本
        LineIndexRole                    //!< 行索引
    };

    explicit Oran7LyricsModel(QObject *parent = nullptr);

    // QAbstractListModel 接口
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /*! @brief 从LRC文件加载歌词*/
    Q_INVOKABLE void loadFromFile(const QString &filePath);

    /*! @brief 根据当前播放时间（浮点秒）更新高亮行*/
    Q_INVOKABLE void updateTime(double currentTimeSec);

    /*! @brief 清空歌词数据*/
    Q_INVOKABLE void clearLyrics();

    int currentLine() const { return m_currentLine; }
    bool hasLyrics() const { return !m_lines.isEmpty(); }
    int count() const { return m_lines.size(); }
    bool skipEmptyLines() const { return m_skipEmptyLines; }
    void setSkipEmptyLines(bool skip);
    Q_INVOKABLE QString textAt(int line) const;
    // 与 updateTime 使用同一延迟补偿，单位为播放秒，负值表示无有效行。
    Q_INVOKABLE double lineTriggerTime(int line) const;

    /*! @brief 当前高亮行的歌词文本（无歌词或越界返回空串）*/
    QString currentText() const;

signals:
    void currentLineChanged();
    void hasLyricsChanged();
    void countChanged();
    void skipEmptyLinesChanged();

private:
    Oran7LrcParser m_parser;
    QVector<Oran7LrcParser::LyricLine> m_lines;
    void rebuildLines();
    bool m_skipEmptyLines = false;
    int m_currentLine = -1;
};

#endif // ORAN7LYRICSMODEL_H
