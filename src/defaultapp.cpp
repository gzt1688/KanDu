#include "defaultapp.h"

#include <QCoreApplication>
#include <QDir>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

namespace kandu {

static const wchar_t *kProgId = L"KanDu.Image";
static const wchar_t *kExts[] = {
    L".jpg", L".jpeg", L".jfif", L".png", L".gif", L".bmp",
    L".webp", L".tif", L".tiff", L".ico", L".svg",
};

static bool writeKey(HKEY root, const QString &subKey, const QString &name,
                     const QString &value)
{
    HKEY hKey = nullptr;
    LONG r = RegCreateKeyExW(root,
                             reinterpret_cast<LPCWSTR>(subKey.utf16()), 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &hKey, nullptr);
    if (r != ERROR_SUCCESS)
        return false;
    // REG_SZ 必须写入 UTF-16LE 字节（含结尾空字符）
    r = RegSetValueExW(hKey, reinterpret_cast<LPCWSTR>(name.utf16()), 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(value.utf16()),
                       DWORD((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);
    return r == ERROR_SUCCESS;
}

QPair<bool, QString> registerDefaultViewer()
{
    // 目标 exe（当前运行的程序）
    const QString target = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString cmd = QStringLiteral("\"%1\" \"%2\"").arg(target, QStringLiteral("%1"));
    const QString icon = QStringLiteral("\"%1\",0").arg(target);

    const QString progKey = QStringLiteral("Software\\Classes\\%1").arg(QString::fromWCharArray(kProgId));
    const QString shellKey = progKey + QStringLiteral("\\shell\\open\\command");
    const QString iconKey = progKey + QStringLiteral("\\DefaultIcon");
    // “打开方式”对话框顶部显示的程序名（优先级：exe 版本资源 > FriendlyAppName > 文件名）
    const QString appKey = QStringLiteral("Software\\Classes\\Applications\\KanDu.exe");

    // 1) 注册程序标识
    if (!writeKey(HKEY_CURRENT_USER, progKey, QStringLiteral("FriendlyTypeName"),
                  QStringLiteral("KanDu 看图")))
        return {false, QStringLiteral("注册程序标识失败")};
    if (!writeKey(HKEY_CURRENT_USER, shellKey, QString(), cmd))
        return {false, QStringLiteral("注册打开命令失败")};
    if (!writeKey(HKEY_CURRENT_USER, iconKey, QString(), icon))
        return {false, QStringLiteral("注册图标失败")};
    writeKey(HKEY_CURRENT_USER, appKey, QStringLiteral("FriendlyAppName"),
             QStringLiteral("KanDu 看图"));

    // 2) 把扩展名关联到 KanDu（并尽力设为默认）
    for (const wchar_t *ext : kExts) {
        const QString e = QString::fromWCharArray(ext);
        const QString openWith = QStringLiteral("Software\\Classes\\%1\\OpenWithProgIds").arg(e);
        writeKey(HKEY_CURRENT_USER, openWith, QString::fromWCharArray(kProgId), QString());
        // 尽力设置默认（部分 Windows 版本会忽略）
        const QString extKey = QStringLiteral("Software\\Classes\\%1").arg(e);
        writeKey(HKEY_CURRENT_USER, extKey, QString(), QString::fromWCharArray(kProgId));
    }

    // 3) 通知系统刷新文件关联
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return {true, target};
}

} // namespace kandu
