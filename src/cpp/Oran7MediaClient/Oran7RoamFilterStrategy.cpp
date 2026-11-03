#include "Oran7RoamFilterStrategy.h"
#include "Oran7TrackLibrary.h"

namespace {
class Oran7KeepAllRoamStrategy final : public Oran7RoamFilterStrategy
{
    bool accepts(qint64, const Oran7TrackLibrary&) const override { return true; }
};
class Oran7ExcludeLikedRoamStrategy final : public Oran7RoamFilterStrategy
{
    bool accepts(qint64 songId, const Oran7TrackLibrary& tracks) const override
    { return !tracks.isLiked(songId); }
};
}
std::unique_ptr<Oran7RoamFilterStrategy> oran7MakeRoamFilterStrategy(bool excludeLiked)
{
    if (excludeLiked) return std::make_unique<Oran7ExcludeLikedRoamStrategy>();
    return std::make_unique<Oran7KeepAllRoamStrategy>();
}
