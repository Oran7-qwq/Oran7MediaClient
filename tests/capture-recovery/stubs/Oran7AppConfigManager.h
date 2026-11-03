#pragma once
#include "Oran7AppConfig.h"
#include <vector>
class Oran7AppConfigManager
{
  public:
    static Oran7AppConfigManager& ins()
    {
        static Oran7AppConfigManager value;
        return value;
    }
    Oran7AppConfig snapshot() const { return config; }
    void updateConfig(const Oran7AppConfig& value) { config = value; }
    bool saveFails = false;
    std::vector<Oran7ScreenCaptureConfig> writes;
    bool saveConfig()
    {
        if (saveFails)
            return false;
        writes.push_back(config.capture);
        return true;
    }
    bool isWritable() const { return true; }
    QString lastError() const { return QStringLiteral("模拟配置保存失败"); }

  private:
    Oran7AppConfig config;
};
