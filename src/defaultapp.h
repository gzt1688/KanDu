#pragma once
// 设置为默认看图软件：注册当前用户的文件关联（HKCU，无需管理员权限）。
#include <QString>
#include <QPair>

namespace kandu {

// 返回 (是否成功, 说明文字)
QPair<bool, QString> registerDefaultViewer();

} // namespace kandu
