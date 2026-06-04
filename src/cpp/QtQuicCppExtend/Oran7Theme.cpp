/*
 * HuskarUI
 *
 * Copyright (C) mengps (MenPenS) (MIT License)
 * https://github.com/mengps/HuskarUI
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 * - The above copyright notice and this permission notice shall be included in
 *   all copies or substantial portions of the Software.
 * - The Software is provided "as is", without warranty of any kind, express or
 *   implied, including but not limited to the warranties of merchantability,
 *   fitness for a particular purpose and noninfringement. In no event shall the
 *   authors or copyright holders be liable for any claim, damages or other
 *   liability, whether in an action of contract, tort or otherwise, arising from,
 *   out of or in connection with the Software or the use or other dealings in the
 *   Software.
 */

/*! @note */

#include "GlobalHelper.h"

#include "Oran7Theme.h"
#include "Oran7Theme_p.h"
#include "Oran7ColorGenerator.h"
#include "Oran7ThemeFunctions.h"
#include "Oran7ThemeProfileManager.h"
#include "AppJsonConfigManager.h"

#include <QtCore/QFile>
#include <QtCore/QRegularExpression>
#include <QtCore/QJsonArray>
#include <math.h>

Oran7Theme* Oran7Theme::s_instance = nullptr;

void Oran7ThemePrivate::initializeComponentPropertyHash()
{
#define ADD_COMPONENT_PROPERTY(ComponentName)\
    g_componentTable->insert(#ComponentName,&q->m_##ComponentName);

    Q_Q(Oran7Theme);

    static bool initialized = false;
    if(initialized == false)
    {
        initialized = true;
        ADD_COMPONENT_PROPERTY(Oran7ProgressSlider);
        ADD_COMPONENT_PROPERTY(Oran7MainGUI);
        ADD_COMPONENT_PROPERTY(Oran7CaptionBar);
        ADD_COMPONENT_PROPERTY(Oran7MusicPlaylistView);
        ADD_COMPONENT_PROPERTY(Oran7MusicPlayControls);
        ADD_COMPONENT_PROPERTY(Oran7MusicLyricsWindow);
    }
}

Oran7ThemePrivate::~Oran7ThemePrivate()
{
    INFO_LOG<<"[DTOR] Oran7ThemePrivate::~Oran7ThemePrivate() finished.";
}

void Oran7ThemePrivate::parse$(QMap<QString, QVariant> &out, const QString &tokenName, const QString &expr)
{
    Q_Q(Oran7Theme);

    static QHash<QString,TokenFunction> g_funcTable{
        {"genColor",            TokenFunction::GenColor},
        {"genFontFamily",       TokenFunction::GenFontFamily},
        {"genFontSize",         TokenFunction::GenFontSize},
        { "genFontLineHeight", TokenFunction::GenFontLineHeight },
        { "genRadius",         TokenFunction::GenRadius },
        { "darker",            TokenFunction::Darker },
        { "lighter",           TokenFunction::Lighter },
        { "brightness",        TokenFunction::Brightness },
        { "alpha",             TokenFunction::Alpha },
        { "onBackground",      TokenFunction::OnBackground },
        { "multiply",          TokenFunction::Multiply }
    };
    static QRegularExpression g_funcRegex("\\$([^(]+)\\(");
    static QRegularExpression g_argsRegex("\\(([^)]+)\\)");

    QRegularExpressionMatch funcMatch = g_funcRegex.match(expr);
    QRegularExpressionMatch argsMatch = g_argsRegex.match(expr);
    if(funcMatch.hasMatch())
    {
        QString func = funcMatch.captured(1);
        QString args = argsMatch.captured(1);
        if(g_funcTable.contains(func))
        {
            switch (g_funcTable[func]) {
            case TokenFunction::GenColor:
            {
                QColor color = colorFromIndexTable(args);

                if (color.isValid()) {
                    auto colorBgBase = m_indexTokenTable["colorBgBase"].value<QColor>();
                    auto colors = Oran7ThemeFunctions::genColor(color,!q->isDark(),colorBgBase);
                    const int count = std::min(static_cast<int>(colors.size()),10);
                    for (int i = 0; i < count; ++i) {
                        auto genColor = colors.at(i);
                        auto key = tokenName + "-" + QString::number(i + 1);
                        out[key] = genColor;
                        //INFO_LOG << "  Generated key:" << key << "value:" << genColor;
                    }
                }
                else
                    ERROR_LOG << QString("func genColor() invalid args of color:(%1)").arg(args);
            }break;
            case TokenFunction::GenFontFamily:
            {
                out["fontFamilyBase"] = Oran7ThemeFunctions::genFontFamily(args.trimmed());
            }break;
            case TokenFunction::GenFontSize:
            {
                bool ok = false;
                auto base = args.toDouble(&ok);
                if(ok)
                {
                    const auto fontSizes = Oran7ThemeFunctions::genFontSize(base);
                    //INFO_LOG << "genFontSize(" << base << ") returned" << fontSizes.length() << "values:" << fontSizes;
                    for(int i = 0;i<fontSizes.length();i++)
                    {
                        auto genFontSize = fontSizes.at(i);
                        auto key = tokenName + "-" + QString::number(i+1);
                        out[key] = genFontSize;
                        //INFO_LOG << "  Generated key:" << key << "value:" << genFontSize;
                    }
                }
                else
                    ERROR_LOG << QString("func genFontSize() invalid args of size:(%1)").arg(args);
            }break;
            case TokenFunction::GenFontLineHeight:
            {
                bool ok = false;
                auto base = args.toDouble(&ok);
                if(ok)
                {
                    const auto fontLineHeights = Oran7ThemeFunctions::genFontLineHeight(base);
                    //INFO_LOG << "genFontLineHeight(" << base << ") returned" << fontLineHeights.length() << "values:" << fontLineHeights;
                    for(int i=0;i<fontLineHeights.length();i++)
                    {
                        auto genFontLineHeight = fontLineHeights.at(i);  // Fixed: was fontLineHeights.at(1)
                        auto key = tokenName + "-" + QString::number(i+1);
                        out[key] = genFontLineHeight;
                        //INFO_LOG << "  Generated key:" << key << "value:" << genFontLineHeight;
                    }
                }
                else
                    ERROR_LOG << QString("func genFontLineHeight() invalid args of size:(%1)").arg(args);
            }break;
            case TokenFunction::GenRadius:
            {
                bool ok = false;
                auto base = args.toInt(&ok);
                if(ok)
                {
                    const auto radius = Oran7ThemeFunctions::genRadius(base);
                    for(int i=0;i<radius.length();i++)
                    {
                        auto genRadius = radius.at(i);
                        auto key = tokenName + "-" +QString::number(i+1);
                        out[key] = genRadius;
                    }
                }
                else
                    ERROR_LOG<< QString("func genRadius() invalid args of size:(%1)").arg(args);
            }break;
            case TokenFunction::Darker:
            {
                auto argList = args.split(',');
                if(argList.length() == 1)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    out[tokenName] = Oran7ThemeFunctions::darker(arg1);
                }
                else if(argList.length() == 2)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    auto arg2 = numberFromIndexTable(argList.at(1));
                    out[tokenName] = Oran7ThemeFunctions::darker(arg1,arg2);
                }
                else
                    ERROR_LOG << QString("func darker() only accepts 1/2 parameters:(%1)").arg(args);
            }break;
            case TokenFunction::Lighter:
            {
                auto argList = args.split(',');
                if (argList.length() == 1)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    out[tokenName] = Oran7ThemeFunctions::lighter(arg1);
                }
                else if (argList.length() == 2)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    auto arg2 = numberFromIndexTable(argList.at(1));
                    out[tokenName] = Oran7ThemeFunctions::lighter(arg1, arg2);
                }
                else
                    ERROR_LOG << QString("func lighter() only accepts 1/2 parameters:(%1)").arg(args);
            }break;
            case TokenFunction::Brightness:
            {
                auto argList = args.split(',');
                if (argList.length() == 1)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    out[tokenName] = Oran7ThemeFunctions::brightness(arg1, !q->isDark());
                }
                else if (argList.length() == 2)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    auto arg2 = numberFromIndexTable(argList.at(1));
                    out[tokenName] = Oran7ThemeFunctions::brightness(arg1, !q->isDark(), arg2);
                }
                else if (argList.length() == 3)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    auto arg2 = numberFromIndexTable(argList.at(1));
                    auto arg3 = numberFromIndexTable(argList.at(2));
                    out[tokenName] = Oran7ThemeFunctions::brightness(arg1, !q->isDark(), arg2, arg3);
                }
                else
                    ERROR_LOG << QString("func brightness() only accepts 1/2/3 parameters:(%1)").arg(args);
            }break;
            case TokenFunction::Alpha:
            {
                auto argList = args.split(',');
                if (argList.length() == 1)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    out[tokenName] = Oran7ThemeFunctions::alpha(arg1);
                }
                else if (argList.length() == 2)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0));
                    auto arg2 = numberFromIndexTable(argList.at(1));
                    out[tokenName] = Oran7ThemeFunctions::alpha(arg1, arg2);
                }
                else
                    ERROR_LOG << QString("func alpha() only accepts 1/2 parameters:(%1)").arg(args);
            }break;
            case TokenFunction::OnBackground:
            {
                auto argList = args.split(',');
                if (argList.length() == 2)
                {
                    auto arg1 = colorFromIndexTable(argList.at(0).trimmed());
                    auto arg2 = colorFromIndexTable(argList.at(1).trimmed());
                    out[tokenName] = Oran7ThemeFunctions::onBackground(arg1, arg2);
                }
                else
                    ERROR_LOG<< QString("func onBackground() only accepts 2 parameters:(%1)").arg(args);
            } break;
            case TokenFunction::Multiply:
            {
                auto argList = args.split(',');
                if (argList.length() == 2)
                {
                    auto arg1 = numberFromIndexTable(argList.at(0).trimmed());
                    auto arg2 = numberFromIndexTable(argList.at(1).trimmed());
                    out[tokenName] = Oran7ThemeFunctions::multiply(arg1, arg2);
                }
                else
                    ERROR_LOG << QString("func multiply() only accepts 2 parameters:(%1)").arg(args);
            } break;
            default:
                break;
            }
        }
        else ERROR_LOG << "Unknown func name:"<<func;
    }
    else ERROR_LOG<< "Unknown expr:" << expr;
}

/**
 * @brief Oran7ThemePrivate::colorFromIndexTable
 *              !epxr startsWith : @,#
 * @param tokenName
 * @return QColor
 */
QColor Oran7ThemePrivate::colorFromIndexTable(const QString &tokenName)
{
    QColor color = QColor(QColor::Invalid);
    if(tokenName.startsWith("@"))
    {
        QString refTokenName = tokenName.mid(1);
        if(m_indexTokenTable.contains(refTokenName)){
            auto v = m_indexTokenTable[refTokenName];
            color = v.value<QColor>();
            if(!color.isValid())
                ERROR_LOG << QString("Token toColor faild:(%1)").arg(tokenName);
        }
        else ERROR_LOG << QString("Index Token(%1) not found!").arg(refTokenName);
    }
    else if(tokenName.startsWith("#"))
    {
        if(tokenName.startsWith("#Preset_"))
            color = Oran7ColorGenerator::presetToColor(tokenName);
        else
            color = QColor(tokenName);
        if(!color.isValid())
            ERROR_LOG << QString("Token toColor faild:(%1)").arg(tokenName);
    }
    else ERROR_LOG << "Unknown startsWith of tokenName:" << tokenName;

    return color;
}

qreal Oran7ThemePrivate::numberFromIndexTable(const QString &tokenName)
{
    qreal number = 0.0;
    auto refTokenName = tokenName;
    if(refTokenName.startsWith('@'))
    {
        refTokenName = tokenName.mid(1);
        if(m_indexTokenTable.contains(refTokenName))
        {
            auto value = m_indexTokenTable[refTokenName];
            auto ok = false;
            number = value.toDouble(&ok);
            if(!ok)
                ERROR_LOG << QString("Token toDouble faild:(%1)").arg(refTokenName);
        }
        else
            ERROR_LOG << QString("Index Token(%1) not found!").arg(refTokenName);
    }
    else
    {
        auto ok = false;
        number = tokenName.toDouble(&ok);
        if(!ok)
            ERROR_LOG << QString("Token toDouble faild:(%1)").arg(tokenName);
    }

    return number;
}

void Oran7ThemePrivate::parseIndexExpr(const QString &tokenName, const QString &expr)
{
    //INFO_LOG << "parseIndexExpr: tokenName=" << tokenName << "expr=" << expr;
    if(expr.startsWith('$'))
    {
        //INFO_LOG << "  -> calling parse$ for $ expression";
        parse$(m_indexTokenTable,tokenName,expr);
    }
    else if(expr.startsWith('#'))
    {
        auto color = QColor(QColor::Invalid);

        if(expr.startsWith("#Preset_"))///*! by Preset*/
            color = Oran7ColorGenerator::presetToColor(expr);
        else
            color = QColor(expr);/*! by Color*/

        if(!color.isValid())
        {
            ERROR_LOG << "Unknown color:"<<expr;
            color = QColor(QColor::Invalid);
            return;
        }
        m_indexTokenTable[tokenName] = color;
    }
    else if(expr.startsWith('@'))
    {
        auto refTokenName = expr.mid(1);
        if(m_indexTokenTable.contains(refTokenName))
            m_indexTokenTable[tokenName] = QVariant(m_indexTokenTable[refTokenName]);
    }
    else
    {
        ///*! by String add*/
        m_indexTokenTable[tokenName] = expr;
    }
}

void Oran7ThemePrivate::parseComponentExpr(QVariantMap *tokenMapPtr, const QString &tokenName, const QVariant &expr)
{
    if(expr.toString().startsWith('@'))
    {
        auto refTokenName = expr.toString().mid(1);
        if(tokenMapPtr->contains(refTokenName))
            tokenMapPtr->insert(tokenName,(*tokenMapPtr)[refTokenName]);
        else
            ERROR_LOG << QString("Component: Token(%1):Ref(%2) not found!").arg(tokenName, refTokenName);
    }
    else if(expr.toString().startsWith('$'))
        parse$(*tokenMapPtr,tokenName,expr.toString());
    else if(expr.toString().startsWith('#'))
    {
        auto color = QColor(QColor::Invalid);
        if(expr.toString().startsWith("#Preset_"))///*! by Preset*/
            color = Oran7ColorGenerator::presetToColor(expr.toString());
        else
            color = QColor(expr.toString());/*! by Color*/
        tokenMapPtr->insert(tokenName,color);
    }
    else
    {
        tokenMapPtr->insert(tokenName,expr);
    }
}

void Oran7ThemePrivate::reloadIndexTheme()
{
    Q_Q(Oran7Theme);

    m_indexTokenTable.clear();
    q->m_Primary.clear();

    auto __init__ = m_indexJsonObject["__init__"].toObject();
    auto __base__ = __init__["__base__"].toObject();

    auto colorTextBase = __base__["colorTextBase"].toString();
    auto colorBgBase = __base__["colorBgBase"].toString();
    QStringList colorTextBaseList = colorTextBase.split("|");
    QStringList colorBgBaseList = colorBgBase.split("|");

    Q_ASSERT_X(colorTextBaseList.size() == 2,"Oran7ThemePrivate::reloadIndexTheme",
               QString("colorTextBase(%1) Must be in light:color|dark:color format").arg(colorTextBase).toStdString().c_str());
    Q_ASSERT_X(colorBgBaseList.size() == 2,"Oran7ThemePrivate::reloadIndexTheme",
               QString("colorBgBase(%1) Must be in light:color|dark:color format").arg(colorTextBase).toStdString().c_str());

    m_indexTokenTable["colorTextBase"] = q->isDark() ? colorTextBaseList.at(1) : colorTextBaseList.at(0);
    m_indexTokenTable["colorBgBase"] = q->isDark() ? colorBgBaseList.at(1) : colorBgBaseList.at(0);

    auto __vars__ = __init__["__vars__"].toObject();
    for(auto it = __vars__.constBegin(); it != __vars__.constEnd(); it++){
        auto expr = it.value().toString().simplified();
        parseIndexExpr(it.key(),expr);
    }

    /*! Index.json<__style__> => Primary */
    auto __style__ = m_indexJsonObject["__style__"].toObject();
    for(auto it = __style__.constBegin(); it != __style__.constEnd(); it++){
        auto expr = it.value().toString().simplified();
        parseIndexExpr(it.key(),expr);
    }
    for(auto it = m_indexTokenTable.constBegin(); it != m_indexTokenTable.constEnd(); it++){
        q->m_Primary[it.key()] = it.value();
    }
    emit q->PrimaryChanged();

    auto __component__ = m_indexJsonObject["__component__"].toObject();
    for(auto it = __component__.constBegin(); it != __component__.constEnd(); it++){
        registerDefaultComponentTheme(it.key(),it.value().toString());
    }
}

void Oran7ThemePrivate::reloadComponentTheme(const QMap<QObject *, Oran7ThemeData> &dataMap)
{
    for (auto &themeData: dataMap) {
        for (auto it = themeData.componentMap.constBegin(); it != themeData.componentMap.constEnd(); it++) {
            auto componentName = it.key();
            auto componentTheme = it.value();
            reloadComponentThemeFile(themeData.themeObject, componentName, componentTheme);
        }
    }
}

/**
 * @brief Oran7ThemePrivate::reloadComponentImport
 *              导出默认模板对象中的数据
 * @param style
 * @param componentName
 * @return
 */
bool Oran7ThemePrivate::reloadComponentImport(QJsonObject &style, const QString &componentName)
{
    Q_Q(Oran7Theme);

    const auto __component__ = m_indexJsonObject["__component__"].toObject();

    if(__component__.contains(componentName))
    {
        const auto themePath = __component__[componentName].toString();

        QByteArray data;
        // 优先 QRC，失败则文件系统兜底
        if (QFile theme(themePath); theme.open(QIODevice::ReadOnly)) {
            data = theme.readAll();
        } else {
            QString fileName = QFileInfo(themePath).fileName();
            QString fsPath = GlobalHelper::getConfigDir() + "/themeJson/" + fileName;
            QFile fsFile(fsPath);
            if (fsFile.open(QIODevice::ReadOnly))
                data = fsFile.readAll();
        }

        if (!data.isEmpty())
        {
            QJsonParseError error;
            QJsonDocument themeDoc = QJsonDocument::fromJson(data,&error);
            if(error.error == QJsonParseError::NoError)
            {
                const auto componentObject = themeDoc.object();
                const auto __style__ = componentObject["__style__"].toObject();

                const auto __init__ = componentObject["__init__"].toObject();
                if(__init__.contains("__vars__"))
                {
                    const auto __vars__ = __init__["__vars__"].toObject();
                    ///*! 读取 <Component>.json<__init__.__vars__> 中的变量*/
                    for(auto it = __vars__.constBegin(); it!= __vars__.constEnd(); it++){
                        style[it.key()] = it.value();
                    }
                }

                ///*! 读取 <Component>.json<__style__> 中的变量*/
                for(auto it = __style__.constBegin(); it!= __style__.constEnd(); it++){
                    style[it.key()] = it.value();
                }
            }
            else
                ERROR_LOG << QString("Parse import component theme [%1] faild:").arg(themePath) << error.errorString();
        }
        else
            ERROR_LOG << "Open import component theme faild:" << themePath
                      << "(also tried FS:"
                      << GlobalHelper::getConfigDir() + "/themeJson/" + QFileInfo(themePath).fileName() << ")";

        return true;
    }
    else return false;
}

void Oran7ThemePrivate::reloadComponentThemeFile(QObject *themeObject, const QString &componentName, const Oran7ThemeData::Component &componentTheme)
{
    Q_Q(Oran7Theme);

    auto tokenMapPtr = componentTheme.tokenMap;
    auto installTokenMap = componentTheme.installTokenMap;

    auto style = QJsonObject();
    bool importSuccess = reloadComponentImport(style,componentName);
    if(!importSuccess){
        ERROR_LOG << "  Failed to reload component import for" << componentName;
        return;
    }

    if(importSuccess)
    {
         /*! @note Clear before parsing*/
        tokenMapPtr->clear();

        for(auto it = style.constBegin(); it != style.constEnd(); it++){
            parseComponentExpr(tokenMapPtr,it.key(),it.value().toString().simplified());
        }

        /*! 读取通过 @link  //d->reloadCustomComponentTheme();() 安装的变量, 存在则覆盖, 否则添加 */
        for(auto it = installTokenMap.constBegin(); it != installTokenMap.constEnd(); it++){
            parseComponentExpr(tokenMapPtr,it.key(),it.value());
        }

        auto signalName = componentName + "Changed";
        //INFO_LOG << "  Invoking signal:" << signalName;
        /*! @note 使用 QMetaMethod::invoke 发射信号，触发 QML 绑定更新*/
        QMetaMethod signalMethod = themeObject->metaObject()->method(
            themeObject->metaObject()->indexOfSignal(QMetaObject::normalizedSignature(
                (signalName + "()").toUtf8().constData())));
        signalMethod.invoke(themeObject, Qt::AutoConnection);
    }
}

void Oran7ThemePrivate::reloadDefaultComponentTheme()
{
    Q_Q(Oran7Theme);

    reloadComponentTheme(m_defaultTheme);

    // // 应用用户覆盖
    // if (!m_userComponentOverrides.isEmpty()) {
    //     for (auto it = m_userComponentOverrides.constBegin(); it != m_userComponentOverrides.constEnd(); ++it) {
    //         QString componentName = it.key();
    //         const QMap<QString, QVariant>& overrides = it.value();

    //         // 应用用户覆盖到 installTokenMap
    //         for (auto overrideIt = overrides.constBegin(); overrideIt != overrides.constEnd(); ++overrideIt) {
    //             m_defaultTheme[q].componentMap[componentName].installTokenMap[overrideIt.key()] = overrideIt.value();
    //             INFO_LOG << "  Applied " << overrideIt.key() <<","<< overrideIt.value() << " user overrides to" << componentName << "in default theme";
    //         }
    //     }

    //     // 重新加载以应用覆盖
    //     reloadComponentTheme(m_defaultTheme);
    // }
}

void Oran7ThemePrivate::reloadCustomComponentTheme()
{
    Q_Q(Oran7Theme);

    // 确保所有默认组件也注册到 m_customTheme，以便应用用户覆盖
    for (auto it = g_componentTable->constBegin(); it != g_componentTable->constEnd(); ++it) {
        QString componentName = it.key();
        if (!m_customTheme.contains(q)) {
            m_customTheme[q] = Oran7ThemeData{};
        }
        if (!m_customTheme[q].componentMap.contains(componentName)) {
            m_customTheme[q].componentMap[componentName].tokenMap = it.value();
            m_customTheme[q].componentMap[componentName].path = m_indexJsonObject["__component__"][componentName].toString();
        }
    }

    // // 应用用户覆盖到 m_customTheme
    // if (!m_userComponentOverrides.isEmpty()) {
    //     for (auto it = m_userComponentOverrides.constBegin(); it != m_userComponentOverrides.constEnd(); ++it) {
    //         QString componentName = it.key();
    //         const QMap<QString, QVariant>& overrides = it.value();

    //         // 应用用户覆盖到 installTokenMap
    //         for (auto overrideIt = overrides.constBegin(); overrideIt != overrides.constEnd(); ++overrideIt) {
    //             m_customTheme[q].componentMap[componentName].installTokenMap[overrideIt.key()] = overrideIt.value();
    //         }

    //         INFO_LOG << "  Applied" << overrides.size() << "user overrides to" << componentName;
    //     }
    // }

    reloadComponentTheme(m_customTheme);
}

void Oran7ThemePrivate::registerDefaultComponentTheme(const QString &component, const QString &themePath)
{
    Q_Q(Oran7Theme);

    if(g_componentTable->contains(component))
    {
        registerComponentTheme(q,component,g_componentTable->value(component),themePath,m_defaultTheme);
    }
}

void Oran7ThemePrivate::registerComponentTheme(QObject *themeObject, const QString &component, QVariantMap *themeMap, const QString &themePath, QMap<QObject *, Oran7ThemeData> &dataMap)
{
    if(!themeObject || !themeMap) return;

    if(!dataMap.contains(themeObject))
        //New one
        dataMap[themeObject] = Oran7ThemeData{};

    //Link
    if(dataMap.contains(themeObject)){
        dataMap[themeObject].themeObject = themeObject;
        dataMap[themeObject].componentMap[component].path = themePath;
        dataMap[themeObject].componentMap[component].tokenMap = themeMap;
    }
}

void Oran7ThemePrivate::mergeJsonObject(QJsonObject& target, const QJsonObject& source)
{
    for (auto it = source.begin(); it != source.end(); ++it) {
        const QString& key = it.key();
        const QJsonValue& sourceValue = it.value();

        if (target.contains(key)) {
            QJsonValue targetValue = target.value(key);

            // 如果两边都是对象，递归合并
            if (targetValue.isObject() && sourceValue.isObject()) {
                QJsonObject merged = targetValue.toObject();
                mergeJsonObject(merged, sourceValue.toObject());
                target[key] = merged;
            } else {
                // 否则直接覆盖
                target[key] = sourceValue;
            }
        } else {
            target[key] = sourceValue;
        }
    }
}

void Oran7ThemePrivate::loadCurrentThemeProfile()
{
    Q_Q(Oran7Theme);

    /*! @note 加载用户主题覆盖 */
    QJsonObject userOverride = Oran7ThemeProfileManager::instance().loadThemeProfile(m_currentThemeProfile);

    INFO_LOG << "loadCurrentThemeProfile: profile=" << m_currentThemeProfile << "userOverride.isEmpty()=" << userOverride.isEmpty();

    if (!userOverride.isEmpty()) {
        INFO_LOG << "  userOverride keys:" << userOverride.keys();

        /*! @note 单独处理 __component__，不直接合并到 m_indexJsonObject */
        QJsonObject componentOverrides;
        if (userOverride.contains("__component__")) {
            componentOverrides = userOverride["__component__"].toObject();
            INFO_LOG << "  __component__ in userOverride:" << componentOverrides.keys();
            userOverride.remove("__component__");
        }

        /*! @note 合并其他字段到 m_indexJsonObject */
        if (!userOverride.isEmpty()) {
            mergeJsonObject(m_indexJsonObject, userOverride);
        }

       /*! @note 处理组件覆盖：将用户主题的 __style__ && __vars__ 添加到对应组件的 installTokenMap */
        for (auto compIt = componentOverrides.constBegin(); compIt != componentOverrides.constEnd(); ++compIt)
        {
            QString componentName = compIt.key();
            QJsonObject userCompObj = compIt.value().toObject();

            INFO_LOG << "  Processing component override:" << componentName;

            /*! @note 获取用户主题中的 __style__ */
            QJsonObject __style__{};
            if (userCompObj.contains("__style__")) {
                __style__ = userCompObj["__style__"].toObject();
            }

            if (__style__.isEmpty()) {
                INFO_LOG << "    No __style__ overrides found";
            }

            /*! @note 存储用户覆盖到 m_defaultTheme的installTokenMap，供 reloadCustomComponentTheme 使用 */
            if (!__style__.isEmpty()) {
                for (auto It = __style__.constBegin(); It != __style__.constEnd(); ++It) {
                    m_defaultTheme[q].componentMap[componentName].installTokenMap[It.key()] = It->toVariant();
                    INFO_LOG << "    Stored:" << It.key()<<","
                             <<m_defaultTheme[q].componentMap[componentName].installTokenMap[It.key()] << "overrides for" << componentName;
                }
            }

            /*! @note 获取用户主题中的 __vars__ */
            QJsonObject __vars__{};
            if(userCompObj.contains("__init__")){
                QJsonObject __init__ = userCompObj["__init__"].toObject();
                if(__init__.contains("__vars__")){
                    __vars__ = __init__["__vars__"].toObject();
                }
            }
            if(__vars__.isEmpty()){
                INFO_LOG << "    No __vars__ overrides found";
                continue;
            }
            ///*! @note 存储用户覆盖到 m_defaultTheme的installTokenMap，供 reloadCustomComponentTheme 使用 */
            if(!__vars__.isEmpty()){
                for(auto It = __vars__.constBegin(); It!=__vars__.constEnd(); It++){
                    m_defaultTheme[q].componentMap[componentName].installTokenMap[It.key()] = It->toVariant();
                    INFO_LOG << "    Stored:" << It.key()<<","
                             <<m_defaultTheme[q].componentMap[componentName].installTokenMap[It.key()] << "overrides for" << componentName;
                }
            }
        }
    }
}

Oran7Theme::~Oran7Theme()
{
    INFO_LOG << "[DTOR] Oran7Theme::~Oran7Theme() finished.";
}

Oran7Theme& Oran7Theme::instance()
{
    if(s_instance == nullptr)
        s_instance = new Oran7Theme;
    return *s_instance;
}

Oran7Theme *Oran7Theme::create(QQmlEngine *, QJSEngine *)
{
    return &instance();
}

bool Oran7Theme::isDark() const
{
    Q_D(const Oran7Theme);

    if(d->m_darkMode == DarkMode::System){
        return false;
    }
    else
    {
        return d->m_darkMode == DarkMode::Dark;
    }
}

void Oran7Theme::setDarkMode(const DarkMode mode)
{
    Q_D(Oran7Theme);

    if(d->m_darkMode != mode){
        auto oldIsDark = isDark();
        d->m_darkMode = mode;
        if(oldIsDark != isDark())
        {
            d->reloadIndexTheme();
            d->reloadDefaultComponentTheme();
             //d->reloadCustomComponentTheme();
            emit isDarkChanged();
        }
        emit darkModeChanged();
    }
}

QVariantMap Oran7Theme::sizeHint() const
{
    Q_D(const Oran7Theme);

    return d->m_sizeHintMap;
}

Oran7Theme::DarkMode Oran7Theme::darkMode() const
{
    Q_D(const Oran7Theme);

    return d->m_darkMode;
}

void Oran7Theme::registerCustomComponentTheme(QObject *themeObject, const QString &component, QVariantMap *themeMap, const QString &themePath)
{
    Q_D(Oran7Theme);

    d->registerComponentTheme(themeObject,component,themeMap,themePath,d->m_customTheme);
}

void Oran7Theme::reloadTheme()
{
    Q_D(Oran7Theme);

    // 优先 QRC，失败则从文件系统 ../Config/themeJson/ 兜底
    QByteArray raw;

    if (QFile index(d->themIndexJsonPath); index.open(QIODevice::ReadOnly)) {
        raw = index.readAll();
    } else {
        QString fsPath = GlobalHelper::getConfigDir() + "/themeJson/Index.json";
        QFile fsFile(fsPath);
        if (fsFile.open(QIODevice::ReadOnly)) {
            raw = fsFile.readAll();
            INFO_LOG << "Oran7Theme: Loaded Index.json from filesystem:" << fsPath;
        }
    }

    if (!raw.isEmpty()) {
        QJsonParseError error;
        QJsonDocument indexDoc = QJsonDocument::fromJson(raw, &error);
        if (error.error == QJsonParseError::NoError) {
            d->m_indexJsonObject = indexDoc.object();
            d->reloadIndexTheme();
            Oran7ThemeProfileManager::instance().initializeDefaultTheme(this);
            d->loadCurrentThemeProfile();
            d->reloadDefaultComponentTheme();
        } else {
            ERROR_LOG << "Index.json parse error:" << error.errorString();
        }
    } else {
        ERROR_LOG << "Index.json file open failed (QRC:"
                   << d->themIndexJsonPath << ", FS:"
                   << GlobalHelper::getConfigDir() + "/themeJson/Index.json" << ")";
    }

#if 0
    // 输出 m_indexTokenTable 的所有键值
    qDebug()<<"\n";

    INFO_LOG << "=== m_indexTokenTable ===";
    for (auto it = d->m_indexTokenTable.constBegin(); it != d->m_indexTokenTable.constEnd(); ++it) {
        INFO_LOG << it.key() << ":" << it.value();
    }

    qDebug()<<"\n";

    // 输出 g_componentTable 中每个组件的 tokenMap 的所有键值
    INFO_LOG << "=== g_componentTable ===";
    for (auto it = g_componentTable->constBegin(); it != g_componentTable->constEnd(); ++it) {
        INFO_LOG << "Component:" << it.key();
        if (it.value()) {
            for (auto mapIt = it.value()->constBegin(); mapIt != it.value()->constEnd(); ++mapIt) {
                INFO_LOG << "  " << mapIt.key() << ":" << mapIt.value();
            }
        }
    }
    qDebug()<<"\n";
#endif
}

void Oran7Theme::installThemeColorTextBase(const QString &lightAndDark)
{
    Q_D(Oran7Theme);

    auto __init__ = d->m_indexJsonObject["__init__"].toObject();
    auto __base__ = __init__["__base__"].toObject();

    __base__["colorTextBase"] = lightAndDark.simplified();
    __init__["__base__"] = __base__;
    d->m_indexJsonObject["__init__"] = __init__;

    d->reloadIndexTheme();
    d->reloadDefaultComponentTheme();
     //d->reloadCustomComponentTheme();
}

void Oran7Theme::installThemeColorBgBase(const QString &lightAndDark)
{
    Q_D(Oran7Theme);

    auto __init__ = d->m_indexJsonObject["__init__"].toObject();
    auto __base__ = __init__["__base__"].toObject();

    __base__["colorBgBase"] = lightAndDark.simplified();
    __init__["__base__"] = __base__;
    d->m_indexJsonObject["__init__"] = __init__;

    d->reloadIndexTheme();
    d->reloadDefaultComponentTheme();
     //d->reloadCustomComponentTheme();
}

void Oran7Theme::installThemePrimaryColorBase(const QColor &colorBase)
{
    Q_D(Oran7Theme);

    installIndexToken("colorPrimaryBase",QString("$genColor(%1)").arg(colorBase.name()));
}

void Oran7Theme::installThemePrimaryFontSizeBase(int fontSizeBase)
{
    Q_D(Oran7Theme);

    installIndexToken("fontSizeBase",QString("$genFontSize(%1)").arg(fontSizeBase));
}

void Oran7Theme::installThemePrimaryFontFamiliesBase(const QString &familiesBase)
{
    Q_D(Oran7Theme);

    installIndexToken("fontFamilyBase",QString("$genFontFamily(%1)").arg(familiesBase));
}

void Oran7Theme::installThemePrimaryRadiusBase(int radiusBase)
{
    Q_D(Oran7Theme);

    installIndexToken("radiusBase",QString("$genRadisu(%1)").arg(radiusBase));
}

void Oran7Theme::installThemePrimaryAnimationBase(int durationFast, int durationMid, int durationSlow)
{
    Q_D(Oran7Theme);

    auto __style__ = d->m_indexJsonObject["__style__"].toObject();
    __style__["durationFast"] = QString::number(durationFast);
    __style__["durationMid"] = QString::number(durationMid);
    __style__["durationSlow"] = QString::number(durationSlow);

    d->reloadIndexTheme();
    d->reloadDefaultComponentTheme();
     //d->reloadCustomComponentTheme();
}

void Oran7Theme::installSizeHintRatio(const QString &size, qreal ratio)
{
    Q_D(Oran7Theme);

    bool change = false;
    if(d->m_sizeHintMap.contains(size))
    {
        auto value = d->m_sizeHintMap[size].toDouble();
        if(!qFuzzyCompare(value,ratio))
            change = true;
    }
    else change = true;

    if(change)
    {
        d->m_sizeHintMap[size] = ratio;
        emit sizeHintChanged();
    }
}

void Oran7Theme::installIndexTheme(const QString &themePath)
{
    Q_D(Oran7Theme);

    if(themePath != d->themIndexJsonPath)
    {
        if(themePath.isEmpty())
            d->themIndexJsonPath = ":/src/resource/themeJson/Index.json";
        else
            d->themIndexJsonPath = themePath;

        reloadTheme();
    }
}

void Oran7Theme::installIndexToken(const QString &token, const QString &value)
{
    Q_D(Oran7Theme);

    auto __init__ = d->m_indexJsonObject["__init__"].toObject();
    auto __vars__ = __init__["__vars__"].toObject();
    __vars__[token] = value.simplified();
    __init__["__vars__"] = __vars__;
    d->m_indexJsonObject["__init__"] = __init__;

    d->reloadIndexTheme();
    d->reloadDefaultComponentTheme();
    //d->reloadCustomComponentTheme();
}

void Oran7Theme::installComponentTheme(const QString &component, const QString &themePath)
{
    Q_D(Oran7Theme);

    auto __component__ = d->m_indexJsonObject["__component__"].toObject();
    if(__component__.contains(component))
    {
        __component__[component] = themePath;
        d->m_indexJsonObject["__component__"] = __component__;
        d->reloadDefaultComponentTheme();
    }
    else
        ERROR_LOG << QString("Component [%1] not found!").arg(component);
}

/**
 * @brief Oran7Theme:: //d->reloadCustomComponentTheme();
 *            Updata theme data of memory -->
 * @note this api will not save the config,but only change the memory config
 * @param component
 * @param token
 * @param value
 */
void Oran7Theme::installComponentToken(const QString &component, const QString &token, const QVariant &value)
{
    Q_D(Oran7Theme);

    for (auto &theme: d->m_defaultTheme) {
        if (theme.componentMap.contains(component)) {
            theme.componentMap[component].installTokenMap.insert(token, value);
            d->reloadComponentThemeFile(theme.themeObject, component, theme.componentMap[component]);
            return;
        }
    }

    // for (auto &theme: d->m_customTheme) {
    //     if (theme.componentMap.contains(component)) {
    //         theme.componentMap[component].installTokenMap.insert(token, value);
    //         d->reloadComponentThemeFile(theme.themeObject, component, theme.componentMap[component]);
    //         return;
    //     }
    // }

    ERROR_LOG << QString("Component [%1] not found!").arg(component);
}

void Oran7Theme::installThemeComponentColorBase(const QString &component,const QColor &colorBase)
{
    Q_D(Oran7Theme);

    installComponentToken(component,"colorPrimaryBase",QString("$genColor(%1)").arg(colorBase.name()));
}

Oran7Theme::Oran7Theme(QObject *parent)
    : d_ptr(new Oran7ThemePrivate(this))
{
    Q_D(Oran7Theme);

    d->initializeComponentPropertyHash();

    // 从 AppConfigManager 读取上次使用的主题配置
    QString savedProfile = AppConfigManager::ins().getValueQVariant("appearance.themeProfile", "profile-default").toString();
    d->m_currentThemeProfile = savedProfile;

    d->m_sizeHintMap["small"] = 0.8;
    d->m_sizeHintMap["normal"] = 1.0;
    d->m_sizeHintMap["large"] = 1.25;

    reloadTheme();  // reloadTheme 中会调用 loadCurrentThemeProfile

#if DEVELOPER_MODE
    // 输出 m_indexTokenTable 的所有键值
    qDebug()<<"\n";

    INFO_LOG << "=== m_indexTokenTable ===";
    for (auto it = d->m_indexTokenTable.constBegin(); it != d->m_indexTokenTable.constEnd(); ++it) {
        INFO_LOG << it.key() << ":" << it.value();
    }

    qDebug()<<"\n";

    // 输出 g_componentTable 中每个组件的 tokenMap 的所有键值
    INFO_LOG << "=== g_componentTable ===";
    for (auto it = g_componentTable->constBegin(); it != g_componentTable->constEnd(); ++it) {
        INFO_LOG << "Component:" << it.key();
        if (it.value()) {
            for (auto mapIt = it.value()->constBegin(); mapIt != it.value()->constEnd(); ++mapIt) {
                INFO_LOG << "  " << mapIt.key() << ":" << mapIt.value();
            }
        }
    }
    qDebug()<<"\n";
#endif
}

/*! @note ==================== 主题配置管理 ====================*/

QString Oran7Theme::currentThemeProfile() const
{
    Q_D(const Oran7Theme);
    return d->m_currentThemeProfile;
}

void Oran7Theme::setCurrentThemeProfile(const QString& profileName)
{
    Q_D(Oran7Theme);

    if (d->m_currentThemeProfile != profileName) {
        d->m_currentThemeProfile = profileName;

        AppConfigManager::ins().setValueQVariant("appearance.themeProfile", profileName);
        AppConfigManager::ins().saveConfig();

        d->loadCurrentThemeProfile();
        reloadTheme();

        emit currentThemeProfileChanged();
    }
}

QStringList Oran7Theme::availableThemeProfiles() const
{
    return Oran7ThemeProfileManager::instance().getAvailableThemeProfiles();
}

/**
 * @brief Oran7Theme::saveComponentToken
 * 这里是设置对应Component属性的总入口：
 * 1.读取现有配置文件theme-profile，获取临时对象userTheme
 * 2.保存value到临时主题配置userTheme对象中的对应的token位置
 * 3.写入后userTheme对象后，保存更改后的配置，写入到m_currentThemeProfile文件中，并保存
 * 4.同时加载内存中的配置到tokenMapPtr指向的对应组件的属性的内存中，通过 //d->reloadCustomComponentTheme();
 * @param component
 * @param token
 * @param value
 */
void Oran7Theme::saveComponentToken(const QString& component, const QString& token, const QVariant& value)
{
    Q_D(Oran7Theme);

    QJsonObject userTheme = Oran7ThemeProfileManager::instance().loadThemeProfile(d->m_currentThemeProfile);

    /*! @note Kit Path: __component__.ComponentName.__style__.token */
    if (!userTheme.contains("__component__")) {
        userTheme["__component__"] = QJsonObject();
    }

    QJsonObject components = userTheme["__component__"].toObject();
    if (!components.contains(component)) {
        /*! @note load defalut component*/
        QString defaultPath = d->m_indexJsonObject["__component__"][component].toString();
        if (!defaultPath.isEmpty()) {
            QFile file(defaultPath);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                components[component] = doc.object();
            }
        }
    }
    QJsonObject compObj = components[component].toObject();

    if(compObj.contains("__init__")){
        QJsonObject __init__ = compObj["__init__"].toObject();
        if(__init__.contains("__vars__")){
            QJsonObject __vars__ = __init__["__vars__"].toObject();
            if(__vars__.contains(token))
            {
                __vars__[token] = QJsonValue::fromVariant(value);
                __init__["__vars__"] = __vars__;
                compObj["__init__"] = __init__;
                components[component] = compObj;
                userTheme["__component__"] = components;
            }//else WARNING_LOG<<"Can't save config:"<<component<<"."<<token<<"."<<value;
        }
    }
    if (compObj.contains("__style__")) {
        QJsonObject __style__ = compObj["__style__"].toObject();
        if(__style__.contains(token))
        {
            __style__[token] = QJsonValue::fromVariant(value);
            compObj["__style__"] = __style__;
            components[component] = compObj;
            userTheme["__component__"] = components;
        }//else WARNING_LOG<<"Can't save config:"<<component<<"."<<token<<"."<<value;
    }

    if (Oran7ThemeProfileManager::instance().saveThemeProfile(d->m_currentThemeProfile, userTheme)) {
        /*! @note 应用到内存中Oran7ThemeData的installTokenMap并加载到组件tokenMap指向的内存中*/
        installComponentToken(component,token,value);
        CONFIG_LOG << "Config update:"<<component<<"."<<token<<"."<<value;
    }
}

void Oran7Theme::saveIndexToken(const QString& token, const QVariant& value)
{
    Q_D(Oran7Theme);

    QJsonObject userTheme = Oran7ThemeProfileManager::instance().loadThemeProfile(d->m_currentThemeProfile);

    QJsonObject styleObj;
    if (userTheme.contains("__style__")) {
        styleObj = userTheme["__style__"].toObject();
    }

    styleObj[token] = QJsonValue::fromVariant(value);
    userTheme["__style__"] = styleObj;

    if (Oran7ThemeProfileManager::instance().saveThemeProfile(d->m_currentThemeProfile, userTheme)) {
        // 使用 installIndexToken 应用即时更改
        installIndexToken(token, value.toString());
    }
}

bool Oran7Theme::createThemeProfile(const QString& profileName, const QString& sourceProfile)
{
    QJsonObject sourceTheme = Oran7ThemeProfileManager::instance().loadThemeProfile(sourceProfile);
    return Oran7ThemeProfileManager::instance().saveThemeProfile(profileName, sourceTheme);
}

bool Oran7Theme::deleteThemeProfile(const QString& profileName)
{
    if (Oran7ThemeProfileManager::instance().deleteThemeProfile(profileName)) {
        emit availableThemeProfilesChanged();
        return true;
    }
    return false;
}

bool Oran7Theme::duplicateThemeProfile(const QString& sourceName, const QString& newName)
{
    if (Oran7ThemeProfileManager::instance().duplicateThemeProfile(sourceName, newName)) {
        emit availableThemeProfilesChanged();
        return true;
    }
    return false;
}

bool Oran7Theme::exportThemeProfile(const QString& profileName, const QString& filePath)
{
    return Oran7ThemeProfileManager::instance().exportThemeProfile(profileName, filePath);
}

QString Oran7Theme::importThemeProfile(const QString& filePath, const QString& newName)
{
    QString result = Oran7ThemeProfileManager::instance().importThemeProfile(filePath, newName);
    if (!result.isEmpty()) {
        emit availableThemeProfilesChanged();
    }
    return result;
}

void Oran7Theme::resetToDefaultTheme()
{
    Q_D(Oran7Theme);

    // 清空当前主题覆盖（保存为空对象）
    Oran7ThemeProfileManager::instance().saveThemeProfile(d->m_currentThemeProfile, QJsonObject());

    // 重新加载主题
    d->loadCurrentThemeProfile();
    reloadTheme();
}
