#include "Oran7ThemeSchema_p.h"
#include "Oran7ThemeProfile.h"

#include <limits>

namespace Oran7ThemeSchema
{
namespace
{
using Fields = Oran7ThemeSource::Fields;
using Field = Oran7ThemeSource::Oran7Field;
using Type = Oran7ThemeSource::Type;

void add(Fields& fields, std::initializer_list<const char*> names, const Field& spec)
{
    for (const auto* name : names)
        fields.insert(QLatin1String(name), spec);
}

Field choice(const QStringList& values)
{
    Field field{Type::Choice};
    field.choices = values;
    return field;
}

void requireCompactFields(Fields& fields, const QStringList& required)
{
    for (auto it = fields.begin(); it != fields.end(); ++it)
        it->required = required.contains(it.key());
}

void addBlur(Fields& fields)
{
    fields.insert("blurEffectEnabled", {Type::Boolean});
    fields.insert("saturation", {Type::Number, -1, 2});
    add(fields, {"brightness", "contrast"}, {Type::Number, -1, 1});
}
} // namespace

Fields foundations(bool complete)
{
    Fields fields;
    add(fields, {"fontFamilyBase", "fontPrimaryFamily", "fontDisplayFamily", "fontBrandFamily"}, {Type::String});
    add(fields, {"fontSizeBase", "fontLineHeightBase", "fontPrimarySize", "fontPrimarySizeHeading1",
                 "fontPrimarySizeHeading2", "fontPrimarySizeHeading3", "fontPrimarySizeHeading4", "fontPrimarySizeHeading5"},
        {Type::Number, 0, 8192});
    add(fields, {"fontPrimaryLineHeightHeading1", "fontPrimaryLineHeightHeading2", "fontPrimaryLineHeightHeading3",
                 "fontPrimaryLineHeightHeading4", "fontPrimaryLineHeightHeading5"}, {Type::Number, 0, 8192});
    fields.insert("fontPrimaryHeight", {Type::Number, 0, 8192 * 8192});
    add(fields, {"radiusBase", "radiusPrimary", "radiusPrimaryLG", "radiusPrimarySM", "radiusPrimaryXS",
                 "radiusPrimaryOuter", "radiusSmall"}, {Type::Integer, 0, 4096});
    // 这些旧公开键允许历史的十进制文本，以保留现有 QVariant 类型及旧覆盖。
    add(fields, {"durationVeryFast", "durationFast", "durationMid", "durationSlow", "durationVerySlow"},
        {Type::LegacyInteger, 0, 60000});
    fields.insert("durationNormal", {Type::Integer, 0, 60000});
    if (!complete)
        requireCompactFields(fields, {"radiusSmall", "durationFast", "durationNormal"});
    return fields;
}

Fields colors(bool complete)
{
    Fields fields;
    add(fields, {"accent", "textPrimary", "textSecondary", "surfacePanel", "borderSubtle",
                 "colorTextBase", "colorBgBase", "colorTextPrimary", "colorTextSecondary",
                 "colorTextTertiary", "colorTextQuaternary", "colorFill", "colorFillPrimary",
                 "colorFillSecondary", "colorFillTertiary", "colorFillQuaternary", "colorBgSolid",
                 "colorBgSolidHover", "colorBgSolidActive", "colorBgLayout", "colorBgContainer",
                 "colorBgElevated", "colorBgSpotlight", "colorBgMask", "colorBorder", "colorBorderSecondary",
                 "colorPrimaryContainerBg", "colorPrimaryContainerBgHover", "colorPrimaryContainerBgActive",
                 "colorPrimaryContainerBgDisabled", "colorPrimaryTextDisabled",
                 "colorPrimaryBgActive", "colorPrimaryBorderActive", "colorErrorBgActive",
                 "colorLinkBase", "colorLinkHover", "colorLink", "colorLinkActive"}, {Type::Color});
    fields.insert("colorBgBlur", {Type::ColorName});
    for (const auto* family : {"colorPrimary", "colorSuccess", "colorError", "colorWarning", "colorInfo"})
        for (const auto* suffix : {"Base", "Bg", "BgHover", "Border", "BorderHover",
                                   "Hover", "", "Active", "TextHover", "Text", "TextActive"})
            fields.insert(QLatin1String(family) + QLatin1String(suffix), {Type::Color});
    for (int i = 1; i <= 13; ++i)
        fields.insert(QStringLiteral("colorMain-%1").arg(i), {Type::Color});
    if (!complete)
        requireCompactFields(fields, {"accent", "textPrimary", "textSecondary", "surfacePanel", "borderSubtle",
                                      "colorTextBase", "colorBgBase"});
    return fields;
}

Fields component(const QString& name)
{
    if (name == "Oran7MusicPlaylistView")
    {
        Fields fields{{"rowTextColor", {Type::OptionalColor}}, {"listViewFontPixelSize", {Type::Integer, 7, 20}}};
        add(fields, {"colorPrimaryBase", "textColorBase", "rowBackgroundColor", "rowAccentColor"}, {Type::Color});
        for (const auto& alias : Oran7ThemeProfile::PlaylistAliases)
            fields.insert(QLatin1String(alias.key), {Type::Color});
        return fields;
    }
    if (name == "Oran7MainGUI")
    {
        Fields fields{{"backgroundMode", choice({"Image", "Video"})},
                      {"backgroundParallaxAmplitude", {Type::Integer, 0, 30}},
                      {"soundEffectVolume", {Type::Number, 0, 1}}};
        add(fields, {"themeColor", "colorPrimaryBase"}, {Type::Color});
        add(fields, {"backgroundImage", "backgroundVideo"}, {Type::Resource});
        add(fields, {"backgroundParallaxEnabled", "DWMBlurEffectEnabled"}, {Type::Boolean});
        add(fields, {"topBarDefaultHeight", "settingWinItemDefalutWidth"}, {Type::Number, 0, 4096});
        return fields;
    }
    if (name == "Oran7ProgressSlider")
        return {{"moveThreshold", {Type::LegacyInteger, 0, 4096}}};
    if (name == "Oran7VideoPlayer")
        return {{"ScaleMode", choice({"Fit", "Fill"})}};
    if (name == "Oran7ScreenCapture")
        return {{"ScaleMode", choice({"Fit", "Fill"})}, {"workspaceSkin", choice({"default", "violet"})}};

    Fields fields;
    addBlur(fields);
    if (name == "Oran7CaptionBar" || name == "Oran7MusicPlayControls")
    {
        add(fields, {"colorPrimaryBase", "iconColorBase", "textColorBase"}, {Type::Color});
        if (name == "Oran7CaptionBar")
        {
            add(fields, {"textColor", "selectedColor", "hoveredColor"}, {Type::Color});
            fields.insert("isSimpleMode", {Type::Boolean});
        }
        else
        {
            fields.insert("progressVisibleBar_dynamicGradientEnabled", {Type::Boolean});
            fields.insert("progressVisibleBar_ColorIndex", {Type::Integer, 0, 19});
        }
        return fields;
    }
    if (name == "Oran7MusicLyricsWindow" || name == "Oran7MusicLyricsIsland")
    {
        fields.insert("textColorBase", {Type::Color});
        fields.insert("textSizeBase", {Type::Number, 12, 40});
        if (name == "Oran7MusicLyricsIsland")
        {
            fields.insert("floatingBackdropColor", {Type::Color});
            fields.insert("backgroundImage_url", {Type::Resource});
            fields.insert("lyricsFontSize", {Type::Integer, 12, 40});
            fields.insert("floatingOpacity", {Type::Number, 0, 1});
            fields.insert("floatingLyricsHeight", {Type::Integer, 32, 96});
            fields.insert("floatingPositionVersion", {Type::Integer, 0, 2});
            add(fields, {"backgroundImageEnabled", "floatingEnabled", "floatingLocked", "floatingClickThrough",
                         "floatingAlwaysOnTop"}, {Type::Boolean});
            // 位置可含小数，且虚拟桌面可能有负坐标。
            const auto limit = std::numeric_limits<double>::max();
            add(fields, {"floatingCenterX", "floatingX", "floatingY"}, {Type::Number, -limit, limit});
        }
        return fields;
    }
    return {};
}
} // namespace Oran7ThemeSchema
