#include "settings.h"

#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QCollator>

namespace kandu {

void Settings::load()
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "KanDu", "KanDu");
    theme            = s.value("theme", theme).toString();
    bgColor          = s.value("bgColor", bgColor).toString();
    fitOnOpen        = s.value("fitOnOpen", fitOnOpen).toBool();
    showStatusBar    = s.value("showStatusBar", showStatusBar).toBool();
    wheelInvert      = s.value("wheelInvert", wheelInvert).toBool();
    wheelStep        = s.value("wheelStep", wheelStep).toDouble();
    doubleClickAction= s.value("doubleClickAction", doubleClickAction).toString();
    dragPan          = s.value("dragPan", dragPan).toBool();
    confirmOverwrite = s.value("confirmOverwrite", confirmOverwrite).toBool();
    lastDir          = s.value("lastDir", lastDir).toString();

    const QVariant g = s.value("windowGeometry");
    if (g.canConvert<QList<int>>()) {
        windowGeometry = g.value<QList<int>>();
    }
}

void Settings::save() const
{
    QSettings s(QSettings::IniFormat, QSettings::UserScope, "KanDu", "KanDu");
    s.setValue("theme", theme);
    s.setValue("bgColor", bgColor);
    s.setValue("fitOnOpen", fitOnOpen);
    s.setValue("showStatusBar", showStatusBar);
    s.setValue("wheelInvert", wheelInvert);
    s.setValue("wheelStep", wheelStep);
    s.setValue("doubleClickAction", doubleClickAction);
    s.setValue("dragPan", dragPan);
    s.setValue("confirmOverwrite", confirmOverwrite);
    s.setValue("lastDir", lastDir);
    if (windowGeometry.size() == 4)
        s.setValue("windowGeometry", QVariant::fromValue(windowGeometry));
}

QStringList supportedExtensions()
{
    return {"jpg", "jpeg", "jfif", "png", "gif", "bmp", "webp",
            "tif", "tiff", "ico", "svg"};
}

bool isSupportedImage(const QString &fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    return supportedExtensions().contains(ext);
}

int naturalCompare(const QString &a, const QString &b)
{
    // 分段比较：连续数字按数值，其余按字符
    const int n = qMin(a.size(), b.size());
    int i = 0, j = 0;
    while (i < a.size() && j < b.size()) {
        const QChar ca = a.at(i), cb = b.at(j);
        if (ca.isDigit() && cb.isDigit()) {
            int i2 = i, j2 = j;
            while (i2 < a.size() && a.at(i2).isDigit()) ++i2;
            while (j2 < b.size() && b.at(j2).isDigit()) ++j2;
            const QString na = a.mid(i, i2 - i);
            const QString nb = b.mid(j, j2 - j);
            const QByteArray ba = na.toLatin1(), bb = nb.toLatin1();
            // 去掉前导零后按长度/字典序比较数值
            const QString ta = QString::number(ba.toLongLong());
            const QString tb = QString::number(bb.toLongLong());
            if (ta != tb)
                return ta < tb ? -1 : 1;
            i = i2;
            j = j2;
        } else {
            const int r = QString::compare(ca, cb, Qt::CaseInsensitive);
            if (r != 0)
                return r;
            ++i;
            ++j;
        }
    }
    if (i < a.size()) return 1;
    if (j < b.size()) return -1;
    return 0;
}

} // namespace kandu
