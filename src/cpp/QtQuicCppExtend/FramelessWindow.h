#ifndef FRAMELESSWINDOW_H
#define FRAMELESSWINDOW_H

/**
 * @note Discard temporary
 *
*/
#include <QObject>
#include <QQuickWindow>
#include <QAbstractNativeEventFilter>
#include <QPoint>
#include <QQuickItem>
#include <QHash>
#include <QQuickItem>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

// DWM 常量定义（如果系统没有定义）
// 不需要重新定义 DWMNCRENDERINGPOLICY，因为 dwmapi.h 中已经有了
#endif

class FramelessWindow : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_PROPERTY(QQuickWindow *targetWindow READ targetWindow WRITE setTargetWindow NOTIFY targetWindowChanged)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth NOTIFY borderWidthChanged)
    Q_PROPERTY(int borderHeight READ borderHeight WRITE setBorderHeight NOTIFY borderHeightChanged)
    Q_PROPERTY(int titleBarHeight READ titleBarHeight WRITE setTitleBarHeight NOTIFY titleBarHeightChanged)

public:
    enum SystemButton
    {
        Unknown = 0,
        WindowIcon = 1,
        Help = 2,
        Minimize = 3,
        Maximize = 4,
        Close = 5,
    };
    Q_ENUM(SystemButton)

public:
    explicit FramelessWindow(QObject *parent = nullptr);
    ~FramelessWindow();

    QQuickWindow *targetWindow() const { return m_targetWindow; }
    void setTargetWindow(QQuickWindow *window);

    int borderWidth() const { return m_borderWidth; }
    void setBorderWidth(int width)
    {
        m_borderWidth = width;
        emit borderWidthChanged();
    }

    int borderHeight() const { return m_borderHeight; }
    void setBorderHeight(int height)
    {
        m_borderHeight = height;
        emit borderHeightChanged();
    }

    int titleBarHeight() const { return m_titleBarHeight; }
    void setTitleBarHeight(int height)
    {
        m_titleBarHeight = height;
        emit titleBarHeightChanged();
    }

    Q_INVOKABLE void setupWindow();
    Q_INVOKABLE void hitTest(int x, int y, int &result);
    Q_INVOKABLE void nativeMaximize();
    Q_INVOKABLE void nativeMinimize(); // 原生最小化（保留动画效果）
    Q_INVOKABLE void setSystemButton(int buttonType, QQuickItem *item);
    Q_INVOKABLE void setTitleBar(QQuickItem *item);
    Q_INVOKABLE void setHitTestVisible(QQuickItem *item, bool visible = true);

#ifdef Q_OS_WIN
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
    void ensureCorrectWindowStyle();
#endif

signals:
    void targetWindowChanged();
    void borderWidthChanged();
    void borderHeightChanged();
    void titleBarHeightChanged();

private:
    QQuickWindow *m_targetWindow;
    int m_borderWidth;
    int m_borderHeight;
    int m_titleBarHeight;
    QQuickItem *m_titleBarItem;
    QHash<int, QQuickItem *> m_systemButtons;
    QHash<QQuickItem *, bool> m_hitTestItems;

#ifdef Q_OS_WIN
    HWND m_hwnd;
    bool m_isCompositionEnabled;

    void extendFrameIntoClientArea();
    void setupWindowStyle();
    void enableComposition();
    void setupWindowRoundedCorners();
#endif
};

#endif // FRAMELESSWINDOW_H
