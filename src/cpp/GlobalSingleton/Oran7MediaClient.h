#ifndef ORAN7MEDIACLIENT_H
#define ORAN7MEDIACLIENT_H

#include "Oran7MediaPlayer.h"
#include "D3d11Videoitem.h"
#include "GlobalHelper.h"
#include "Oran7ScreenCapture.h"
#include "OnlineLyricsFetcher.h"

#include <QMutex>
#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QMap>
#include <QSet>
#include <QHash>
#include <QNetworkAccessManager>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QQmlEngine>

#include <atomic>
#include <memory>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QDir>
#include <QMutexLocker>
#include <QQmlApplicationEngine>

enum ProtocolHandlerCMD_{
    Login_=0x01,
    LoadingData_=0x02
};

/*! @brief 客户端协议处理器函数包*/
using Packedhandler_ = std::function<void(quint32,quint8,const QByteArray &)>;

class Client : public QObject
{
    Q_OBJECT
public:
    explicit Client(const QString &host,quint16 port,QObject *parent = nullptr);
    ~Client();
    /*! @brief 外部调用强制清理播放器内核*/
    void stopPlayerRunning()
    {
        shutting_down_ = true;
        this->onStop();
    }
    /*! @brief 初始化连接客户端各种预先信号处理槽函数*/
    int InitSignalsAndSlots();

    /*! @brief 注册客户端响应服务器协议处理器方法*/
    void registerHandler_(quint8 opcode,Packedhandler_ handler);

    //<<---------------<<登录>>---------------------------->>

    /*! @brief QML前端调用登录验证*/
    Q_INVOKABLE void validateLogin(QString username,QString password);

    /*! @brief 处理登录响应*/
    void loginResponse(quint32 totalLength,quint8 opcode,const QByteArray &packet);

    //<<---------------<<加载缓存云端数据>>--------------->>

    /*! @brief 加载云端用户数据询问*/
    void loadingSeverUserDataQuery();
    /*! @brief 解析服务器云端数据传输协议响应*/
    void explainSeverUserData(quint32 totalLength, quint8 opcode, const QByteArray &packet);

    //===============================<Client Public function>==========================//

    /*! @brief 创建AppData临时文件夹目录，若已有则会自动返回默认目录*/
    QString createAppDirectories();

    /*! @brief 分析媒体文件信息，内部使用ffprobe和ffmpeg.exe*/
    QMap<QString, QVariant> analyzeMediaFileInfo(QString filepath);

    /*! @brief 防止后端频繁响应前端请求信号的节流计时器*/
    QElapsedTimer m_callTimer;
    static const int CALL_INTERVAL_MS = 100;

    /*! @brief 触发LocalMusicList前端列表刷新*/
    void refreshLocalMusicList();
    /*! @brief 异步扫描本地音乐文件夹（Oran7Async 线程池），完成后逐条发 localMusicElementReady */
    Q_INVOKABLE void searchLocalMediaFiles(const QString& folderPath);
    /*! @brief 获取当前正在播放的媒体文件路径*/
    Q_INVOKABLE QString getCurrentMediaFilePath(){return appData.CurMediaFilePath;}
    /*! @brief 更新当前聚焦的媒体文件路径*/
    Q_INVOKABLE void updateFocusCurMediaFile(QString filepath);
    /*! @brief QML前端调用，请求添加本地音乐文件*/
    Q_INVOKABLE void addNewLocalMusic(const QVariantList &fileList);
    /*! @brief QML前端调用，请求删除本地音乐文件（异步）*/
    Q_INVOKABLE void deleteLocalMusicFiles(const QVariantList& fileList);

    /*! @brief 按指定路径顺序排序QFileInfo列表*/
    QList<QFileInfo> sortFileInfoList_byFilePaths(const QList<QFileInfo> &fileInfoList , const QVariantList& orderPaths);

    //-----------------------------------------------------<Config-->Load-Save接口>-----------------------------------------------------------//
    /*! @brief 获取配置文件记录的上次关闭应用前最后播放的music，并加载*/
    void loadConfig_lastCloseAppFocusedMusic();
    /*! @brief 持久化的网络歌元数据缺专辑/时长时,按 songId 重新拉取并补发 focusedMusicRestored*/
    void refetchLastMediaDetail();
    /*! @brief 加载config中保存的LocalMusicStack播放顺序，协商排列顺序*/
    void loadConfig_localMusicList_playOrder();
    /*! @brief 加载ApplicationWindow的width和height配置*/
    void loadConfig_AppWindowSize(QQmlApplicationEngine &engine);
    /*! @brief 加载ApplicationWindow的x和y位置配置*/
    void loadConfig_AppWindowPosition(QQmlApplicationEngine &engine);
    /*! @brief 加载App保存的音量设置*/
    void loadConfig_AppSetPlayerVolume();

    /*! @brief 将上次关闭应用前最后播放的music文件路径更新到配置文件*/
    void saveConfig_lastCloseAppFocusedMusic();
    /*! @brief 保存LocalMusicStack播放顺序到config文件，用于下次启动时协商排序加载*/
    void saveConfig_localMusicList_playOrder();
    /*! @brief 保存ApplicationWindow的width和height到配置文件*/
    void saveConfig_AppWindowSize(QQmlApplicationEngine &engine);
    /*! @brief 保存ApplicationWindow的x和y位置到配置文件*/
    void saveConfig_AppWindowPosition(QQmlApplicationEngine &engine);
    /*! @brief 保存App音量配置到配置文件*/
    void saveConfig_AppSetPlayerVolume();

    //===============================<Control Oran7MediaPlayer接口>===============================//
    /*! @brief Oran7MediaPlayer与内层FFPlay跨线程通信接口工具*/
    int message_loop(void *arg);
    /*! @brief VideoFrame渲染接口，在FFPlayer内层Video_refresh_thread中回调*/
    int OutputVideo(const Frame *frame,AVFrame* copy_frame);
    /*! @brief QML前端调用，music列表点击播放事件*/
    Q_INVOKABLE void requestPlayMusic(QString file_path);
    /*! @brief QML前端播放时注入当前歌曲的网易云 songId（会员歌拉歌词用）*/
    Q_INVOKABLE void setCurrentMediaSongId(qint64 songId);
    /*! @brief QML前端播放时注入当前歌曲是否为会员（VIP）歌曲（最近播放记录用）*/
    Q_INVOKABLE void setCurrentMediaIsVip(bool isVip);
    /*! @brief QML前端播放网络歌曲时保存其元数据（启动恢复上次播放的网络歌用）*/
    Q_INVOKABLE void rememberLastMedia(const QString &name, const QString &artist,
                                       const QString &album, const QString &cover,
                                       const QString &timesize, qint64 songId, bool isVip);
    /*! @brief 播放网络歌曲：VIP/登录态用 songId 重新解析真实播放地址（持久化临时链接会过期）；
     *         解析失败 emit networkPlayResolveFailed */
    Q_INVOKABLE void playNetworkSong(qint64 songId, const QString &fallbackUrl, bool isVip);
    /*! @brief 当前播放歌曲的网易云 songId / 是否 VIP（QML 各播放入口统一重新解析用）*/
    Q_INVOKABLE qint64 currentSongId() const { return m_currentSongId; }
    Q_INVOKABLE bool currentIsVip() const { return m_currentIsVip; }
    /*! @brief 设置播放模式（0=顺序播放 1=单曲循环 2=随机播放），单曲循环在歌曲结束时不销毁播放器直接 seek 回 0 重播*/
    Q_INVOKABLE void setPlayMode(int mode);
    /*! @brief "我整合的音乐"列表持久化：整体保存/加载到 config.json 的 integratedMusic.list（顺序=传入顺序）*/
    Q_INVOKABLE void saveIntegratedMusic(const QVariantList &list);
    Q_INVOKABLE QVariantList loadIntegratedMusic();
    /*! @brief QML前端调用，VideoPlayerStack点击播放事件*/
    Q_INVOKABLE void requestPlayVideo(QString file_path);
    /*! @brief 准备播放指定媒体文件（内部播放逻辑核心）*/
    void preparePlayingMedia(QString file_path);
    /*! @brief QML前端调用，请求进度条seek操作*/
    Q_INVOKABLE int seekTo(int cur_value);
    /*! @brief QML前端调用，请求调整音量*/
    Q_INVOKABLE void setVolume(int cur_volume);
    /*! @brief QML前端调用，请求播放下一首*/
    Q_INVOKABLE void playNext()
    {
        emit this->playNextTriggered();
    }
    /*! @brief QML前端调用，请求播放上一首*/
    Q_INVOKABLE void playPrevious()
    {
        emit this->playPreviousTriggered();
    }

    //===== D3D11VideoRender --> Oran7VideoRender ==========
    /*! @brief 视频缩放模式：Fit保持宽高比居中，Fill拉伸填满*/
    enum class ScaleMode { Fit, Fill };
    Q_ENUM(ScaleMode)
    /*! @brief 渲染对象类型：VideoPlayerRender视频播放，ScreenCaptureRender屏幕录制*/
    enum class RenderObject { VideoPlayerRender, ScreenCaptureRender };
    Q_ENUM(RenderObject)

    /*! @brief D3D11渲染槽位，存储渲染项、宿主、源尺寸和缩放模式*/
    typedef struct D3D11RenderSlot {
        QPointer<D3D11VideoItem> item;
        QPointer<QQuickItem> host;
        QMetaObject::Connection cW, cH;
        QSize srcSize;
        ScaleMode scaleMode = ScaleMode::Fit;
        bool syncPending = false;
    }D3D11RenderContext;

    inline uint qHash(RenderObject key, uint seed = 0) noexcept {
        return ::qHash(static_cast<int>(key), seed);
    }
    /*! @brief D3D11渲染槽位映射表*/
    QHash<RenderObject, D3D11RenderSlot> m_d3d11Slots;

    /*! @brief 创建D3D11VideoItem并绑定到指定渲染宿主*/
    void createVideoItem(const RenderObject key,QQuickItem* host);
    /*! @brief QML前端调用，将D3D11VideoItem附加到指定渲染宿主*/
    Q_INVOKABLE bool attachVideoItem(const RenderObject key,QQuickItem* host);
    /*! @brief 设置源视频尺寸并调度异步缩放同步*/
    void setVideoSourceSize(const RenderObject &key,const QSize &s) { m_d3d11Slots[key].srcSize = s; scheduleSyncVideoItemSize(key); }
    /*! @brief QML前端调用，设置视频缩放模式（Fit/Fill）*/
    Q_INVOKABLE void setScaleMode(const RenderObject &key,const ScaleMode m) { m_d3d11Slots[key].scaleMode = m; syncVideoItemSize(key); }
    /*! @brief D3D11设备临时存储，用于FFmpeg硬件解码*/
    ComPtr<ID3D11Device> m_qtDevice = nullptr;
    /*! @brief 设置FFPlayer（FFmpeg）使用的硬件解码D3D11设备*/
    void setD3D11Device(ID3D11Device *dev);
    /*! @brief QML前端调用，在指定渲染对象上渲染黑帧*/
    Q_INVOKABLE void renderBlackFrame(const RenderObject &key){m_d3d11Slots[key].item->renderBlackFrame();};
    /*! @brief QML前端调用，获取指定渲染对象的 D3D11VideoItem，用于 ShaderEffectSource 采集*/
    Q_INVOKABLE QQuickItem* getVideoItem(RenderObject key) const {
        auto it = m_d3d11Slots.constFind(key);
        return (it != m_d3d11Slots.constEnd()) ? it->item.data() : nullptr;
    }

    //===== Oran7ScreenCapture =====//
    /*! @brief 屏幕录制控制器实例*/
    QPointer<Oran7ScreenCaptureController> m_screenCap;
    /*! @brief QML前端调用，获取屏幕录制控制器对象*/
    Q_INVOKABLE QObject* screenCapture() const{return m_screenCap;}

public:
    //=============================<Get Or Save Client QThread shared_data Function>======================//
    /*! @brief 获取本地音乐自定义播放顺序列表的引用（线程共享数据）*/
    QVariantList& getCustom_LocalMusic_playOrder();
    /*! @brief 获取本地音乐文件路径集合的引用（线程共享数据）*/
    QSet<QString>& getLocalMusic_fileSet();
    /*! @brief 获取本地音乐文件信息列表的引用（线程共享数据）*/
    QList<QFileInfo>& getLocalMusic_fileList();

    /*! @brief 获取Client共享数据线程同步锁的引用*/
    QMutex& getClientMutex() { return client_mutex; }
signals:
    //====================<Client Server -->signals>===================//
    /*! @brief 最终登录结果通知*/
    void loginSuccess(QString username);
    /*! @brief 登录输入格式验证结果响应信号*/
    void loginInputFormatValid(int errorFormatCode);
    /*! @brief 往QML端"我整合的音乐"musicListModel推送一条元数据*/
    void integratedMusicElementReady(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id);
    /*! @brief 往QML端LocalMusic的musicListModel推送一条元数据*/
    void localMusicElementReady(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id,QString file_path);
    /*! @brief 清空QML前端LocalMusicStack列表*/
    void localMusicListCleared();
    /*! @brief 网络歌播放地址解析失败（未登录/无会员/链接失效），QML 据此自动切下一首或暂停*/
    void networkPlayResolveFailed(qint64 songId);

    //=====================<ConfigLoad Or Get Signal from Qml>========================//
    /*! @brief 加载上次关闭App时最后播放的文件信息，恢复播放状态*/
    void focusedMusicRestored(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id,QString file_path);
    /*! @brief 加载App保存的音量配置，同步到QML端VolumeSlider*/
    void playerVolumeConfigLoaded(int value);

    //=====================<Oran7MediaPlayer -->signals>=================== //
    /*! @brief 播放/暂停请求信号，由QML控件触发，Client内部建立连接*/
    void playOrPauseRequested();
    /*! @brief 停止播放请求信号，由QML控件触发或播放结束时自动触发*/
    Q_INVOKABLE void stopRequested();
    /*! @brief 媒体文件播放暂停时自动触发，更新QML端暂停/停止图标状态*/
    void stopIconUpdated();
    /*! @brief 更新QML端播放进度条的当前位置和当前时间（浮点秒）*/
    void playProgressUpdated(int CurPos,double CurTime);
    /*! @brief 更新QML端ProgressSlider旁边的当前播放文件总时长*/
    void totalDurationUpdated(int AllTime);
    /*! @brief 匹配到LRC歌词文件时发射，通知QML加载歌词*/
    void lyricsAvailable(const QString &lrcFilePath);
    /*! @brief 未匹配到LRC歌词文件时发射，通知QML清空歌词*/
    void lyricsUnavailable();
    /*! @brief 在线歌词搜索开始时发射，通知QML显示"正在拉取歌词"*/
    void onlineLyricsSearching();
    /*! @brief 在线歌词搜索失败时发射，通知QML显示"未找到歌词"*/
    void onlineLyricsNotFound();
    /*! @brief 触发播放下一首，由QML端或播放结束自动触发*/
    void playNextTriggered();
    /*! @brief 触发播放上一首，由QML端触发*/
    void playPreviousTriggered();
    /*! @brief 触发前端LocalMusicStack的musicList自定义重新排序操作*/
    void localMusicListReordered(const QVariantList reOrderd_list);

    /*! @brief 发送视频渲染详细信息到QML端VideoRenderItem*/
    void videoRenderInfoUpdated(const QVariantMap &info);

private slots:
    //====================<Client Server -->private slots>===================//
    /*! @brief TCP套接字连接成功处理*/
    void onConnected();
    /*! @brief TCP套接字断开连接处理*/
    void onDisconnected();
    /*! @brief TCP套接字数据就绪读取处理*/
    void onReadReady();

private slots:
    //==========================<Oran7MediaPlayer -->private slots>=================== //
    /*! @brief 播放/暂停内部处理槽，创建或控制Oran7MediaPlayer实例*/
    void onPlayOrPause();
    /*! @brief 停止播放内部处理槽，销毁Oran7MediaPlayer实例并释放资源*/
    void onStop();
    /*! @brief 同步D3D11VideoItem的尺寸到宿主容器，保持宽高比或填充*/
    void syncVideoItemSize(const Client::RenderObject &key);
    /*! @brief 调度异步的D3D11VideoItem尺寸同步请求*/
    void scheduleSyncVideoItemSize(const Client::RenderObject &key);

private:
    //==========================<Client Server>===========================//
    /*! @brief 客户端TCP套接字*/
    QTcpSocket *Client_socket;
    /*! @brief 客户端响应服务器协议处理器容器，按操作码索引*/
    QMap<quint8,Packedhandler_> Client_handler;
    /*! @brief 从服务器读取的数据缓冲区*/
    QByteArray responseFromServer;
    /*! @brief 客户端缓存的用户UID*/
    quint32 UID;
    /*! @brief 本地SQLite数据库连接*/
    QSqlDatabase localdb;

    //========= Client of Connections ==============
    /*! @brief 存储的信号槽连接列表，用于析构时统一断开*/
    QList<QMetaObject::Connection> connections;

    //=============================<Oran7MediaPlayer>========================//
    //--------------------------<Parameters>-------------------//
    /*! @brief Oran7MediaPlayer实例指针，播放器核心对象*/
    Oran7MediaPlayer *mp_=nullptr;
    /*! @brief 保护 mp_ 的递归锁：播放器销毁被挪到后台线程（onStop），
     *  防止 100ms 进度定时器等主线程操作与后台销毁并发使用已释放的指针 */
    mutable QRecursiveMutex m_mpMutex;
    /*! @brief 播放进度条位置更新请求定时器*/
    QTimer *Progress_SliderPos_ReqUpdateTimer;
    /*! @brief 当前媒体总播放时间，单位ms*/
    long total_duration_ = 0;
    /*! @brief 进度条Seek请求标志位，防止seek期间更新进度*/
    bool req_seeking_ = false;
    /*! @brief 切歌防重入标志：300ms 延迟创建新播放器回调已在排队时置 true，
     *         后续切歌请求直接更新 CurMediaFilePath 即可，避免累积多个 singleShot 回调
     *         导致反复创建/暂停/播放、SDL 音频设备并发开闭。 */
    bool m_pendingSwitch = false;
    /*! @brief 记录当前播放位置，单位秒（浮点精度）*/
    double Current_SecPosition_ = 0.0;
    /*! @brief 退出时防止帧堆积的原子标志，直接释放渲染资源*/
    std::atomic_bool shutting_down_{false};

    //--------------------------<function>-------------------//
    /*! @brief 调用Native层请求内部FFPlayer返回媒体文件总时长（传入所属播放器，避免旧播放器消息线程误读全局 mp_）*/
    void getTotalDuration(Oran7MediaPlayer* p);
    /*! @brief 调用Native层请求内部FFPlayer返回媒体当前播放位置*/
    void reqUpdateCurrentPosition();

    /*! @brief 尝试为当前播放的媒体文件匹配同名LRC歌词文件*/
    void tryLoadLrcForFile(const QString &mediaFilePath);
    /*! @brief 本地无歌词时，通过在线API搜索并下载LRC歌词*/
    void tryOnlineLyricsSearch(const QString &mediaFilePath);
    /*! @brief 在线歌词获取器（LRCLIB优先 + 网易云fallback）*/
    OnlineLyricsFetcher *m_lyricsFetcher = nullptr;

    /*! @brief 当前网络歌曲的歌词缓存文件路径（播放结束/停止时销毁，不影响本地歌词）*/
    QString m_networkLyricsCachePath;
    void cleanupNetworkLyricsCache();
    static bool isNetworkUrl(const QString &path);
    /*! @brief 当前播放歌曲的网易云 songId（播放时由 QML 注入，会员歌拉歌词用）*/
    qint64 m_currentSongId = 0;
    /*! @brief 当前播放歌曲是否为会员（VIP）歌曲（最近播放记录用）*/
    bool m_currentIsVip = false;
    /*! @brief 播放模式：0=顺序播放 1=单曲循环 2=随机播放（QML 切歌/结束重播时用）*/
    int m_playMode = 0;
    /*! @brief 待解析播放的网络歌 songId（一次性，解析完成后清零）*/
    qint64 m_pendingNetworkSongId = 0;
    /*! @brief 非 VIP 歌已用外链立即起播（解析在后台并行仅更新缓存，完成后不打断播放）*/
    bool m_playStartedByFallback = false;
    /*! @brief 解析失败(-110 限流)是否已重试过一次（防无限重试）*/
    bool m_retriedOnce = false;
    /*! @brief 起播打开失败后是否已用已解析地址补救过一次（防无限重试，每首歌复位）*/
    bool m_openRetryDone = false;
    /*! @brief 最近解析成功的播放地址缓存：songId -> (url, 缓存时刻ms)
     *         网易返回的 URL 有效期 expi=1200s，20 分钟内切回同一首歌复用缓存，避免重复解析触发 -110 限流 */
    QHash<qint64, QPair<QString, qint64>> m_songUrlCache;
    /*! @brief 恢复上次播放时补全元数据用的网络管理器(专辑/时长缺失时按 songId 重新拉取)*/
    QNetworkAccessManager *m_metaNam = nullptr;
    void onNetworkUrlResolved(const QHash<qint64, QString>& urlMap);
    /*! @brief 取缓存中该 songId 的新鲜播放地址（超过 20min 视为失效并顺手清理）；没有则返回空串 */
    QString freshCachedUrl(qint64 songId);
    /*! @brief 起播打开失败后的补救（主线程）：有新鲜的已解析地址就换它重试一次，否则交给自动切歌 */
    void onPlaybackOpenFailed();
    static qint64 extractSongId(const QString &s);

    //=============================<Client QThread shared_data >======================//
    /*! @brief Client共享数据线程同步互斥锁*/
    mutable QMutex client_mutex;
    // 异步任务生命周期守卫：析构时置 true，worker 线程检查后提前返回，避免 UAF
    std::shared_ptr<std::atomic<bool>> m_destroyedFlag;
    //-----------------------------------<Client Global Data>--------------------------------//
    /*! @brief Client全局应用数据结构体，存储播放状态、文件列表、配置等*/
    Oran7AppData appData=Oran7AppData();

    //==========================<Client Config tools>=====================>
    /*! @brief 从ffprobe生成的JSON中提取媒体信息*/
    QMap<QString, QVariant> extractMediaInfo(const QJsonObject& root);
    /*! @brief 应对JSON标签大小写不同，灵活提取指定标签的音乐信息*/
    QString extractMusicInfoTag(const QJsonObject& root, const QString& tagName, const QString& defaultValue = "");
    /*! @brief 递归遍历JSON嵌套结构的工具函数*/
    QVariant getNestedValue(const QStringList& keys, const QJsonObject& obj);
};


static inline qreal snapToPixel(qreal v, qreal dpr)
{
    return std::round(v * dpr) / dpr;
}

static QRectF calcAspectRectSnapped(const QSizeF &src, const QSizeF &dst, bool fill, qreal dpr)
{
    if (src.isEmpty() || dst.isEmpty())
        return QRectF(QPointF(0,0), dst);

    if (fill) {
        qreal dstWidth = snapToPixel(dst.width(), dpr);
        qreal dstHeight = snapToPixel(dst.height(), dpr);
        return QRectF(QPointF(0, 0), QSizeF(dstWidth, dstHeight));
    }

    const qreal sx = dst.width()  / src.width();
    const qreal sy = dst.height() / src.height();
    const qreal s = std::min(sx, sy);

    QSizeF out(src.width() * s, src.height() * s);
    QPointF topLeft((dst.width() - out.width()) * 0.5,
                    (dst.height() - out.height()) * 0.5);

    topLeft.setX(snapToPixel(topLeft.x(), dpr));
    topLeft.setY(snapToPixel(topLeft.y(), dpr));
    out.setWidth (snapToPixel(out.width(),  dpr));
    out.setHeight(snapToPixel(out.height(), dpr));

    return QRectF(topLeft, out);
}

#endif // ORAN7MEDIACLIENT_H
