// 自动化自测：驱动真实 UI 交互，验证 KanDu 核心功能并写结果文件。
// 用法：kandu_selftest.exe <测试图片> [结果文件]
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QScrollArea>
#include <QSet>
#include <QPixmap>
#include <QTabWidget>
#include <QTextStream>
#include <QStatusBar>
#include <QtTest/QTest>
#include <windows.h>

#include "mainwindow.h"
#include "imageview.h"
#include "titlebar.h"
#include "settings.h"
#include "settingsdialog.h"
#include "aboutdialog.h"
#include "icons.h"
#include "flatbutton.h"

using kandu::MainWindow;
using kandu::ImageView;

namespace {

QString gResult;

void report(const QString &name, bool ok, const QString &detail = QString())
{
    gResult += QStringLiteral("%1: %2 %3\n").arg(name, ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), detail);
    qInfo().noquote() << (ok ? "[PASS]" : "[FAIL]") << name << detail;
}

bool checkPix(const QImage &img, int x, int y, const QColor &expect, int tol = 6)
{
    if (x < 0 || y < 0 || x >= img.width() || y >= img.height())
        return false;
    const QColor c = img.pixelColor(x, y);
    return qAbs(c.red() - expect.red()) <= tol && qAbs(c.green() - expect.green()) <= tol
        && qAbs(c.blue() - expect.blue()) <= tol;
}

bool isTopmost(HWND hwnd)
{
    return (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("KanDu"));
    QApplication::setOrganizationName(QStringLiteral("KanDu"));

    const QString imgPath = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    const QString outPath = argc > 2 ? QString::fromLocal8Bit(argv[2])
                                     : QStringLiteral("selftest_result.txt");
    if (imgPath.isEmpty()) {
        qCritical() << "需要测试图片参数";
        return 2;
    }

    MainWindow win;
    win.show();
    QTest::qWait(150);

    // 1) 打开图片
    win.openPath(imgPath);
    QTest::qWait(250);
    ImageView *view = win.findChild<ImageView *>();
    report(QStringLiteral("打开图片"), view && view->hasImage());

    // 主窗口截图（README 使用）
    win.grab().save(QFileInfo(outPath).absolutePath() + QStringLiteral("/main_screenshot.png"));

    // 2) 窗口贴合：图片显示区（view）宽高比 ≈ 图片宽高比（无黑边）
    const QImage orig = view->image();
    const double ratio = double(orig.width()) / orig.height();
    const QSize vs = view->size();
    const double winRatio = double(vs.width()) / double(vs.height());
    report(QStringLiteral("窗口贴合无黑边"),
           qAbs(ratio - winRatio) < 0.01,
           QStringLiteral("图片 %1x%2 (ratio=%3) vs 显示区 %4x%5 (ratio=%6)")
               .arg(orig.width()).arg(orig.height()).arg(ratio, 0, 'f', 4)
               .arg(vs.width()).arg(vs.height()).arg(winRatio, 0, 'f', 4));

    // 3) 旋转右 90°：尺寸交换 + 左上角像素应来自原图左下角
    const QColor tl0 = orig.pixelColor(2, 2);
    const QColor bl0 = orig.pixelColor(2, orig.height() - 3);
    QTest::keyClick(&win, Qt::Key_R);
    QTest::qWait(150);
    const QImage rot = view->image();
    bool rotOk = rot.width() == orig.height() && rot.height() == orig.width()
                 && checkPix(rot, 2, 2, bl0);
    report(QStringLiteral("右旋90°"), rotOk,
           QStringLiteral("%1x%2 → %3x%4").arg(orig.width()).arg(orig.height())
               .arg(rot.width()).arg(rot.height()));

    // 4) 水平镜像：左边缘像素应等于原右边缘
    const QColor rEdge = rot.pixelColor(rot.width() - 3, 2);
    QTest::keyClick(&win, Qt::Key_H);
    QTest::qWait(150);
    const QImage fh = view->image();
    report(QStringLiteral("水平镜像"), checkPix(fh, 2, 2, rEdge));

    // 5) 垂直镜像：顶部像素应等于镜像前的底部像素
    const QColor bEdge = fh.pixelColor(2, fh.height() - 3);
    QTest::keyClick(&win, Qt::Key_V);
    QTest::qWait(150);
    const QImage fv = view->image();
    report(QStringLiteral("垂直镜像"), checkPix(fv, 2, 2, bEdge));

    // 6) 置顶：T 键 → WS_EX_TOPMOST
    QTest::keyClick(&win, Qt::Key_T);
    QTest::qWait(120);
    const bool top = isTopmost(reinterpret_cast<HWND>(win.winId()));
    QTest::keyClick(&win, Qt::Key_T);
    QTest::qWait(120);
    const bool untop = !isTopmost(reinterpret_cast<HWND>(win.winId()));
    report(QStringLiteral("双击动作置顶(Key T)"), top && untop,
           QStringLiteral("top=%1 untop=%2").arg(top).arg(untop));

    // 7) 裁剪：进入裁剪 → 拖左上角手柄到中心缩小选框 → Enter 应用
    win.debugToggleCrop();
    QTest::qWait(100);
    const QRect vr = view->rect();
    const QPoint tl(2, 2);                      // 左上角手柄附近
    const QPoint mid(vr.width() / 2, vr.height() / 2);
    QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, tl);
    QTest::mouseMove(view, mid, 30);
    QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, mid);
    QTest::qWait(100);
    QTest::keyClick(&win, Qt::Key_Return);
    QTest::qWait(200);
    const QImage cropped = view->image();
    const bool cropOk = cropped.width() < fv.width() && cropped.height() < fv.height()
                        && cropped.width() > 0 && cropped.height() > 0;
    report(QStringLiteral("拖拽裁剪+Enter应用"), cropOk,
           QStringLiteral("%1x%2 → %3x%4").arg(fv.width()).arg(fv.height())
               .arg(cropped.width()).arg(cropped.height()));

    // 7b) 透视校准：进入 → 把左上/右上角拖到图片范围之外 → Enter 应用，
    //     验证角点可越出原图边界、应用成功且尺寸不变、模式退出
    {
        view->startPerspective();
        QTest::qWait(100);
        const bool entered = view->perspectiveMode();
        const QSize ps0 = view->image().size();
        // 计算手柄的屏幕位置（fit 状态下图像居中显示，用显示区尺寸反推）
        const QRect vr2 = view->rect();
        const double sc = qMin(double(vr2.width()) / ps0.width(),
                               double(vr2.height()) / ps0.height());
        const QPoint tlH(int((vr2.width() - ps0.width() * sc) / 2.0),
                         int((vr2.height() - ps0.height() * sc) / 2.0));
        const QPoint trH(int((vr2.width() + ps0.width() * sc) / 2.0),
                         int((vr2.height() - ps0.height() * sc) / 2.0));
        // 拖到图片之外：左上角向左、右上角向右（进入背景区）
        const QPoint outTL(qMax(0, tlH.x() - 180), tlH.y() + 40);
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, tlH);
        QTest::mouseMove(view, outTL, 30);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, outTL);
        const QPoint outTR(qMin(vr2.width() - 1, trH.x() + 180), trH.y() + 40);
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, trH);
        QTest::mouseMove(view, outTR, 30);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, outTR);
        QTest::qWait(100);
        // 抓取透视模式画面（四角框可越出图片 + 右下角实时结果预览），供视觉核对
        view->grab().save(QFileInfo(outPath).absolutePath() + QStringLiteral("/persp_mode.png"));
        QTest::keyClick(&win, Qt::Key_Return);   // 应用
        QTest::qWait(200);
        const bool applied = !view->perspectiveMode();
        const QSize ps1 = view->image().size();
        const bool ok = entered && applied && ps1 == ps0 && ps1.width() > 0 && ps1.height() > 0;
        report(QStringLiteral("透视校准应用(角点可拖出图片)"), ok,
               QStringLiteral("进入=%1 应用=%2 左上拖至=%3,%4 右上拖至=%5,%6 %7x%8 → %9x%10")
                   .arg(entered).arg(applied)
                   .arg(outTL.x()).arg(outTL.y()).arg(outTR.x()).arg(outTR.y())
                   .arg(ps0.width()).arg(ps0.height()).arg(ps1.width()).arg(ps1.height()));
    }

    // 8) 保存为 PNG 并读回
    const QString savePath = QFileInfo(outPath).absolutePath() + QStringLiteral("/selftest_saved.png");
    const bool saved = cropped.save(savePath, "PNG");
    bool reloadOk = false;
    if (saved) {
        const QImage back(savePath);
        reloadOk = back.width() == cropped.width() && back.height() == cropped.height()
                   && checkPix(back, 2, 2, cropped.pixelColor(2, 2));
    }
    report(QStringLiteral("保存 PNG 并读回"), saved && reloadOk, savePath);

    // 9) 深色主题切换
    win.debugSetTheme(QStringLiteral("dark"));
    QTest::qWait(150);
    const QColor bg = view->background();
    report(QStringLiteral("深色主题背景"), bg == QColor("#3a3a3f"), bg.name());
    // 深色主题截图（README 使用）
    win.grab().save(QFileInfo(outPath).absolutePath() + QStringLiteral("/screenshot_dark.png"));
    win.debugSetTheme(QStringLiteral("light"));
    QTest::qWait(100);

    // 12) 切图保留视图状态：放大/平移后按方向键切换，缩放与位置不重置
    {
        // 在测试图同目录生成第二张图
        const QString nextPath =
            QFileInfo(outPath).absolutePath() + QStringLiteral("/selftest_next.png");
        QImage next(500, 800, QImage::Format_RGB32);
        next.fill(QColor("#22aa88"));
        next.save(nextPath, "PNG");

        const double z0 = view->zoom();
        const QPointF ctr(view->width() / 2.0, view->height() / 2.0);
        view->zoomAt(ctr, 2.0);          // 放大到 2x（进入手动缩放态）
        QTest::qWait(80);
        const double z1 = view->zoom();
        const bool enteredZoom = z1 > z0 && !view->isFit();

        QTest::keyClick(&win, Qt::Key_Right);   // 切到下一张
        QTest::qWait(200);
        const bool keptZoom = qAbs(view->zoom() - z1) < 1e-9 && !view->isFit();
        report(QStringLiteral("切图保留视图状态"), enteredZoom && keptZoom,
               QStringLiteral("缩放 %1 → %2，切图后 %3")
                   .arg(z0, 0, 'f', 3).arg(z1, 0, 'f', 3).arg(view->zoom(), 0, 'f', 3));

        QTest::keyClick(&win, Qt::Key_Left);    // 切回原图
        QTest::qWait(400);
        QFile::remove(nextPath);
    }

    // Logo（README 使用）
    kandu::appIcon().pixmap(128, 128).save(QFileInfo(outPath).absolutePath() + QStringLiteral("/logo.png"));

    // 13) 适应窗口按钮：窗口按图片比例调整，但图片位置/缩放不变（不跳回居中）
    {
        QPushButton *fitBtn = nullptr;
        const auto btns = win.findChildren<QPushButton *>();
        for (QPushButton *b : btns) {
            if (b->text() == QStringLiteral("适应窗口")) {
                fitBtn = b;
                break;
            }
        }
        // 先放大再缩小，制造"图片缩小"的状态（缩小后窗口按显示尺寸调整，fit 后缩放不变）
        view->zoomAt(QPointF(view->width() / 2.0, view->height() / 2.0), 2.0);
        QTest::qWait(80);
        view->zoomAt(QPointF(view->width() / 2.0, view->height() / 2.0), 0.4);
        QTest::qWait(80);
        const double z1 = view->zoom();
        QTest::mousePress(view, Qt::LeftButton, Qt::NoModifier, QPoint(300, 250));
        QTest::mouseMove(view, QPoint(420, 340), 30);
        QTest::mouseRelease(view, Qt::LeftButton, Qt::NoModifier, QPoint(420, 340));
        QTest::qWait(80);

        // 窗口移到屏幕右下区域（初始完全在屏内，且 resize 后不越界），纯验证右下角基准
        const QRect av = win.screen()->availableGeometry();
        win.move(av.right() - 1000, av.bottom() - 750);
        win.resize(900, 700);
        QTest::qWait(100);
        const QSize before = win.size();
        const QPoint brBefore(win.frameGeometry().right(), win.frameGeometry().bottom());
        if (fitBtn)
            QTest::mouseClick(fitBtn, Qt::LeftButton);
        QTest::qWait(250);
        const QSize after = win.size();
        const QPoint brAfter(win.frameGeometry().right(), win.frameGeometry().bottom());
        // 窗口含标题栏/状态栏装饰，校验图片显示区比例（1600x1000 → 1.6）
        const QSize vs = view->size();
        const double vratio = vs.height() > 0 ? double(vs.width()) / vs.height() : 0.0;
        // 窗口=图片显示尺寸时 fit 后缩放保持不变，且图片填满窗口
        const bool keptView = qAbs(view->zoom() - z1) < 0.02;
        const bool filled = qAbs(view->zoom() * 1600.0 - double(vs.width())) < 2.0;
        const bool keptPos = brBefore == brAfter;
        const bool ok = fitBtn != nullptr && after != before && qAbs(vratio - 1.6) < 0.03
                        && keptView && keptPos && filled;
        report(QStringLiteral("适应窗口按钮"), ok,
               QStringLiteral("点击前 %1x%2 → 点击后 %3x%4，显示区 %5x%6，缩放保持 %7，右下角不动=%8，占满=%9")
                   .arg(before.width()).arg(before.height())
                   .arg(after.width()).arg(after.height())
                   .arg(vs.width()).arg(vs.height())
                   .arg(view->zoom(), 0, 'f', 3).arg(keptPos).arg(filled));
    }

    // 14) 窗口越界保护：窗口贴近屏幕底部时点适应窗口，标题栏不超出屏幕
    {
        const QRect av = win.screen()->availableGeometry();
        win.move(av.left() + 60, av.bottom() - 150);
        win.resize(900, 700);
        QTest::qWait(100);
        QPushButton *fitBtn = nullptr;
        for (QPushButton *b : win.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("适应窗口")) {
                fitBtn = b;
                break;
            }
        }
        if (fitBtn)
            QTest::mouseClick(fitBtn, Qt::LeftButton);
        QTest::qWait(200);
        const QRect g = win.frameGeometry();
        const bool inScreen = g.top() >= av.top() && g.left() >= av.left()
                              && g.right() <= av.right() && g.bottom() <= av.bottom();
        report(QStringLiteral("窗口越界保护"), inScreen,
               QStringLiteral("窗口 %1,%2 %3x%4 vs 屏幕 %5x%6")
                   .arg(g.left()).arg(g.top()).arg(g.width()).arg(g.height())
                   .arg(av.width()).arg(av.height()));
    }

    // 15) 状态栏拖动：按住状态栏空白处移动，窗口跟随
    {
        const QPoint w0 = win.frameGeometry().topLeft();
        QStatusBar *sb = win.statusBar();
        QTest::mousePress(sb, Qt::LeftButton, Qt::NoModifier,
                          QPoint(sb->width() / 2, sb->height() / 2));
        QTest::mouseMove(sb, QPoint(sb->width() / 2 + 60, sb->height() / 2), 40);
        QTest::mouseRelease(sb, Qt::LeftButton, Qt::NoModifier,
                            QPoint(sb->width() / 2 + 60, sb->height() / 2));
        QTest::qWait(80);
        const QPoint w1 = win.frameGeometry().topLeft();
        report(QStringLiteral("状态栏拖动窗口"), w1 != w0,
               QStringLiteral("%1,%2 → %3,%4").arg(w0.x()).arg(w0.y())
                   .arg(w1.x()).arg(w1.y()));
    }

    // 16) 标题栏拖动：按住标题栏空白处移动，窗口跟随
    {
        const QPoint w0 = win.frameGeometry().topLeft();
        kandu::TitleBar *tb = win.debugTitleBar();
        QTest::mousePress(tb, Qt::LeftButton, Qt::NoModifier, QPoint(40, 12));
        QTest::mouseMove(tb, QPoint(140, 80), 40);
        QTest::mouseRelease(tb, Qt::LeftButton, Qt::NoModifier, QPoint(140, 80));
        QTest::qWait(80);
        const QPoint w1 = win.frameGeometry().topLeft();
        report(QStringLiteral("标题栏拖动窗口"), w1 != w0,
               QStringLiteral("%1,%2 → %3,%4").arg(w0.x()).arg(w0.y())
                   .arg(w1.x()).arg(w1.y()));
    }

    // 17) 适应窗口跟随当前显示尺寸：缩小图片后点按钮，窗口变小
    {
        view->fitToWindow();
        QTest::qWait(80);
        win.resize(1600, 1064);
        QTest::qWait(80);
        const QSize before = win.size();
        view->zoomAt(QPointF(view->width() / 2.0, view->height() / 2.0), 0.5);
        QTest::qWait(80);
        QPushButton *fitBtn = nullptr;
        for (QPushButton *b : win.findChildren<QPushButton *>()) {
            if (b->text() == QStringLiteral("适应窗口")) {
                fitBtn = b;
                break;
            }
        }
        if (fitBtn)
            QTest::mouseClick(fitBtn, Qt::LeftButton);
        QTest::qWait(250);
        const QSize after = win.size();
        const bool ok = fitBtn && after.width() < before.width() && after.height() < before.height();
        report(QStringLiteral("适应窗口跟随缩小"), ok,
               QStringLiteral("%1x%2 → %3x%4")
                   .arg(before.width()).arg(before.height())
                   .arg(after.width()).arg(after.height()));
    }

    // 18) 状态栏底部边缘缩放：按住状态栏底部向下拉，窗口变高
    {
        QStatusBar *sb = win.statusBar();
        const QSize s0 = win.size();
        QTest::mousePress(sb, Qt::LeftButton, Qt::NoModifier,
                          QPoint(sb->width() / 2, sb->height() - 2));
        QTest::mouseMove(sb, QPoint(sb->width() / 2, sb->height() + 40), 40);
        QTest::mouseRelease(sb, Qt::LeftButton, Qt::NoModifier,
                            QPoint(sb->width() / 2, sb->height() + 40));
        QTest::qWait(80);
        const QSize s1 = win.size();
        report(QStringLiteral("状态栏底部缩放"), s1.height() > s0.height(),
               QStringLiteral("%1 → %2").arg(s0.height()).arg(s1.height()));
    }

    // 19) 子控件上边缘缩放：鼠标在标题栏按钮顶部（窗口顶部 6px 内）按住可拉伸窗口
    {
        auto *tb = win.findChild<kandu::TitleBar *>();
        bool ok = false;
        QString detail = QStringLiteral("未找到标题栏");
        if (tb) {
            QPushButton *pin = tb->pinButton();
            if (pin) {
                const QSize s0 = win.size();
                QTest::mousePress(pin, Qt::LeftButton, Qt::NoModifier,
                                  QPoint(pin->width() / 2, 1));
                QTest::mouseMove(pin, QPoint(pin->width() / 2, 60), 40);
                QTest::mouseRelease(pin, Qt::LeftButton, Qt::NoModifier,
                                    QPoint(pin->width() / 2, 60));
                QTest::qWait(80);
                const QSize s1 = win.size();
                ok = s1.height() != s0.height();
                detail = QStringLiteral("%1 → %2").arg(s0.height()).arg(s1.height());
            } else {
                detail = QStringLiteral("无图钉按钮");
            }
        }
        report(QStringLiteral("子控件上边缘缩放"), ok, detail);
    }

    // 10) 设置对话框渲染：应无漂浮控件（回归：buildDisplayPage 曾重复调用产生左上角残留）
    {
        kandu::Settings s;
        kandu::SettingsDialog dlg(s, &win);
        dlg.show();
        QTest::qWait(250);
        // 页签：0=通用（鼠标+显示合并） 1=关于。默认停在「通用」页，
        // 该页含背景色行，回归点即在此。
        if (auto *tabs = dlg.findChild<QTabWidget *>()) {
            report(QStringLiteral("设置页签为两页"), tabs->count() == 2,
                   QStringLiteral("%1 页").arg(tabs->count()));
            tabs->setCurrentIndex(0);
        }
        QTest::qWait(250);
        const QString shotPath = QFileInfo(outPath).absolutePath() + QStringLiteral("/settings_render.png");
        const bool saved = dlg.grab().save(shotPath);
        report(QStringLiteral("设置对话框渲染"), saved, shotPath);
        // 高度回归：窗口高度应贴合内容，不应留大片空白
        report(QStringLiteral("设置窗口高度自适应"), dlg.height() <= 620,
               QStringLiteral("窗口高 %1px").arg(dlg.height()));
        dlg.close();
        QTest::qWait(50);
    }

    // 10b) 关于页：开源免费说明 + 两个收款码必须真的渲染出来（回归：qrc 未编入 / 资源路径写错时静默空白）
    {
        QPixmap pmWechat(QStringLiteral(":/res/donate_wechat.png"));
        QPixmap pmAlipay(QStringLiteral(":/res/donate_alipay.png"));
        const bool resOk = !pmWechat.isNull() && !pmAlipay.isNull()
                        && pmWechat.width() >= 300 && pmAlipay.width() >= 300;
        report(QStringLiteral("收款码资源已编入qrc"), resOk,
               QStringLiteral("微信 %1x%2 / 支付宝 %3x%4")
                   .arg(pmWechat.width()).arg(pmWechat.height())
                   .arg(pmAlipay.width()).arg(pmAlipay.height()));

        kandu::Settings s;
        kandu::SettingsDialog dlg(s, &win);
        dlg.show();
        if (auto *tabs = dlg.findChild<QTabWidget *>())
            tabs->setCurrentIndex(1); // 关于页
        QTest::qWait(300);
        const QImage img = dlg.grab().toImage();

        // 二维码是黑白高对比：统计灰度处于两端的像素占比，应显著大于普通文字/图标
        int dark = 0, light = 0, total = img.width() * img.height();
        for (int y = 0; y < img.height(); ++y) {
            for (int x = 0; x < img.width(); ++x) {
                const int v = img.pixelColor(x, y).lightness();
                if (v < 60) ++dark; else if (v > 225) ++light;
            }
        }
        const double contrast = double(dark + light) / double(total);
        const bool qrOk = contrast > 0.55;
        report(QStringLiteral("关于页二维码渲染"), qrOk,
               QStringLiteral("黑白像素占比 %1%")
                   .arg(QString::number(contrast * 100, 'f', 1)));

        const QString aboutShot = QFileInfo(outPath).absolutePath()
                                + QStringLiteral("/about_render.png");
        img.save(aboutShot);
        report(QStringLiteral("关于页截图"), true, aboutShot);
        dlg.close();
        QTest::qWait(50);
    }

    // 10c) 菜单「关于 KanDu」对话框：内容与设置页同源，须含收款码且放大窗可打开
    {
        auto *dlg = new QDialog(&win);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(QStringLiteral("关于 KanDu"));
        auto *root = new QVBoxLayout(dlg);
        auto *scroll = new QScrollArea(dlg);
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto *page = new QWidget(scroll);
        auto *lay = new QVBoxLayout(page);
        kandu::buildAboutContent(page, lay);
        scroll->setWidget(page);
        root->addWidget(scroll);
        // 按内容高度开窗（与 showAboutDialog 同一策略），避免下半截空白
        dlg->resize(510, 400);
        dlg->show();
        QTest::qWait(300);

        const QImage img = dlg->grab().toImage();

        // 高度回归：关于页内容应紧凑（初版竖排堆叠时约 640px）。
        // 超过 420px 说明又被拉散了（QLabel 吸收多余高度 / 头部竖排）。
        const int contentH = page->sizeHint().height();
        report(QStringLiteral("关于页紧凑度"), contentH > 0 && contentH <= 420,
               QStringLiteral("内容高 %1px").arg(contentH));

        int dark = 0, light = 0, total = img.width() * img.height();
        for (int y = 0; y < img.height(); ++y)
            for (int x = 0; x < img.width(); ++x) {
                const int v = img.pixelColor(x, y).lightness();
                if (v < 60) ++dark; else if (v > 225) ++light;
            }
        const double contrast = double(dark + light) / double(total);
        report(QStringLiteral("关于对话框渲染"), contrast > 0.55,
               QStringLiteral("黑白像素占比 %1%")
                   .arg(QString::number(contrast * 100, 'f', 1)));

        const QString shot = QFileInfo(outPath).absolutePath()
                           + QStringLiteral("/about_dialog.png");
        img.save(shot);
        report(QStringLiteral("关于对话框截图"), true, shot);

        // 收款码现为直接显示（不再有放大窗）：
        // 契约是「尺寸够扫 + 不可点击」，低于 110px 手机识别会失败
        const QPixmap src(kandu::donateResource(0));
        int minCodePx = 0, clickable = 0;
        for (QLabel *l : page->findChildren<QLabel *>()) {
            if (l->pixmap().isNull())
                continue;
            const QSize s = l->size();
            if (s.width() < 200) { // 只看收款码那种小方块
                minCodePx = qMax(minCodePx, qMin(s.width(), s.height()));
                if (l->focusPolicy() != Qt::NoFocus || l->testAttribute(Qt::WA_ForceDisabled))
                    ++clickable;
            }
        }
        report(QStringLiteral("收款码尺寸可扫"), !src.isNull() && minCodePx >= 110,
               QStringLiteral("实际 %1px（下限 110px）").arg(minCodePx));
        report(QStringLiteral("收款码不可点击"), clickable == 0,
               QStringLiteral("可聚焦控件 %1 个").arg(clickable));

        dlg->close();
        QTest::qWait(50);
    }

    // 11) 置顶图钉 checked 渲染：浅色背景 + 图钉图标可见（回归：背景与图标同色导致纯色）
    {
        auto *pin = win.debugTitleBar()->pinButton();
        pin->setChecked(true);
        QTest::qWait(150);
        const QImage img = pin->grab().toImage();
        QSet<QRgb> colors;
        for (int y = 0; y < img.height(); y += 1)
            for (int x = 0; x < img.width(); x += 1)
                colors.insert(img.pixel(x, y));
        const bool hasIcon = colors.size() >= 4; // 背景 + 图标 + 抗锯齿应不止一种纯色
        const QString shot = QFileInfo(outPath).absolutePath() + QStringLiteral("/pin_checked.png");
        img.save(shot);
        report(QStringLiteral("置顶图钉checked渲染"), hasIcon,
               QStringLiteral("%1 种颜色").arg(colors.size()));
        pin->setChecked(false);
        QTest::qWait(50);
    }

    QFile f(outPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream(&f) << gResult;
    }
    qInfo().noquote() << "结果已写入" << outPath;
    return gResult.contains("FAIL") ? 1 : 0;
}
