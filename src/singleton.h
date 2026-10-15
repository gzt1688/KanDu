#pragma once
// 单实例：Windows 互斥量检测已有实例，WM_COPYDATA 把新打开的图片传给已有窗口。
#include <QString>

namespace kandu {

// 尝试获取单实例互斥。返回 true 表示本进程是首个实例（应继续运行）；
// 返回 false 表示已有实例在运行（调用方应把参数转发后退出）。
bool acquireSingleton();

// 把图片路径发给已有 KanDu 窗口（无窗口时返回 false）。
bool sendPathToExisting(const QString &path);

} // namespace kandu
