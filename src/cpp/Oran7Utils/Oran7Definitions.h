#ifndef ORAN7DEFINITIONS_H
#define ORAN7DEFINITIONS_H

/*! 声明一般属性 */
#define ORAN7_PROPERTY(type, get, set) \
private:\
    Q_PROPERTY(type get READ get WRITE set NOTIFY get##Changed FINAL) \
    public: \
    type get() const { return m_##get; } \
    void set(const type &t) { if (t != m_##get) { m_##get = t; emit get##Changed(); } } \
    Q_SIGNAL void get##Changed(); \
    private: \
    type m_##get;

/*! 声明一般属性并初始化 */
#define ORAN7_PROPERTY_INIT(type, get, set, init_value) \
private:\
    Q_PROPERTY(type get READ get WRITE set NOTIFY get##Changed FINAL) \
    public: \
    type get() const { return m_##get; } \
    void set(const type &t) { if (t != m_##get) { m_##get = t; emit get##Changed(); } } \
    Q_SIGNAL void get##Changed(); \
    private: \
    type m_##get{init_value};


/*! 声明只读属性 */
#define ORAN7_PROPERTY_READONLY(type, get) \
private:\
    Q_PROPERTY(type get READ get NOTIFY get##Changed FINAL) \
    public: \
    type get() const { return m_##get; } \
    Q_SIGNAL void get##Changed(); \
    private: \
    type m_##get;

/*! 音效添加*/
#define SOUND_EFFECT(type,Url)\
public:\
    Q_INVOKABLE void type()  { playPcm(type##Pcm, type##Format); }\
    private:\
    QAudioFormat type##Format;\
    QByteArray type##Pcm = loadWav(Url,type##Format);

#endif // ORAN7DEFINITIONS_H
