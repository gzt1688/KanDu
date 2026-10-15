#include "singleton.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <QDir>

namespace kandu {

namespace {
constexpr wchar_t kMutexName[] = L"Local\\KanDu.SingletonMutex";
constexpr UINT kCopyDataId = 0x4B44; // "KD"

// 找到正在运行的 KanDu 主窗口（标题为 KanDu 且 exe 名为 KanDu.exe）
HWND findKanDuWindow()
{
    HWND found = nullptr;
    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        HWND *out = reinterpret_cast<HWND *>(lp);
        if (!IsWindowVisible(hwnd))
            return TRUE;
        wchar_t title[128] = {};
        if (GetWindowTextW(hwnd, title, 128) == 0)
            return TRUE;
        // 窗口标题为 “KanDu” 或 “KanDu — 文件名（尺寸）” 开头
        if (wcsncmp(title, L"KanDu", 5) != 0)
            return TRUE;
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == 0)
            return TRUE;
        wchar_t path[MAX_PATH] = {};
        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!proc)
            return TRUE;
        DWORD len = MAX_PATH;
        QueryFullProcessImageNameW(proc, 0, path, &len);
        CloseHandle(proc);
        const QString exe = QString::fromWCharArray(path);
        if (exe.endsWith(QLatin1String("KanDu.exe"), Qt::CaseInsensitive)) {
            *out = hwnd;
            return FALSE; // 找到即停
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&found));
    return found;
}
} // namespace

bool acquireSingleton()
{
    HANDLE mutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!mutex)
        return true; // 无法创建互斥时不阻塞启动
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }
    // 互斥句柄由本进程持有，进程退出时由系统释放
    return true;
}

bool sendPathToExisting(const QString &path)
{
    const HWND target = findKanDuWindow();
    if (!target)
        return false;
    const std::wstring w = path.toStdWString();
    COPYDATASTRUCT cds{};
    cds.dwData = kCopyDataId;
    cds.cbData = DWORD((w.size() + 1) * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t *>(w.c_str());
    const LRESULT r = SendMessageTimeoutW(target, WM_COPYDATA, 0,
                                          reinterpret_cast<LPARAM>(&cds),
                                          SMTO_ABORTIFHUNG, 3000, nullptr);
    return r != 0;
}

} // namespace kandu
