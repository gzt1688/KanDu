#pragma once
// 关于页内容：图标 / 版本 / 开源免费说明 / 收款码。
// 菜单「关于 KanDu」与设置对话框「关于」页共用，保证两处文案一致。
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QVBoxLayout;

namespace kandu {

// 把关于页的全部内容（不含外层布局）依次加入 lay，调用方自行决定对齐与留白。
// withDonate=false 时省略收款码区块（窄空间场景）。
void buildAboutContent(QWidget *owner, QVBoxLayout *lay, bool withDonate = true);

// 收款码资源路径（已编入 qrc）
QString donateResource(int index); // 0=微信 1=支付宝

// 独立的「关于 KanDu」对话框（菜单 / 右键菜单用）。
void showAboutDialog(QWidget *parent);

} // namespace kandu
