#include "Oran7EqualizerModel.h"

#include <cmath>

namespace {
constexpr int kBandCount = 10;
const double kFrequencies[kBandCount] = {31.0, 62.0, 125.0, 250.0, 500.0,
                                         1000.0, 2000.0, 4000.0, 8000.0, 16000.0};
const char* const kLabels[kBandCount] = {"31", "62", "125", "250", "500",
                                         "1k", "2k", "4k", "8k", "16k"};
constexpr double kMinGainDb = -12.0;
constexpr double kMaxGainDb = 12.0;

double clampGain(double gainDb)
{
    if (!std::isfinite(gainDb)) return 0.0;
    if (gainDb < kMinGainDb) return kMinGainDb;
    if (gainDb > kMaxGainDb) return kMaxGainDb;
    return gainDb;
}
}

Oran7EqualizerModel::Oran7EqualizerModel(QObject* parent)
    : QAbstractListModel(parent), m_gains(kBandCount, 0.0)
{
}

int Oran7EqualizerModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : kBandCount;
}

QVariant Oran7EqualizerModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= kBandCount)
        return {};
    switch (role)
    {
    case FrequencyRole: return kFrequencies[index.row()];
    case GainDbRole:    return m_gains.at(index.row());
    case LabelRole:     return QString::fromLatin1(kLabels[index.row()]);
    default:            return {};
    }
}

QHash<int, QByteArray> Oran7EqualizerModel::roleNames() const
{
    return {{FrequencyRole, "frequency"}, {GainDbRole, "gainDb"}, {LabelRole, "label"}};
}

void Oran7EqualizerModel::setGain(int row, double gainDb)
{
    if (row < 0 || row >= kBandCount) return;
    const double value = clampGain(gainDb);
    if (m_gains.at(row) == value) return;

    m_gains[row] = value;
    const auto changed = index(row, 0);
    emit dataChanged(changed, changed, {GainDbRole});
    emit gainChanged(row, value);
}

double Oran7EqualizerModel::gain(int row) const
{
    return (row >= 0 && row < kBandCount) ? m_gains.at(row) : 0.0;
}

void Oran7EqualizerModel::reset()
{
    setGains(QList<double>(kBandCount, 0.0));
}

void Oran7EqualizerModel::setGains(const QList<double>& gains)
{
    for (int row = 0; row < kBandCount; ++row)
    {
        const double value = clampGain(row < gains.size() ? gains.at(row) : 0.0);
        if (m_gains.at(row) == value) continue;
        m_gains[row] = value;
        const auto changed = index(row, 0);
        emit dataChanged(changed, changed, {GainDbRole});
    }
}
