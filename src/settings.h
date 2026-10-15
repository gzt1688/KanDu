#pragma once
// KanDu 配置：QSettings 读写（%APPDATA%\KanDu\KanDu.ini）
#include <QString>
#include <QList>
#include <QStringList>

namespace kandu {

inline const char *DBL_ALWAYS_ON_TOP = "always_on_top";
inline const char *SAVE_FILTER =
    "图片 (*.jpg *.jpeg *.png *.gif *.bmp *.ico);;所有文件 (*.*)";

struct Settings {
    QString theme = "light";            // light / dark
    QString bgColor = "#e8e8ea";
    bool fitOnOpen = true;              // 打开后窗口贴合图片比例（无黑边）
    bool showStatusBar = true;
    bool wheelInvert = false;
    double wheelStep = 1.25;
    QString doubleClickAction = DBL_ALWAYS_ON_TOP;
    bool dragPan = true;                // 左键拖拽平移
    bool confirmOverwrite = true;
    QList<int> windowGeometry;          // [x, y, w, h]
    QString lastDir;

    void load();
    void save() const;
};

// 支持的图片扩展名
QStringList supportedExtensions();
bool isSupportedImage(const QString &fileName);
// 自然排序：数字段按数值比较
int naturalCompare(const QString &a, const QString &b);

} // namespace kandu
