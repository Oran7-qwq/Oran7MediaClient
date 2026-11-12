#pragma once

#include <QAbstractListModel>
#include <QList>

/**
 * @brief 固定十段均衡器模型（31Hz…16kHz）。
 * 仅提供增益编辑；频率/Q/类型属于 A1 固定算法定义，不在模型里开放。
 */
class Oran7EqualizerModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count CONSTANT)
public:
    enum Roles {
        FrequencyRole = Qt::UserRole + 1,
        GainDbRole,
        LabelRole
    };

    explicit Oran7EqualizerModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return rowCount(); }
    Q_INVOKABLE void setGain(int row, double gainDb);
    Q_INVOKABLE double gain(int row) const;
    Q_INVOKABLE void reset();

    void setGains(const QList<double>& gains);
    QList<double> gains() const { return m_gains; }

signals:
    void gainChanged(int row, double gainDb);

private:
    QList<double> m_gains;
};
