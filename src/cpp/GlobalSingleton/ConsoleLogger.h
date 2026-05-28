#ifndef CONSOLELOGGER_H
#define CONSOLELOGGER_H

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <cstdio>
#include <QDebug>
#include <QMutex>

class ConsoleLogger
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

        // 安装自定义 Qt 消息处理器
        qInstallMessageHandler(customMessageHandler);

        // 测试输出
        qDebug() << "\n========== Console Logger Attached ==========";
        qDebug() << "Title:" << title;
        qDebug() << "Console created:" << (consoleCreated ? "Yes" : "No (attached to parent)");
        qDebug() << "VT Enabled:" << (s_vtEnabled ? "Yes" : "No");
        qDebug() << "==============================================\n";

        return true;
    }

    static void detachConsole()
    {
        if (s_consoleAttached) {
            qInstallMessageHandler(nullptr);
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

    // Windows 传统控制台颜色
    static WORD winColor(QtMsgType type)
    {
        switch (type) {
        case QtDebugMsg:
            return FOREGROUND_GREEN | FOREGROUND_INTENSITY; // 亮绿
        case QtInfoMsg:
            return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY; // 亮青
        case QtWarningMsg:
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY; // 亮黄
        case QtCriticalMsg:
            return FOREGROUND_RED | FOREGROUND_INTENSITY; // 亮红
        case QtFatalMsg:
            return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE |
                   FOREGROUND_INTENSITY | BACKGROUND_RED; // 红底亮白
        }

        return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    }

    // ANSI/VT 颜色
    static const char *ansiColor(QtMsgType type)
    {
        switch (type) {
        case QtDebugMsg:
            return "\033[92m";       // bright cyan
        case QtInfoMsg:
            return "\033[96m";       // bright green
        case QtWarningMsg:
            return "\033[93m";       // bright yellow
        case QtCriticalMsg:
            return "\033[91m";       // bright red
        case QtFatalMsg:
            return "\033[1;97;41m";  // bright white on red
        }

        return "\033[37m";
    }

    // 日志前缀
    static const char *logPrefix(QtMsgType type)
    {
        switch (type) {
        case QtDebugMsg:
            return "";
        case QtInfoMsg:
            return "";
        case QtWarningMsg:
            return "";
        case QtCriticalMsg:
            return "";
        case QtFatalMsg:
            return "";
        }

        return "";
    }

    // 自定义 Qt 消息处理器
    static void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
    {
        Q_UNUSED(context)

        static QMutex logMutex;
        QMutexLocker locker(&logMutex);

        const char *prefix = logPrefix(type);

        if (s_vtEnabled) {
            // 使用 ANSI/VT 颜色序列
            fprintf(stdout, "%s%s%s\033[0m\n", ansiColor(type), prefix, msg.toUtf8().constData());
            fflush(stdout);
        }
        else
        {
            // 退回到传统 Windows 控制台颜色 API
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hConsole != INVALID_HANDLE_VALUE && hConsole != nullptr) {
                CONSOLE_SCREEN_BUFFER_INFO oldInfo {};
                const BOOL hasOldInfo = GetConsoleScreenBufferInfo(hConsole, &oldInfo);

                SetConsoleTextAttribute(hConsole, winColor(type));

                // 使用 printf 输出到 stdout（已被 freopen 重定向）
                printf("%s%s\n", prefix, msg.toUtf8().constData());

                // 恢复原始颜色
                if (hasOldInfo) {
                    SetConsoleTextAttribute(hConsole, oldInfo.wAttributes);
                }
            }
        }

        if (type == QtFatalMsg) {
            abort();
        }
    }

    static bool s_consoleAttached;
    static bool s_consoleCreatedByUs;
    static bool s_vtEnabled;
};

inline bool ConsoleLogger::s_consoleAttached = false;
inline bool ConsoleLogger::s_consoleCreatedByUs = false;
inline bool ConsoleLogger::s_vtEnabled = false;

#define ATTACH_CONSOLE(title) ConsoleLogger::attachConsole(title)
#define DETACH_CONSOLE() ConsoleLogger::detachConsole()

#else // 非 Windows 平台

class ConsoleLogger
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

#endif // CONSOLELOGGER_H
