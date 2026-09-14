#ifndef NCMPROTOCOL_H
#define NCMPROTOCOL_H

#include "NcmRequest.h"

#include <QJsonObject>
#include <QString>

enum class NcmApiType
{
    WEAPI,
    EAPI // 预留，v1 未实现
};

/**
 * @brief 网易云协议层：唯一知道 WEAPI URL + body 构造的地方
 *
 * 业务/API 层只写 "路径 + payload"，
 * 这里负责把内部路径转成 https://music.163.com/weapi/... 并加密出 form body。
 */
class NcmProtocol
{
public:
    NcmRequest prepare(NcmApiType type, const QString& path, const QJsonObject& payload) const;
    NcmRequest prepareWeapi(const QString& path, const QJsonObject& payload) const;
    // 未来: NcmRequest prepareEapi(...)

private:
    static const QString kBaseUrl; // https://music.163.com
};

#endif // NCMPROTOCOL_H
