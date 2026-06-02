#ifndef BILIBILIAUTHMANAGER_H
#define BILIBILIAUTHMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

/**
 * @brief B站扫码登录认证管理器
 *
 * 通过B站QR码登录流程获取用户Cookie（SESSDATA、DedeUserID、bili_jct），
 * 提供给 BilibiliRoomAddressCatch 和 FFPlayer 使用，以获取原画画质直播流。
 *
 * 使用方式：
 *   QML:  BilibiliAuthManager.startLogin() / .isLoggedIn / .loginState
 *   C++:  BilibiliAuthManager::instance().cookieString() / .ffmpegCookieString()
 */
class BilibiliAuthManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool isLoggedIn READ isLoggedIn NOTIFY loginStatusChanged FINAL)
    Q_PROPERTY(QString userName READ userName NOTIFY loginStatusChanged FINAL)
    Q_PROPERTY(QString qrCodeUrl READ qrCodeUrl NOTIFY qrCodeGenerated FINAL)
    Q_PROPERTY(LoginState loginState READ loginState NOTIFY loginStateChanged FINAL)

public:
    /**
     * @brief 登录流程状态枚举
     */
    enum class LoginState {
        Idle,           ///< 空闲，无登录进行中
        Generating,     ///< 正在请求QR码
        WaitingScan,    ///< QR码已生成，等待扫码
        Scanned,        ///< 已扫码，等待手机确认
        Success,        ///< 登录成功
        Expired,        ///< QR码已过期
        Error           ///< 网络或API错误
    };
    Q_ENUM(LoginState)

    /**
     * @brief 获取单例实例
     */
    static BilibiliAuthManager& instance();

    /**
     * @brief 安全销毁单例（仿照Oran7Theme::cleanup，在qAddPostRoutine中调用）
     */
    static void cleanup();

    /**
     * @brief 启动QR码登录流程
     * 调用B站API生成QR码，成功后发射 qrCodeGenerated 信号
     */
    Q_INVOKABLE void startLogin();

    /**
     * @brief 取消正在进行的登录流程
     */
    Q_INVOKABLE void cancelLogin();

    /**
     * @brief 登出并清除存储的Cookie
     */
    Q_INVOKABLE void logout();

    // --- Cookie访问方法 ---

    /**
     * @brief 获取HTTP请求用的Cookie字符串
     * @return 格式: "SESSDATA=xxx; DedeUserID=xxx; bili_jct=xxx"，未登录返回空
     */
    QString cookieString() const;

    /**
     * @brief 获取FFmpeg AVDictionary用的Cookie字符串（格式同cookieString）
     */
    QString ffmpegCookieString() const;

    // --- 属性读取 ---

    bool isLoggedIn() const;
    const QString& userName() const;
    const QString& qrCodeUrl() const;
    LoginState loginState() const;

    // --- 持久化 ---

    /**
     * @brief 从AppConfigManager加载Cookie，并校验有效性
     */
    void loadCookies();

    /**
     * @brief 将Cookie保存到AppConfigManager
     */
    void saveCookies();

signals:
    void loginStatusChanged();
    void loginStateChanged();
    void qrCodeGenerated();
    void loginError(const QString& message);

private:
    explicit BilibiliAuthManager(QObject* parent = nullptr);
    ~BilibiliAuthManager() override;

    // 禁止拷贝
    BilibiliAuthManager(const BilibiliAuthManager&) = delete;
    BilibiliAuthManager& operator=(const BilibiliAuthManager&) = delete;

    // --- 内部方法 ---

    /**
     * @brief 设置登录状态并发射信号
     */
    void setLoginState(LoginState state);

    /**
     * @brief QR码生成API回调
     */
    void onQrCodeReply(QNetworkReply* reply);

    /**
     * @brief 轮询扫码状态
     */
    void pollLoginStatus();

    /**
     * @brief 轮询API回调
     */
    void onPollReply(QNetworkReply* reply);

    /**
     * @brief 从 data.url 中提取Cookie参数
     */
    void extractCookiesFromUrl(const QString& url);

    /**
     * @brief 启动时校验已存储Cookie的有效性
     */
    void validateStoredCookies();

    /**
     * @brief 校验API回调
     */
    void onValidateReply(QNetworkReply* reply);

    // --- 成员变量 ---

    QNetworkAccessManager* m_networkManager;
    QTimer* m_pollTimer;          ///< QR码状态轮询定时器（3秒间隔）
    QString m_qrcodeKey;          ///< 当前QR码的key，用于轮询
    QString m_qrCodeUrl;          ///< QR码图片URL
    LoginState m_loginState = LoginState::Idle;

    // 存储的Cookie
    QString m_sessdata;
    QString m_dedeUserId;
    QString m_biliJct;
    QString m_userName;
    bool m_isLoggedIn = false;
};

#endif // BILIBILIAUTHMANAGER_H
