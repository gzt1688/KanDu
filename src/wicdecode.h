#pragma once
// Windows WIC 兜底解码：Qt 缺少插件（webp/tiff 等）时，用系统 WIC 解码。
#include <QImage>
#include <QString>

namespace kandu {

// 返回空 QImage 表示失败。
QImage wicLoadImage(const QString &path);

// Qt 是否内置该格式的编码器（用于保存时判断）。
bool qtCanEncode(const QString &extension);

} // namespace kandu
