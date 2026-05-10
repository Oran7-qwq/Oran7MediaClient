#ifndef CLIENT_H
#define CLIENT_H

#include"Oran7MediaPlayer.h"
#include "D3d11Videoitem.h"
#include "GlobalHelper.h"
#include "Oran7ScreenCapture.h"

#include <QTimer>
#include <QObject>
#include <QTcpSocket>
#include <QMap>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QQmlEngine>
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

//客户端处理器函数包
using Packedhandler_ = std::function<void(quint32,quint8,const QByteArray &)>;

class Client : public QObject
{
    Q_OBJECT
    // Q_PROPERTY(VideoOpenGL name READ name WRITE setName NOTIFY nameChanged FINAL)
public:
    explicit Client(const QString &host,quint16 port,QObject *parent = nullptr);
    ~Client();
    //外部调用强制清理播放器内核
    void StopPlayerRuning()
    {
        shutting_down_ = true;//make sure video frame will be free
        this->OnStop();
    }
    //在构造函数内最后调用，初始化连接客户端各种预先信号处理槽函数
    int InitSignalsAndSlots();

    //-------------------------------------------->>>注册客户端响应服务器协议处理器方法<<---------------------------------//
    void registerHandler_(quint8 opcode,Packedhandler_ handler);

    //<<---------------<<登录>>---------------------------->>

    // 登录验证
    Q_INVOKABLE void validateLogin(QString username,QString password);

    //处理登录响应
    void loginResponse(quint32 totalLength,quint8 opcode,const QByteArray &packet);

    //<<---------------<<加载缓存云端数据>>--------------->>

    //加载云端用户数据询问
    void loadingSeverUserDataQuery();
    //解析服务器云端数据传输协议响应
    void explainSeverUserData(quint32 totalLength, quint8 opcode, const QByteArray &packet);

    //===============================<Client Public function>==========================//
    //创建AppData临时文件夹目录(若已有则会自动返回默认目录)
    QString createAppDirectories();

    //用于分析媒体文件信息 , 内部使用ffprobe和ffmepg.exe
    QMap<QString, QVariant> analyzeMediaFileInfo(QString filepath);

    //防止后端频繁响应前端请求信号工具
    QElapsedTimer m_callTimer;
    static const int CALL_INTERVAL_MS = 100; //100ms

    void refreshLocalMusicList();//用于触发Local MusicList前端列表刷新
    Q_INVOKABLE void addNewLocalMusic(const QVariantList &fileList);//供qml前端调用,请求添加本地音乐文件
    QList<QFileInfo> sortFileInfoList_byFilePaths(const QList<QFileInfo> &fileInfoList , const QVariantList& orderPaths);//排序audioFileTool

    //-----------------------------------------------------<Config-->Load-Save接口>-----------------------------------------------------------//
    //获取配置文件记录ui底部的近期最近一次关闭应用程序前的第一首music，并加载
    void loadConfig_lastCloseAppFocusedMusic();
    //加载提取config文件中保存的loaclMusicStack-->play_order，协商排列顺序
    void loadConfig_localMusicList_playOrder();
    //加载ApplicationWindow的width和height
    void loadConfig_AppWindowSize(QQmlApplicationEngine &engine);
    //加载ApplicationWindow的x和y
    void loadConfig_AppWindowPosition(QQmlApplicationEngine &engine);
    //加载App的保存的音量设置
    void loadConfig_AppSetPlayerVolume();

    //ui底部的近期最近一次关闭应用程序前的第一首music文件路径更新到配置文件
    void saveConfig_lastCloseAppFocusedMusic();
    //保存loaclMusicStack-->play_order到config文件中，用于下次打开Application时协商排序加载
    void saveConfig_localMusicList_playOrder();
    //保存ApplicationWindow的width和height
    void saveConfig_AppWindowSize(QQmlApplicationEngine &engine);
    //保存ApplicationWindow的x和y（Position）
    void saveConfig_AppWindowPosition(QQmlApplicationEngine &engine);
    //保存App的音量Config
    void saveConfig_AppSetPlayerVolume();

    //===============================<Control Oran7MediaPlayer接口>===============================//
    int message_loop(void *arg);//Oran7MediaPlayer与内层FFPlay跨线程通信接口工具
    int OutputVideo(const Frame *frame,AVFrame* copy_frame);//VideoFrameRender渲染接口 , 会在FFplayer内层Video_refresh_thread回调
    //供qml前端调用触发Client信号
    Q_INVOKABLE void triggerSigPlayOrPause(){emit SigPlayOrPause();}
    Q_INVOKABLE void triggerSigStop(){emit sigStop();}
    Q_INVOKABLE void qmlClickedReqPreparePlayMusic(QString file_path);//由qml前端调用，music列表点击播放事件
    Q_INVOKABLE void qmlClickedReqPreparePlayVideo(QString file_path);//由VideoPlayerStack调用
    void preparePlayingMedia(QString file_path);
    Q_INVOKABLE int progressSlider_Seek(int cur_valule);//供qml前端调用，请求seek操作
    Q_INVOKABLE void reqChangeVolumeValue(int cur_valume);//供qml前端调用，请求调整音量
    Q_INVOKABLE void reqPlayNext()//供qml前端调用，请求播放下一首
    {
        emit this->triggerPlayNext();
    }
    Q_INVOKABLE void reqPlayLast()//供qml前端调用，请求播放上一首
    {
        emit this->triggerPlayLast();
    }

    //===== D3D11VideoRender --> Oran7VideoRender ==========
    enum class ScaleMode { Fit, Fill };
    Q_ENUM(ScaleMode)
    enum class RenderObject { VideoPlayerRender, ScreenCaptureRender };
    Q_ENUM(RenderObject)

    typedef struct D3D11RenderSlot {
        QPointer<D3D11VideoItem> item;
        QPointer<QQuickItem> host;//qml端视频画面渲染父对象host
        QMetaObject::Connection cW, cH;
        QSize srcSize;                         // 源视频尺寸
        ScaleMode scaleMode = ScaleMode::Fit;
        bool syncPending = false;    //异步缩放请求
    }D3D11RenderContext;

    inline uint qHash(RenderObject key, uint seed = 0) noexcept {
        return ::qHash(static_cast<int>(key), seed);
    }
    QHash<RenderObject, D3D11RenderSlot> m_d3d11Slots;

    void createVideoItem(const RenderObject key,QQuickItem* host);
    Q_INVOKABLE bool attachVideoItem(const RenderObject key,QQuickItem* host);
    void setVideoSourceSize(const RenderObject &key,const QSize &s) { m_d3d11Slots[key].srcSize = s; scheduleSyncVideoItemSize(key); }
    Q_INVOKABLE void setScaleMode(const RenderObject &key,const ScaleMode m) { m_d3d11Slots[key].scaleMode = m; syncVideoItemSize(key); }
    ComPtr<ID3D11Device> m_qtDevice = nullptr; //D3D11设备临时存储
    void setD3D11Device(ID3D11Device *dev);//定义FFplayer(ffmpeg)使用的硬件解码D3D11设备-接口
    Q_INVOKABLE void renderBlackFrame(const RenderObject &key){m_d3d11Slots[key].item->renderBlackFrame();};

    //===== Oran7ScreenCapture =====//
    QPointer<Oran7ScreenCaptureController> m_screenCap;
    Q_INVOKABLE QObject* screenCapture() const{return m_screenCap;}

public:
    //=============================<Get Or Save Client QThread shared_data Function>======================//
    QVariantList& getCustom_LocalMusic_playOrder();
    QSet<QString>& getLocalMusic_fileSet();
    QList<QFileInfo>& getLocalMusic_fileList();

    QMutex& getClientMutex() { return client_mutex; }
signals:
    //====================<Client Sever  -->signals>===================//
    //最终登录结果
    void loginSuccess(QString username);
    //登录结果响应信号，在qml中建立连接
    void loginInputFormatValid(int errorFormatCode);
    //往qml中favoritemusic中musicListModel来push元数据的信号响应，在qml中建立连接，每次传入元数据
    void pushMyFavoriteMusicLIstListModel_ElementData(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id);
    //往qml中favoritemusic中musicLIstListModel来push元数据的信号响应，在qml中建立连接，每次传入元数据
    void pushLocalMusicListListModel_ElementData(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id,QString file_path);
    //用于清空qml前端LocalMusicStack列表，在qml中BottomPage.qml中建立连接
    void flushClear_LocalMusicStackList();

    //=====================<ConfigLoad Or Get Singnal from Qml>========================//
    //加载上一次关闭app的最后一次播放文件，在qml中建立连接
    void configSignal_loadLastCloseAppFocusedMusic(QString icon_path,QString music_name,QString music_artist,QString music_album,QString timesize,QString music_id,QString file_path);
    // //加载AppWindow的Config保存的width和height，在qml的main.qml中建立连接
    // void configSignal_loadAppWindowSize(int width,int height);//Discard
    // //加载AppWindow的Config保存的x和y（Position），在qml的main.qml中建立连接
    // void configSignal_loadAppWindowPosition(int x,int y);//Discard

    //加载App的保存的音量Condfig，在qml中BottomPage->VolumeSlieder建立连接
    void configSignal_loadPlayerVolumeConfig(int value);

    void configSignal_reqSavePlayerVolumeConfig();

    //=====================<Oran7MediaPlayer  -->signals>=================== //
    //用于处理qml控件发出的暂停按键触发信号在Client建立连接
    void SigPlayOrPause();
    //用于处理qml控件发出的停止信号在Client建立连接
    Q_INVOKABLE void sigStop();
    //用于响应媒体文件播放暂停时，自动触发一次qml中暂停区域的onClicked ,在qml中建立连接
    void updataQmlTransforStopIcon();
    //用于传入前端qml播放的当前进度条的当前位置，在qml中建立连接
    void updataQmlPlayProgressSliderCurPos(int CurPos,int CurTime);
    //用于更新ProgressSlider旁边当前播放文件的总时长，在qml中建立连接
    void updataQmlPlayNowFileAllTime(int AllTime);
    //用于触发播放下一首 , 在qml中建立连接
    void triggerPlayNext();
    //用于触发播放上一首 , 在qml中建立连接
    void triggerPlayLast();
    //用于触发前端LocalMusicStack的musicList自定义排序操作,重新排序,在qml中触发，客户端建立连接
    void reOrder_localMusicList(const QVariantList reOrderd_list);
    //用于触发LocalMusicStack的musicList有元素加入时的淡入动画,在qml中建立连接
    void triggerAddNewMusic_OpcityAniamtion(int index);

    //用于发送给UI端渲染的VideoInfo中详细内容，在qml端的，VideoRenderItem中建立连接
    void updateQmlRenderedVideoInfo(const QVariantMap &info);

private slots:
    //====================<Client Sever -->private slots>===================//
    void onConnected();
    void onDisconnected();
    void onReadReady();

private slots:
    //==========================<Oran7MediaPlayer  -->private slots>=================== //
    void OnPlayOrPause();
    void OnStop();
    void syncVideoItemSize(const Client::RenderObject &key);
    void scheduleSyncVideoItemSize(const Client::RenderObject &key);

private:
    //==========================<Client Sever>===========================//
    //单例实例
    // static std::shared_ptr<Client> m_instance;<-----Discard
    QTcpSocket *Client_socket;//客户端套接字
    QMap<quint8,Packedhandler_> Client_handler;//客户端响应服务器协议处理器容器
    QByteArray responseFromSever;//从服务器读取的数据缓冲区
    quint32 UID;//<客户端缓存数据>
    QSqlDatabase localdb;//本地数据库

    //========= Client of Connections ==============
    QList<QMetaObject::Connection> connections;

    //=============================<Oran7MediaPlayer>========================//
    //--------------------------<Parameters>-------------------//
    Oran7MediaPlayer *mp_=nullptr;//私有处理Oran7MediaPlayer通信接口工具
    QTimer *Progress_SliderPos_ReqUpdateTimer;
    long total_duration_ = 0;           //当前媒体总播放时间，(单位ms)
    bool req_seeking_ = false;          //<progressSlider_SeekRequest标志位>//;
    long Current_SecPosition_ = 0; //记录当前播放位置，单位秒
    std::atomic_bool shutting_down_{false};//退出时防止帧堆积，直接释放标志

    //--------------------------<function>-------------------//
    void getTotalDuration();            //调用Native层请求内部ffplayer返回媒体文件总时长
    void reqUpdateCurrentPosition();/*调用Native层请求内部ffplayer返回媒体当前播放位置*/

    //=============================<Client QThread shared_data >======================//
    mutable QMutex client_mutex; //Client共享数据线程同步锁
    //-----------------------------------<Client Global Data>--------------------------------//
    Oran7AppData appData=Oran7AppData();

    //==========================<Client Config tools>=====================>
    QMap<QString, QVariant> extractMediaInfo(const QJsonObject& root);  //分析从ffprobe生成的媒体信息json文件
    QString extractMusicInfoTag(const QJsonObject& root, const QString& tagName, const QString& defaultValue = "");//应变提取的json的键值标签大小写不同
    QVariant getNestedValue(const QStringList& keys, const QJsonObject& obj);//用于遍历JSON文件信息递归工具
};


static inline qreal snapToPixel(qreal v, qreal dpr)
{
    return std::round(v * dpr) / dpr;
}

static QRectF calcAspectRectSnapped(const QSizeF &src, const QSizeF &dst, bool fill, qreal dpr)
{
    if (src.isEmpty() || dst.isEmpty())
        return QRectF(QPointF(0,0), dst);

    // Fill模式：直接拉伸填满目标区域，不保持宽高比
    if (fill) {
        qreal dstWidth = snapToPixel(dst.width(), dpr);
        qreal dstHeight = snapToPixel(dst.height(), dpr);
        return QRectF(QPointF(0, 0), QSizeF(dstWidth, dstHeight));
    }

    // Fit模式：保持宽高比，计算居中显示区域
    const qreal sx = dst.width()  / src.width();
    const qreal sy = dst.height() / src.height();
    const qreal s = std::min(sx, sy);

    QSizeF out(src.width() * s, src.height() * s);
    QPointF topLeft((dst.width() - out.width()) * 0.5,
                    (dst.height() - out.height()) * 0.5);

    //对齐到物理像素
    topLeft.setX(snapToPixel(topLeft.x(), dpr));
    topLeft.setY(snapToPixel(topLeft.y(), dpr));
    out.setWidth (snapToPixel(out.width(),  dpr));
    out.setHeight(snapToPixel(out.height(), dpr));

    return QRectF(topLeft, out);
}

#endif // CLIENT_H
