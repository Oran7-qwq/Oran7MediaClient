#include "FramelessWindow.h"
#include <QQuickWindow>
#include <QDebug>
#include <QGuiApplication>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windowsx.h>
#include <shellapi.h>
#endif

FramelessWindow::FramelessWindow(QObject *parent)
    : QObject(parent)
    , m_targetWindow(nullptr)
    , m_borderWidth(8)
    , m_borderHeight(8)
    , m_titleBarHeight(32)
    , m_titleBarItem(nullptr)
#ifdef Q_OS_WIN
    , m_hwnd(nullptr)
    , m_isCompositionEnabled(false)
#endif
{
    qApp->installNativeEventFilter(this);
}

FramelessWindow::~FramelessWindow()
{
    if (qApp) {
        qApp->removeNativeEventFilter(this);
    }
}

void FramelessWindow::setTargetWindow(QQuickWindow* window)
{
    if (m_targetWindow != window) {
        m_targetWindow = window;
        emit targetWindowChanged();

#ifdef Q_OS_WIN
        if (window) {
            m_hwnd = reinterpret_cast<HWND>(window->winId());
            setupWindow();
        }
#endif
    }
}

void FramelessWindow::setupWindow()
{
#ifdef Q_OS_WIN
    if (!m_hwnd) return;

    //DWM
    enableComposition();

    setupWindowRoundedCorners();//only Windows 11

    extendFrameIntoClientArea();

    setupWindowStyle();

    qDebug() << "FramelessWindow setup complete with rounded corners for window:" << m_hwnd;
#endif
}

void FramelessWindow::hitTest(int x, int y, int &result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(x);
    Q_UNUSED(y);
    Q_UNUSED(result);
#endif
}

void FramelessWindow::nativeMaximize()
{
#ifdef Q_OS_WIN
    if (!m_hwnd) return;

    if (IsZoomed(m_hwnd)) {
        ShowWindow(m_hwnd, SW_RESTORE);

        ensureCorrectWindowStyle();
    } else {
        ShowWindow(m_hwnd, SW_MAXIMIZE);
    }
#endif
}

void FramelessWindow::nativeMinimize()
{
#ifdef Q_OS_WIN
    if (!m_hwnd) return;

    ShowWindow(m_hwnd, SW_MINIMIZE);
#endif
}

void FramelessWindow::ensureCorrectWindowStyle()
{
#ifdef Q_OS_WIN
    if (!m_hwnd) return;

    LONG_PTR style = GetWindowLongPtr(m_hwnd, GWL_STYLE);

    if (!(style & WS_THICKFRAME)) {
        style |= WS_THICKFRAME | WS_POPUP;
        SetWindowLongPtr(m_hwnd, GWL_STYLE, style);
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
        qDebug() << "FramelessWindow: Restored WS_THICKFRAME to prevent mouse through";
    }
#endif
}

#ifdef Q_OS_WIN

void FramelessWindow::enableComposition()
{
    HRESULT hr = S_OK;
    BOOL compositionEnabled = FALSE;

    hr = DwmIsCompositionEnabled(&compositionEnabled);
    if (SUCCEEDED(hr)) {
        m_isCompositionEnabled = (compositionEnabled == TRUE);
        qDebug() << "DWM Composition enabled:" << m_isCompositionEnabled;
    }
}

void FramelessWindow::setupWindowRoundedCorners()
{
    if (!m_hwnd) return;

    // Windows 11 Radius
    // DWMWA_WINDOW_CORNER_PREFERENCE = 33
    typedef enum {
        DWMWCP_DEFAULT = 0,
        DWMWCP_DONOTROUND = 1,
        DWMWCP_ROUND = 2,
        DWMWCP_ROUNDSMALL = 3
    } DWM_WINDOW_CORNER_PREFERENCE;

    // Load DwmSetWindowAttribute function
    HMODULE hDwmApi = LoadLibraryW(L"dwmapi.dll");
    if (hDwmApi) {
        typedef HRESULT (WINAPI *P_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        P_DwmSetWindowAttribute pDwmSetWindowAttribute =
            (P_DwmSetWindowAttribute)GetProcAddress(hDwmApi, "DwmSetWindowAttribute");

        if (pDwmSetWindowAttribute) {
            // Windows Default Radius
            DWORD cornerPreference = DWMWCP_DEFAULT;
            HRESULT hr = pDwmSetWindowAttribute(m_hwnd, 33, &cornerPreference, sizeof(DWORD));

            if (SUCCEEDED(hr)) {
                qDebug() << "Successfully set window rounded corners to default";
            } else {
                qDebug() << "Failed to set window rounded corners, error:" << hr;
                // Force start radius
                cornerPreference = DWMWCP_ROUND;
                hr = pDwmSetWindowAttribute(m_hwnd, 33, &cornerPreference, sizeof(DWORD));
                if (SUCCEEDED(hr)) {
                    qDebug() << "Successfully forced window rounded corners";
                }
            }
        }

        FreeLibrary(hDwmApi);
    }
}

void FramelessWindow::extendFrameIntoClientArea()
{
    if (!m_hwnd || !m_isCompositionEnabled) return;

    // 扩展边框到整个客户端区域以支持圆角和透明效果
    // 使用负边距来确保 DWM 效果覆盖整个窗口
    MARGINS margins = {-1, -1, -1, -1};
    HRESULT hr = DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    if (SUCCEEDED(hr)) {
        qDebug() << "Extended frame into client area successfully with negative margins for rounded corners";

        // 启用 DWM 模糊背景以支持更好的圆角效果
        DWM_BLURBEHIND bb = {0};
        bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
        bb.fEnable = TRUE;
        bb.hRgnBlur = NULL;
        DwmEnableBlurBehindWindow(m_hwnd, &bb);

        // 启用 DWM 延伸框架到客户端区域
        DWMNCRENDERINGPOLICY ncpolicy = DWMNCRP_ENABLED;
        DwmSetWindowAttribute(m_hwnd, DWMWA_NCRENDERING_POLICY, &ncpolicy, sizeof(DWMNCRENDERINGPOLICY));
    } else {
        qWarning() << "Failed to extend frame into client area:" << hr;
    }
}

void FramelessWindow::setupWindowStyle()
{
    if (!m_hwnd) return;

    LONG_PTR style = GetWindowLongPtr(m_hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtr(m_hwnd, GWL_EXSTYLE);

    style &= ~WS_CAPTION;
    style |= WS_THICKFRAME;

    style |= WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

    exStyle |= WS_EX_LAYERED;
    exStyle |= WS_EX_WINDOWEDGE;

    // Windows 11
    typedef enum {
        DWMWCP_DEFAULT = 0,
        DWMWCP_DONOTROUND = 1,
        DWMWCP_ROUND = 2,
        DWMWCP_ROUNDSMALL = 3
    } DWM_WINDOW_CORNER_PREFERENCE;

    HMODULE hDwmApi = LoadLibraryW(L"dwmapi.dll");
    if (hDwmApi) {
        typedef HRESULT (WINAPI *P_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
        P_DwmSetWindowAttribute pDwmSetWindowAttribute =
            (P_DwmSetWindowAttribute)GetProcAddress(hDwmApi, "DwmSetWindowAttribute");

        if (pDwmSetWindowAttribute) {
            DWORD cornerPreference = DWMWCP_ROUND;
            pDwmSetWindowAttribute(m_hwnd, 33, &cornerPreference, sizeof(DWORD));
        }
        FreeLibrary(hDwmApi);
    }

    SetWindowLongPtr(m_hwnd, GWL_STYLE, style);
    SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);

    qDebug() << "Window style setup complete with rounded corners and shadow support";
}

bool FramelessWindow::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType != "windows_generic_MSG") {
        return false;
    }

    MSG* msg = static_cast<MSG*>(message);
    HWND hwnd = msg->hwnd;

    if (m_hwnd && hwnd != m_hwnd) {
        return false;
    }

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (msg->wParam == TRUE) {
            NCCALCSIZE_PARAMS* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);

            Q_UNUSED(params)

            *result = 0;
            return true;
        }
        break;
    }

    case WM_NCHITTEST: {
        *result = 0;

        RECT rect;
        GetWindowRect(hwnd, &rect);

        POINT pos = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };

        if (pos.x < rect.left || pos.x > rect.right ||
            pos.y < rect.top || pos.y > rect.bottom) {
            return false;
        }

        int x = pos.x - rect.left;
        int y = pos.y - rect.top;

        int width = rect.right - rect.left;
        int height = rect.bottom - rect.top;

        bool onSystemButton = false;
        for (auto it = m_systemButtons.constBegin(); it != m_systemButtons.constEnd(); ++it) {
            QQuickItem* buttonItem = it.value();
            if (buttonItem) {
                QPointF scenePos = buttonItem->mapToScene(QPointF(0, 0));
                QRectF windowRect(0, 0, width, height);
                QRectF buttonRect = buttonItem->boundingRect().translated(scenePos - QPointF(windowRect.x(), windowRect.y()));

                if (x >= buttonRect.left() && x <= buttonRect.right() &&
                    y >= buttonRect.top() && y <= buttonRect.bottom()) {
                    switch (it.key()) {
                    case Close:
                        *result = HTCLOSE;
                        return true;
                    case Maximize:
                        *result = HTMAXBUTTON;
                        return true;
                    case Minimize:
                        *result = HTMINBUTTON;
                        return true;
                    default:
                        break;
                    }
                }
            }
        }

        // 控制按钮区域（右上角） - 现在只影响拖动，不影响边框拉伸
        int controlButtonWidth = 140;
        int controlButtonHeight = 36;
        int margin = 12;

        bool onControlButtons = (x >= width - controlButtonWidth - margin && y <= controlButtonHeight + margin);

        // oran7IconAreaItem 区域（左上角）- 排除以防止被拖拽
        int iconAreaX = 12;
        int iconAreaY = 18;
        int iconAreaSize = 40;
        bool onIconArea = (x >= iconAreaX && x <= iconAreaX + iconAreaSize &&
                          y >= iconAreaY && y <= iconAreaY + iconAreaSize);

        // openSemiCircle 区域（左侧半圆按钮）- 排除以防止被拖拽
        // 根据QML：anchors.left: leftRectangle.right + 4, height: 0.8 * titleBarHeight, width: height/2
        int semiCircleHeight = int(0.8 * m_titleBarHeight);
        //int semiCircleWidth = semiCircleHeight / 2;
        int semiCircleY = int(0.1 * m_titleBarHeight);

        // openSemiCircle的位置会根据leftRectangle的宽度变化
        // 当leftRectangle展开时（width=204），openSemiCircle在 x=208+4=212
        // 当leftRectangle收起时（width=0），openSemiCircle在 x=0+4=4
        bool onSemiCircle = (x >= 4 && x <= 240 &&  // 覆盖从4到240的范围
                           y >= semiCircleY && y <= semiCircleY + semiCircleHeight);

        // 如果在这些交互区域内，让系统处理点击，但边框拉伸仍然有效
        if (onControlButtons || onIconArea || onSemiCircle) {
            // 不返回 false，继续下面的边框检测逻辑
        }

        // 检查是否在系统按钮区域
        for (auto it = m_systemButtons.constBegin(); it != m_systemButtons.constEnd(); ++it) {
            QQuickItem* buttonItem = it.value();
            if (buttonItem) {
                // 获取按钮的边界矩形
                QRectF itemLocalRect = buttonItem->boundingRect();

                // 将按钮的四个角映射到场景坐标
                QPointF topLeft = buttonItem->mapToScene(itemLocalRect.topLeft());
                QPointF topRight = buttonItem->mapToScene(itemLocalRect.topRight());
                QPointF bottomLeft = buttonItem->mapToScene(itemLocalRect.bottomLeft());
                QPointF bottomRight = buttonItem->mapToScene(itemLocalRect.bottomRight());

                // 检查鼠标位置是否在按钮区域内
                if (x >= topLeft.x() && x <= bottomRight.x() &&
                    y >= topLeft.y() && y <= bottomRight.y()) {
                    onSystemButton = true;
                    break;
                }
            }
        }

        // 如果在系统按钮区域，让系统处理点击（返回 HTCLIENT）
        if (onSystemButton) {
            *result = HTCLIENT;
            return true;
        }

        // 调整边框检测宽度
        int adjustedBorderWidth = m_borderWidth + 4;
        int adjustedBorderHeight = m_borderHeight + 4;

        // 检查是否在边框区域（用于拉伸）
        bool onLeft = (x <= adjustedBorderWidth);
        bool onRight = (x >= width - adjustedBorderWidth);  // 不再排除控制按钮区域
        bool onTop = (y <= adjustedBorderHeight);       // 不再排除控制按钮区域
        bool onBottom = (y >= height - adjustedBorderHeight);

        // 检查是否在标题栏区域（用于拖动）- 排除控制按钮、图标区域和半圆按钮区域
        bool onTitleBar = (y <= m_titleBarHeight && !onLeft && !onRight &&
                          !onControlButtons && !onIconArea && !onSemiCircle);

        if (onLeft && onTop) {
            *result = HTTOPLEFT;
        } else if (onRight && onTop) {
            *result = HTTOPRIGHT;
        } else if (onLeft && onBottom) {
            *result = HTBOTTOMLEFT;
        } else if (onRight && onBottom) {
            *result = HTBOTTOMRIGHT;
        } else if (onLeft) {
            *result = HTLEFT;
        } else if (onRight) {
            *result = HTRIGHT;
        } else if (onTop) {
            *result = HTTOP;
        } else if (onBottom) {
            *result = HTBOTTOM;
        } else if (onTitleBar) {
            *result = HTCAPTION;
        } else {
            *result = HTCLIENT;
        }

        return true;
    }

    case WM_NCACTIVATE: {
        // 处理窗口激活，保持自定义绘制
        if (!IsIconic(hwnd)) {
            *result = 1;
            return true;
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        // 处理最小/最大尺寸信息
        if (m_targetWindow) {
            MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
            mmi->ptMinTrackSize.x = m_targetWindow->minimumWidth();
            mmi->ptMinTrackSize.y = m_targetWindow->minimumHeight();
            *result = 0;
            return true;
        }
        break;
    }

    case WM_DPICHANGED: {
        // 处理 DPI 变化
        RECT* rect = reinterpret_cast<RECT*>(msg->lParam);
        SetWindowPos(hwnd, nullptr, rect->left, rect->top,
                     rect->right - rect->left, rect->bottom - rect->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        *result = 0;
        return true;
    }

    case WM_SIZE: {
        // 处理窗口大小变化
        if (msg->wParam == SIZE_RESTORED || msg->wParam == SIZE_MAXIMIZED || msg->wParam == SIZE_MINIMIZED) {
            // 在窗口状态改变后，确保窗口样式正确，防止鼠标穿透
            ensureCorrectWindowStyle();
        }
        break;
    }

    case WM_NCLBUTTONDBLCLK: {
        // 处理非客户区双击，支持双击标题栏最大化/还原
        if (msg->wParam == HTCAPTION) {
            // 让系统默认处理双击最大化
            // 不返回 true，让系统继续处理
            return false;
        }
        break;
    }

    case WM_NCRBUTTONDOWN: {
        // 处理非客户区右键点击，显示系统菜单
        // 暂时禁用此功能以避免崩溃问题
        // if (msg->wParam == HTCAPTION || msg->wParam == HTMINBUTTON ||
        //     msg->wParam == HTMAXBUTTON || msg->wParam == HTCLOSE) {
        //
        //     // 获取鼠标位置（屏幕坐标）
        //     POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
        //
        //     // 显示系统菜单
        //     HMENU hMenu = GetSystemMenu(hwnd, FALSE);
        //     if (hMenu) {
        //         // 根据窗口状态启用/禁用菜单项
        //         EnableMenuItem(hMenu, SC_RESTORE, IsZoomed(hwnd) ? MF_ENABLED : MF_GRAYED);
        //         EnableMenuItem(hMenu, SC_MOVE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
        //         EnableMenuItem(hMenu, SC_SIZE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
        //         EnableMenuItem(hMenu, SC_MINIMIZE, MF_ENABLED);
        //         EnableMenuItem(hMenu, SC_MAXIMIZE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
        //
        //         // 设置前台窗口，确保菜单能正确显示
        //         SetForegroundWindow(hwnd);
        //
        //         // 显示菜单 - 使用同步方式
        //         DWORD cmd = TrackPopupMenu(hMenu,
        //                                    TPM_LEFTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        //                                    pt.x, pt.y, 0, hwnd, nullptr);
        //
        //         // 如果用户选择了菜单项，发送相应的系统命令
        //         if (cmd != 0) {
        //             // 直接发送命令，不使用 PostMessage
        //             SendMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
        //         }
        //     }
        //
        //     *result = 0;
        //     return true;
        // }
        return false; // 让系统默认处理
    }
    }

    return false;
}

void FramelessWindow::setSystemButton(int buttonType, QQuickItem* item)
{
    if (buttonType < Minimize || buttonType > Close) {
        qWarning() << "Invalid system button type:" << buttonType;
        return;
    }

    // 移除旧按钮
    if (m_systemButtons.contains(buttonType)) {
        QQuickItem* oldItem = m_systemButtons[buttonType];
        if (oldItem) {
            setHitTestVisible(oldItem, false);
        }
        m_systemButtons.remove(buttonType);
    }

    // 设置新按钮
    if (item) {
        m_systemButtons[buttonType] = item;
        setHitTestVisible(item, true);
        qDebug() << "Set system button" << buttonType << "to item" << item;
    }
}

void FramelessWindow::setTitleBar(QQuickItem* item)
{
    if (m_titleBarItem == item) {
        return;
    }

    // 移除旧标题栏
    if (m_titleBarItem) {
        setHitTestVisible(m_titleBarItem, false);
        m_titleBarItem = nullptr;
    }

    // 设置新标题栏
    if (item) {
        m_titleBarItem = item;
        setHitTestVisible(item, true);
        qDebug() << "Set title bar to item" << item;
    }
}

void FramelessWindow::setHitTestVisible(QQuickItem* item, bool visible)
{
    if (!item) {
        return;
    }

    if (visible) {
        m_hitTestItems[item] = true;
    } else {
        m_hitTestItems.remove(item);
    }

    // 在 Windows 上，我们需要通过窗口消息来处理 hit test
    // 这里只是标记，实际的 hit test 处理在 nativeEventFilter 中
    Q_UNUSED(visible);
}

#endif // Q_OS_WIN
