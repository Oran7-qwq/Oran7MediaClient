#ifndef ORAN7CONSOLELOGGER_H
#define ORAN7CONSOLELOGGER_H

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <QDebug>
#include <QMutex>

class Oran7ConsoleLogger
{
public:
    static bool attachConsole(const QString &title = "Oran7MediaClient Logs")
    {
        static QMutex mutex;
        QMutexLocker locker(&mutex);

        if (s_consoleAttached) {
            return true;
        }

        bool consoleCreated = false;

        // 先尝试附加到父进程的控制台（从 cmd 启动时）
        if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
            DWORD error = GetLastError();

            if (error == ERROR_ACCESS_DENIED) {
                // 已经有控制台，不需要 AllocConsole
            } else {
                if (!AllocConsole()) {
                    error = GetLastError();
                    Q_UNUSED(error);
                    return false;
                }
                consoleCreated = true;
            }
        }

        // 设置控制台标题
        if (!title.isEmpty()) {
            SetConsoleTitleW((LPCWSTR)title.utf16());
        }

        // 重定向标准输出
        if (freopen("CONOUT$", "w", stdout) == nullptr) {
            return false;
        }
        setvbuf(stdout, NULL, _IONBF, 0);

        // 重定向标准错误
        if (freopen("CONOUT$", "w", stderr) == nullptr) {
            return false;
        }
        setvbuf(stderr, NULL, _IONBF, 0);

        // 重定向标准输入
        freopen("CONIN$", "r", stdin);

        // 设置控制台输出代码页为 UTF-8
        SetConsoleOutputCP(CP_UTF8);

        // 启用虚拟终端处理（支持 ANSI/VT 颜色序列）
        s_vtEnabled = enableVirtualTerminal();

        s_consoleCreatedByUs = consoleCreated;
        s_consoleAttached = true;

        // 消息处理器由 Oran7LogSink 统一管理，控制台只负责附加与编码。

        // 测试输出
        qDebug() << "\n========== Console Oran7Logger Attached ==========";
        qDebug() << "Title:" << title;
        qDebug() << "Console created:" << (consoleCreated ? "Yes" : "No (attached to parent)");
        qDebug() << "VT Enabled:" << (s_vtEnabled ? "Yes" : "No");
        qDebug() << "==============================================\n";

        return true;
    }

    static void detachConsole()
    {
        if (s_consoleAttached) {
            if (s_consoleCreatedByUs) {
                FreeConsole();
            }
            s_consoleAttached = false;
        }
    }

    static bool isAttached()
    {
        return s_consoleAttached;
    }

    static bool isVTEnabled()
    {
        return s_vtEnabled;
    }

private:
    // 启用虚拟终端处理
    static bool enableVirtualTerminal()
    {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
            return false;
        }

        DWORD mode = 0;
        if (!GetConsoleMode(hOut, &mode)) {
            return false;
        }

        mode |= ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;

        return SetConsoleMode(hOut, mode) != 0;
    }

    static bool s_consoleAttached;
    static bool s_consoleCreatedByUs;
    static bool s_vtEnabled;
};

inline bool Oran7ConsoleLogger::s_consoleAttached = false;
inline bool Oran7ConsoleLogger::s_consoleCreatedByUs = false;
inline bool Oran7ConsoleLogger::s_vtEnabled = false;

#define ATTACH_CONSOLE(title) Oran7ConsoleLogger::attachConsole(title)
#define DETACH_CONSOLE() Oran7ConsoleLogger::detachConsole()

#else // 非 Windows 平台

class Oran7ConsoleLogger
{
public:
    static bool attachConsole(const QString &title = "")
    {
        Q_UNUSED(title)
        return false;
    }
    static void detachConsole() {}
    static bool isAttached() { return false; }
};

#define ATTACH_CONSOLE(title) (void)(title)
#define DETACH_CONSOLE() (void)0

#endif

#endif // ORAN7CONSOLELOGGER_H
