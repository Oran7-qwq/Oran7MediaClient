#pragma once

#include <QJsonObject>
#include <optional>

namespace Oran7ThemeProfile
{
struct Oran7ModeAlias
{
    const char* key;
    const char* light;
    const char* dark;
};

// 仅登记确实表示浅色和深色的旧键。
inline constexpr Oran7ModeAlias PlaylistAliases[] = {
    {"rowSurfaceBase", "rowSurfaceBaseLight", "rowSurfaceBaseDark"},
    {"rowHoverBase", "rowHoverBaseLight", "rowHoverBaseDark"},
    {"rowSelectedBase", "rowSelectedBaseLight", "rowSelectedBaseDark"},
    {"rowTextBase", "rowTextBaseLight", "rowTextBaseDark"},
    {"rowSecondaryBase", "rowSecondaryBaseLight", "rowSecondaryBaseDark"},
    {"rowDetailBase", "rowDetailBaseLight", "rowDetailBaseDark"},
    {"rowAccentBase", "rowAccentBaseLight", "rowAccentBaseDark"},
    {"rowSurface", "rowSurfaceLight", "rowSurfaceDark"},
    {"rowSurfaceHover", "rowSurfaceHoverLight", "rowSurfaceHoverDark"},
    {"rowSurfaceSelected", "rowSurfaceSelectedLight", "rowSurfaceSelectedDark"},
    {"rowText", "rowTextLight", "rowTextDark"},
    {"rowSecondaryText", "rowSecondaryTextLight", "rowSecondaryTextDark"},
    {"rowDetail", "rowDetailLight", "rowDetailDark"},
    {"rowAccent", "rowAccentLight", "rowAccentDark"},
};

QString modeName(bool dark);
QString explicitMode(const QString& component, QString& token);
QJsonObject layer(const QJsonObject& group, const QString& mode = {});
void setLayer(QJsonObject& group, const QString& mode, const QJsonObject& tokens);
QJsonObject legacyDefaults(const QJsonObject& source, const QString& component = {});
bool isV3(const QJsonObject& profile);
// 旧格式以发布过的旧源为基准。v3 的裁剪基准由保存方提供，便携副本可原样往返。
std::optional<QJsonObject> normalize(QJsonObject profile, const QJsonObject* defaults = nullptr);
const QJsonObject& legacyProfileDefaults();
} // namespace Oran7ThemeProfile
