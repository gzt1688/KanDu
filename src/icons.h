#pragma once
// KanDu 图标：全部由 QPainter 程序绘制，无外部素材文件。
#include <QIcon>
#include <QColor>

namespace kandu {

// 旋转箭头：大半圈圆弧 + 大号端部箭头（方向清晰可辨）
QIcon ringArrow(const QColor &c, bool clockwise, double size = 20.0);
// 水平镜像：竖直对称轴 + 左右两个朝向轴的箭头
QIcon flipHIcon(const QColor &c, double size = 20.0);
// 垂直镜像：水平对称轴 + 上下两个朝向轴的箭头
QIcon flipVIcon(const QColor &c, double size = 20.0);
// 裁剪：左上角 + 右下角两组直角（经典裁剪图标）
QIcon cropIcon(const QColor &c, double size = 20.0);
// 摆正：一条倾斜参考线（两端圆点）+ 水平基准虚线
QIcon straightenIcon(const QColor &c, double size = 20.0);
// 透视校准：透视梯形网格 + 四角圆点
QIcon perspectiveIcon(const QColor &c, double size = 20.0);
// 保存：软盘（外框 + 标签 + 底部小拨片）
QIcon saveIcon(const QColor &c, double size = 20.0);
// 设置：齿轮（圆环 + 6 个齿）
QIcon gearIcon(const QColor &c, double size = 20.0);
// 图钉（侧视大头针）：扁圆帽 + 细长针杆
QIcon pinIcon(const QColor &c, double size = 20.0);
// 应用图标：蓝色圆角方 + 手绘直线白 K + 右侧两个小圆点（眼睛）
QIcon appIcon();

} // namespace kandu
