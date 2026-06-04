#ifndef LYRICSMODEL_H
#define LYRICSMODEL_H

#include <QAbstractListModel>
#include "LrcParser.h"

/*! @brief 歌词数据模型，为QML ListView提供歌词数据，并追踪当前高亮行*/
class LyricsModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int currentLine READ currentLine NOTIFY currentLineChanged)
    Q_PROPERTY(bool hasLyrics READ hasLyrics NOTIFY hasLyricsChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    /*! @brief QML ListView 数据角色枚举*/
    enum Roles {
        TextRole = Qt::UserRole + 1,   //!< 歌词文本
        LineIndexRole                    //!< 行索引
    };

    explicit LyricsModel(QObject *parent = nullptr);

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
    bool hasLyrics() const { return m_parser.hasLyrics(); }
    int count() const { return m_parser.lines().size(); }

signals:
    void currentLineChanged();
    void hasLyricsChanged();
    void countChanged();

private:
    LrcParser m_parser;
    int m_currentLine = -1;
};

#endif // LYRICSMODEL_H
