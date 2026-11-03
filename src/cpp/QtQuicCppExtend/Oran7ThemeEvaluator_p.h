#pragma once

#include "Oran7ColorGenerator.h"
#include "Oran7ThemeFunctions.h"
#include <QRegularExpression>
#include <QSet>
#include <QScopedValueRollback>
#include <QVariantMap>
#include <cmath>

// 每次计算独立持有输入与结果；引用按依赖解析，不依赖 JSON 键的排序。
class Oran7ThemeEvaluator
{
  public:
    Oran7ThemeEvaluator(QVariantMap raw, QVariantMap inherited, bool dark, QVariantMap fallback = {})
        : m_raw(std::move(raw)), m_values(std::move(inherited)), m_fallback(std::move(fallback)), m_dark(dark)
    {
    }

    QVariantMap evaluate()
    {
        const auto inheritedKeys = m_values.keys();
        for (auto it = m_raw.cbegin(); it != m_raw.cend(); ++it)
            resolve(it.key());
        for (const auto& key : inheritedKeys)
            if (!m_raw.contains(key) && !m_generated.contains(key))
                m_values.remove(key);
        return m_values;
    }

  private:
    QVariant resolve(const QString& key)
    {
        if (m_done.contains(key))
            return m_values.value(key);
        if (m_active.contains(key))
            return {};
        if (!m_raw.contains(key))
        {
            // 生成的 colorBase-1 等由对应生成器负责。
            const auto dash = key.lastIndexOf('-');
            if (dash > 0 && m_raw.contains(key.left(dash)))
                resolve(key.left(dash));
            return m_values.value(key);
        }
        m_active.insert(key);
        auto value = expression(m_raw.value(key), key);
        if (!value.isValid())
        {
            qWarning() << "主题表达式无效，使用回退值，token:" << key;
            value = m_fallback.value(key);
            // 生成器失败时也恢复其序列，避免下游引用丢失默认色板/字号。
            const auto prefix = key + "-";
            for (auto it = m_fallback.cbegin(); it != m_fallback.cend(); ++it)
                if (it.key().startsWith(prefix))
                {
                    m_values[it.key()] = it.value();
                    m_generated.insert(it.key());
                }
        }
        if (value.isValid())
            m_values.insert(key, value);
        m_active.remove(key);
        m_done.insert(key);
        return m_values.value(key);
    }

    QVariant expression(const QVariant& input, const QString& key = {})
    {
        if (m_depth >= 64)
            return {};
        QScopedValueRollback<int> expressionDepth(m_depth, m_depth + 1);
        // 原生 JSON 数字/布尔不转换成字符串，普通字符串保留空白。
        if (input.metaType().id() != QMetaType::QString)
            return input;
        const auto text = input.toString();
        if (text.size() > 8192)
            return {};
        if (text.startsWith('@'))
            return resolve(text.mid(1).trimmed());
        if (text.startsWith('#'))
        {
            const QColor color = text.startsWith("#Preset_") ? Oran7ColorGenerator::presetToColor(text) : QColor(text);
            return color.isValid() ? QVariant(color) : QVariant();
        }
        if (!text.startsWith('$'))
            return input;
        static const QRegularExpression call(R"(^\$([A-Za-z]+)\((.*)\)$)");
        const auto match = call.match(text.trimmed());
        if (!match.hasMatch())
            return {};
        const auto function = match.captured(1);
        const auto body = match.captured(2);
        if (function == "genFontFamily")
            return Oran7ThemeFunctions::genFontFamily(body);
        // 仅拆分顶层逗号，允许嵌套函数。
        QStringList args;
        int depth = 0, start = 0;
        for (int i = 0; i < body.size(); ++i)
        {
            if (body[i] == '(')
                ++depth;
            if (body[i] == ')')
                --depth;
            if (depth < 0)
                return {};
            if (body[i] == ',' && depth == 0)
            {
                args << body.mid(start, i - start).trimmed();
                start = i + 1;
            }
        }
        if (depth != 0)
            return {};
        args << body.mid(start).trimmed();
        auto arg = [&](int i) { return i < args.size() ? expression(args[i]) : QVariant(); };
        auto number = [&](int i, double* out)
        {
            bool ok = false;
            *out = arg(i).toDouble(&ok);
            return ok && std::isfinite(*out);
        };
        auto color = [&](int i) { return arg(i).value<QColor>(); };
        double a = 0, b = 0, c = 0;
        if (function == "genColor" && args.size() == 1 && color(0).isValid())
        {
            const auto palette =
                Oran7ThemeFunctions::genColor(color(0), !m_dark, resolve("colorBgBase").value<QColor>());
            for (int i = 0; i < qMin<qsizetype>(10, palette.size()); ++i)
                generated(key, i, palette[i]);
            return color(0);
        }
        if ((function == "genFontSize" || function == "genFontLineHeight" || function == "genRadius") &&
            args.size() == 1 && number(0, &a) && a > 0 && a <= 4096)
        {
            if (function == "genRadius")
            {
                const auto values = Oran7ThemeFunctions::genRadius(int(a));
                for (int i = 0; i < values.size(); ++i)
                    generated(key, i, values[i]);
            }
            else
            {
                const auto values = function == "genFontSize" ? Oran7ThemeFunctions::genFontSize(a)
                                                              : Oran7ThemeFunctions::genFontLineHeight(a);
                for (int i = 0; i < values.size(); ++i)
                    generated(key, i, values[i]);
            }
            return a;
        }
        if (function == "multiply" && args.size() == 2 && number(0, &a) && number(1, &b) && std::isfinite(a * b))
            return a * b;
        if (function == "onBackground" && args.size() == 2 && color(0).isValid() && color(1).isValid())
            return Oran7ThemeFunctions::onBackground(color(0), color(1));
        if (!color(0).isValid())
            return {};
        if (function == "alpha" && (args.size() == 1 || (args.size() == 2 && number(1, &a) && a >= 0 && a <= 1)))
            return Oran7ThemeFunctions::alpha(color(0), args.size() == 1 ? 0.5 : a);
        if ((function == "darker" || function == "lighter") &&
            (args.size() == 1 || (args.size() == 2 && number(1, &a) && a >= 0 && a <= 10000)))
            return function == "darker" ? Oran7ThemeFunctions::darker(color(0), args.size() == 1 ? 140 : int(a))
                                        : Oran7ThemeFunctions::lighter(color(0), args.size() == 1 ? 140 : int(a));
        if (function == "brightness" && args.size() >= 1 && args.size() <= 3)
        {
            b = 140;
            c = 140;
            if ((args.size() > 1 && (!number(1, &b) || b < 0 || b > 10000)) ||
                (args.size() > 2 && (!number(2, &c) || c < 0 || c > 10000)))
                return {};
            return Oran7ThemeFunctions::brightness(color(0), !m_dark, int(b), int(c));
        }
        return {};
    }

    void generated(const QString& key, int index, const QVariant& value)
    {
        const auto name = key + "-" + QString::number(index + 1);
        m_values[name] = value;
        m_generated.insert(name);
    }
    QVariantMap m_raw, m_values, m_fallback;
    QSet<QString> m_active, m_done, m_generated;
    bool m_dark;
    int m_depth = 0;
};
