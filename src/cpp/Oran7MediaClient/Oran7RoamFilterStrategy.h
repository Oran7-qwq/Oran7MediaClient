#pragma once
#include <QtGlobal>
#include <memory>

class Oran7TrackLibrary;

// 队列只询问策略是否接收；成员关系复用模型既有哈希索引，不复制喜欢列表。
class Oran7RoamFilterStrategy
{
  public:
    virtual ~Oran7RoamFilterStrategy() = default;
    virtual bool accepts(qint64 songId, const Oran7TrackLibrary& tracks) const = 0;
};

std::unique_ptr<Oran7RoamFilterStrategy> oran7MakeRoamFilterStrategy(bool excludeLiked);
