#include "mainwindow.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QImageReader>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QScreen>
#include <QStatusBar>
#include <QTextBrowser>
#include <QDialog>
#include <QPushButton>
#include <QClipboard>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <algorithm>

#include "imageview.h"
#include "titlebar.h"
#include "flatbutton.h"
#include "icons.h"
#include "settingsdialog.h"
#include "aboutdialog.h"
#include "defaultapp.h"
#include "wicdecode.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace kandu {

namespace {
constexpr int kEdge = 6; // 无边框窗口边缘缩放热区（像素）

struct Theme {
    QString viewBg;
    QColor fg;
    QColor pin;
    QColor pinActive;
    QColor pinCheckedBg;
    QColor titleBg;
    QColor hover;
    QColor sep;
    QColor statusBg;
    QColor statusFg;
};

Theme themeFor(const QString &name)
{
    Theme t;
    if (name == QLatin1String("dark")) {
        t.viewBg = "#3a3a3f";
        t.fg = QColor("#d8d8da");
        t.pin = QColor("#c8c8ca");
        t.pinActive = QColor("#4a9eff");
        t.pinCheckedBg = QColor("#3a5f8a");
        t.titleBg = QColor("#2d2d30");
        t.hover = QColor("#3f3f46");
        t.sep = QColor("#3a5f8a");
        t.statusBg = QColor("#2d2d30");
        t.statusFg = QColor("#c8c8ca");
    } else {
        t.viewBg = "#e8e8ea";
        t.fg = QColor("#3a3a3c");
        t.pin = QColor("#555558");
        t.pinActive = QColor("#1e6fd9");
        t.pinCheckedBg = QColor("#cfe3fb");
        t.titleBg = QColor("#f3f3f5");
        t.hover = QColor("#e2e2e6");
        t.sep = QColor("#cfe3fb");
        t.statusBg = QColor("#f3f3f5");
        t.statusFg = QColor("#555558");
    }
    return t;
}

const QString kHelpText = QStringLiteral(
    "<h3>KanDu 快捷键与鼠标操作</h3>"
    "<p><b>标题栏</b>：设置 · 左旋 · 右旋 · 水平镜像 · 垂直镜像 · 裁剪 · 覆盖保存 · 置顶（图钉）· 最小化/最大化/关闭（拖动空白移动窗口，双击最大化）</p>"
    "<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\">"
    "<tr><td><b>操作</b></td><td><b>鼠标 / 键盘</b></td></tr>"
    "<tr><td>打开图片</td><td>Ctrl+O，或将图片拖入窗口</td></tr>"
    "<tr><td>覆盖保存 / 另存为</td><td>Ctrl+S / Ctrl+Shift+S</td></tr>"
    "<tr><td>上一张 / 下一张</td><td>← / →，↑ / ↓，PageUp / PageDown，空格 / 退格</td></tr>"
    "<tr><td>滚轮缩放</td><td>鼠标滚轮（以光标为中心，方向与速度可在设置中修改）</td></tr>"
    "<tr><td>平移图片</td><td>按住左键或中键拖拽（放大后）</td></tr>"
    "<tr><td>双击</td><td>默认：切换窗口置顶（可在设置中改为适应窗口 / 实际大小 / 全屏）</td></tr>"
    "<tr><td>适应窗口</td><td>F 或 Ctrl+0</td></tr>"
    "<tr><td>实际大小</td><td>1 或 Ctrl+1</td></tr>"
    "<tr><td>放大 / 缩小</td><td>+ / -（或 Ctrl++ / Ctrl+-）</td></tr>"
    "<tr><td>裁剪</td><td>C 进入，拖动选择区域，Enter 应用，Esc 取消</td></tr>"
    "<tr><td>两点摆正</td><td>S 进入，在图上点两个参考点连成线，H/V 切水平/垂直基准，Enter 应用</td></tr>"
    "<tr><td>透视校准</td><td>P 进入，拖动四个角点定义校正区域（框+透视网格），Enter 应用，Esc/右键取消</td></tr>"
    "<tr><td>旋转右 / 左</td><td>R / Shift+R</td></tr>"
    "<tr><td>水平 / 垂直镜像</td><td>H / V</td></tr>"
    "<tr><td>窗口置顶</td><td>T（或标题栏图钉按钮）</td></tr>"
    "<tr><td>全屏</td><td>F11，Esc 退出</td></tr>"
    "<tr><td>复制图片到剪贴板</td><td>Ctrl+C</td></tr>"
    "</table>");

bool setTopmost(HWND hwnd, bool topmost)
{
    return SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                        0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE) != 0;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    mSettings.load();

    // 无边框窗口需显式保留最小化/最大化 hint，否则原生窗口缺少
    // WS_MINIMIZEBOX/WS_MAXIMIZEBOX 样式，点击任务栏图标无法最小化/还原
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowMinimizeButtonHint
                   | Qt::WindowMaximizeButtonHint);
    setWindowIcon(appIcon());
    setMinimumSize(480, 240);

    mView = new ImageView(mSettings, this);
    setCentralWidget(mView);

    buildActions();

    mTitleBar = new TitleBar(this);
    setMenuWidget(mTitleBar);
    buildStatusBar();

    // 信号
    connect(mView, &ImageView::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(mView, &ImageView::doubleClicked, this, &MainWindow::onDoubleClicked);
    connect(mView, &ImageView::contextMenuRequested, this, &MainWindow::showContextMenu);
    connect(mView, &ImageView::fileDropped, this, [this](const QString &p) { openPath(p); });
    connect(mView, &ImageView::cropApplyRequested, this, &MainWindow::applyCrop);
    connect(mView, &ImageView::cropCancelRequested, this, &MainWindow::cancelCrop);
    connect(mView, &ImageView::straightenApplyRequested, this, &MainWindow::applyStraighten);
    connect(mView, &ImageView::straightenCancelRequested, this, &MainWindow::cancelStraighten);
    connect(mView, &ImageView::perspectiveCancelRequested, this, &MainWindow::cancelPerspective);
    connect(mTitleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(mTitleBar, &TitleBar::maximizeRequested, this, [this] {
        if (isMaximized()) showNormal(); else showMaximized();
        mTitleBar->setMaximized(isMaximized());
    });
    connect(mTitleBar, &TitleBar::closeRequested, this, &QWidget::close);

    // 置顶图钉
    mTitleBar->pinButton()->setChecked(mAlwaysOnTop);
    connect(mTitleBar->pinButton(), &QPushButton::toggled, this,
            [this](bool checked) { toggleTop(checked); });

    // 标题栏按钮 -> 动作
    mTitleBar->button("rot")->setEnabled(false);
    mTitleBar->button("flip")->setEnabled(false);
    mTitleBar->button("crop")->setEnabled(false);
    mTitleBar->button("straighten")->setEnabled(false);
    mTitleBar->button("persp")->setEnabled(false);
    mTitleBar->button("save")->setEnabled(false);
    connect(mTitleBar->button("settings"), &QPushButton::clicked, this,
            [this] { openSettingsDialog(); });
    connect(mTitleBar->button("rot"), &QPushButton::clicked, this, [this] { rotate(90); });
    connect(mTitleBar->button("flip"), &QPushButton::clicked, this, [this] { flipH(); });
    connect(mTitleBar->button("crop"), &QPushButton::clicked, this, [this] { toggleCrop(); });
    connect(mTitleBar->button("straighten"), &QPushButton::clicked, this, [this] { toggleStraighten(); });
    connect(mTitleBar->button("persp"), &QPushButton::clicked, this, [this] { togglePerspective(); });
    connect(mTitleBar->button("save"), &QPushButton::clicked, this, [this] { saveCurrent(); });

    // 键盘与边缘缩放：拦截发给子控件的按键/鼠标事件
    mView->installEventFilter(this);
    mTitleBar->installEventFilter(this);
    // 标题栏 / 状态栏及其子控件：纳入边缘缩放（安装事件过滤器）
    // 并开启鼠标追踪——无按钮移动也能收到 MouseMove，边缘缩放箭头才会出现
    const auto tbAll = mTitleBar->findChildren<QWidget *>();
    mTitleBar->setMouseTracking(true);
    for (QWidget *c : tbAll) {
        c->installEventFilter(this);
        c->setMouseTracking(true);
    }
    const auto sbAll = statusBar()->findChildren<QWidget *>();
    statusBar()->setMouseTracking(true);
    for (QWidget *c : sbAll) {
        c->installEventFilter(this);
        c->setMouseTracking(true);
    }

    restoreGeometryFromSettings();
    applySettings();
    refreshActions();
}

void MainWindow::buildActions()
{
    actOpen = new QAction(QStringLiteral("打开…\tCtrl+O"), this);
    connect(actOpen, &QAction::triggered, this, [this] { openDialog(); });

    actSave = new QAction(QStringLiteral("覆盖保存\tCtrl+S"), this);
    connect(actSave, &QAction::triggered, this, [this] { saveCurrent(); });

    actSaveAs = new QAction(QStringLiteral("另存为…\tCtrl+Shift+S"), this);
    connect(actSaveAs, &QAction::triggered, this, [this] { saveAsCurrent(); });

    actCopy = new QAction(QStringLiteral("复制图片\tCtrl+C"), this);
    connect(actCopy, &QAction::triggered, this, [this] {
        if (!mImage.isNull())
            QApplication::clipboard()->setImage(mImage);
    });

    actCrop = new QAction(QStringLiteral("裁剪\tC"), this);
    connect(actCrop, &QAction::triggered, this, [this] { toggleCrop(); });
    actStraighten = new QAction(QStringLiteral("摆正\tS"), this);
    connect(actStraighten, &QAction::triggered, this, [this] { toggleStraighten(); });
    actPerspective = new QAction(QStringLiteral("透视校准\tP"), this);
    connect(actPerspective, &QAction::triggered, this, [this] { togglePerspective(); });
    actRotR = new QAction(QStringLiteral("右旋\tR"), this);
    connect(actRotR, &QAction::triggered, this, [this] { rotate(90); });
    actRotL = new QAction(QStringLiteral("左旋\tShift+R"), this);
    connect(actRotL, &QAction::triggered, this, [this] { rotate(-90); });
    actFlipH = new QAction(QStringLiteral("水平镜像\tH"), this);
    connect(actFlipH, &QAction::triggered, this, [this] { flipH(); });
    actFlipV = new QAction(QStringLiteral("垂直镜像\tV"), this);
    connect(actFlipV, &QAction::triggered, this, [this] { flipV(); });

    actFit = new QAction(QStringLiteral("适应窗口\tF"), this);
    connect(actFit, &QAction::triggered, this, [this] { mView->fitToWindow(); });
    actActual = new QAction(QStringLiteral("实际大小\t1"), this);
    connect(actActual, &QAction::triggered, this, [this] { mView->setActualSize(); });
    actZoomIn = new QAction(QStringLiteral("放大\t+"), this);
    connect(actZoomIn, &QAction::triggered, this, [this] { mView->zoomIn(); });
    actZoomOut = new QAction(QStringLiteral("缩小\t-"), this);
    connect(actZoomOut, &QAction::triggered, this, [this] { mView->zoomOut(); });

    actPrev = new QAction(QStringLiteral("上一张\t←"), this);
    connect(actPrev, &QAction::triggered, this, [this] { go(-1); });
    actNext = new QAction(QStringLiteral("下一张\t→"), this);
    connect(actNext, &QAction::triggered, this, [this] { go(1); });

    actFullscreen = new QAction(QStringLiteral("全屏\tF11"), this);
    actFullscreen->setCheckable(true);
    connect(actFullscreen, &QAction::triggered, this, [this](bool checked) {
        toggleFullscreen(checked);
    });

    actTop = new QAction(QStringLiteral("窗口置顶\tT"), this);
    actTop->setCheckable(true);
    connect(actTop, &QAction::triggered, this, [this](bool checked) { toggleTop(checked); });

    actStatusBar = new QAction(QStringLiteral("显示状态栏"), this);
    actStatusBar->setCheckable(true);
    connect(actStatusBar, &QAction::triggered, this, [this](bool checked) {
        toggleStatusBar(checked);
    });

    actSettings = new QAction(QStringLiteral("设置…"), this);
    connect(actSettings, &QAction::triggered, this, [this] { openSettingsDialog(); });
    actHelp = new QAction(QStringLiteral("快捷键说明"), this);
    connect(actHelp, &QAction::triggered, this, [this] { showHelp(); });
    actAbout = new QAction(QStringLiteral("关于 KanDu"), this);
    connect(actAbout, &QAction::triggered, this, [this] { showAbout(); });
}

void MainWindow::buildStatusBar()
{
    QStatusBar *bar = statusBar();
    bar->setSizeGripEnabled(false);
    bar->installEventFilter(this); // 状态栏空白区域拖动移动窗口
    mLblFile = new QLabel(bar);
    mLblInfo = new QLabel(bar);
    mLblIdx = new QLabel(bar);
    bar->addWidget(mLblFile, 1);
    bar->addWidget(mLblInfo);

    // 张数导航：❮ 1 / 12 ❯，点击箭头翻页（复用 go()，首尾循环）
    QFont navFont = font();
    navFont.setBold(true);
    navFont.setPointSize(13);
    mNavPrev = new FlatButton(bar);
    mNavPrev->setText(QStringLiteral("❮"));
    mNavPrev->setPaintText(true);
    mNavPrev->setFont(navFont);
    mNavPrev->setFixedSize(24, 22);
    mNavPrev->setToolTip(QStringLiteral("上一张 (←)"));
    connect(mNavPrev, &QPushButton::clicked, this, [this] { go(-1); });
    mNavNext = new FlatButton(bar);
    mNavNext->setText(QStringLiteral("❯"));
    mNavNext->setPaintText(true);
    mNavNext->setFont(navFont);
    mNavNext->setFixedSize(24, 22);
    mNavNext->setToolTip(QStringLiteral("下一张 (→)"));
    connect(mNavNext, &QPushButton::clicked, this, [this] { go(1); });
    QWidget *navBox = new QWidget(bar);
    auto *navLay = new QHBoxLayout(navBox);
    navLay->setContentsMargins(0, 0, 0, 0);
    navLay->setSpacing(2);
    navLay->addWidget(mNavPrev);
    navLay->addWidget(mLblIdx);
    navLay->addWidget(mNavNext);
    bar->addWidget(navBox);

    // 右下角：适应窗口按钮（窗口按图片当前显示尺寸调整 + 图片填满窗口）
    mBtnFit = new FlatButton(bar);
    mBtnFit->setText(QStringLiteral("适应窗口"));
    mBtnFit->setPaintText(true);
    mBtnFit->setMinimumWidth(72);
    mBtnFit->setToolTip(QStringLiteral("窗口适应图片 (0)"));
    connect(mBtnFit, &QPushButton::clicked, this, [this] { resizeToImage(true); });
    bar->addPermanentWidget(mBtnFit);
}

// ------------------------------------------------------------------ 事件
bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    MSG *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_COPYDATA) {
        const COPYDATASTRUCT *cds = reinterpret_cast<const COPYDATASTRUCT *>(msg->lParam);
        if (cds && cds->dwData == 0x4B44 && cds->lpData && cds->cbData >= 2) {
            const int chars = int(cds->cbData) / int(sizeof(wchar_t)) - 1; // 去结尾空字符
            if (chars > 0) {
                const QString path =
                    QString::fromWCharArray(static_cast<const wchar_t *>(cds->lpData), chars);
                QTimer::singleShot(0, this, [this, path] { openPath(path, true); });
                showNormal();
                raise();
                activateWindow();
                if (result)
                    *result = 1;
                return true;
            }
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == mView && event->type() == QEvent::KeyPress)
        return handleKey(static_cast<QKeyEvent *>(event));

    const QEvent::Type et = event->type();
    const bool mouseEv = et == QEvent::MouseButtonPress
                         || et == QEvent::MouseMove
                         || et == QEvent::MouseButtonRelease;
    if (!mouseEv)
        return QMainWindow::eventFilter(obj, event);

    // 属于标题栏 / 状态栏 / 视图 的控件（含其子控件）统一做边缘缩放与拖动
    const bool onTitle = obj == mTitleBar
                         || (mTitleBar && obj->parent() == mTitleBar);
    const bool onStatus = obj == statusBar()
                          || (statusBar() && obj->parent() == statusBar());
    if (!onTitle && !onStatus && obj != mView)
        return QMainWindow::eventFilter(obj, event);

    auto *me = static_cast<QMouseEvent *>(event);
    const QPoint winPos = me->globalPosition().toPoint() - frameGeometry().topLeft();

    // 摆正按钮：右键进入垂直摆正，左键已由 clicked 进入水平摆正
    if (obj == mTitleBar->button("straighten") && et == QEvent::MouseButtonPress
        && me->button() == Qt::RightButton) {
        toggleStraighten(false);
        return true;
    }
    // 旋转按钮：右键左旋（左键已由 clicked 右旋）
    if (obj == mTitleBar->button("rot") && et == QEvent::MouseButtonPress
        && me->button() == Qt::RightButton) {
        rotate(-90);
        return true;
    }
    // 镜像按钮：右键垂直镜像（左键已由 clicked 水平镜像）
    if (obj == mTitleBar->button("flip") && et == QEvent::MouseButtonPress
        && me->button() == Qt::RightButton) {
        flipV();
        return true;
    }

    const QString edge = edgeFromWindowPos(winPos);

    // ---- 窗口边缘 6px：缩放（含子控件上的边缘） ----
    if (!edge.isEmpty() || !mResizeEdge.isEmpty()) {
        if (et == QEvent::MouseButtonPress) {
            if (me->button() == Qt::LeftButton) {
                mResizeEdge = edge;
                return true;
            }
        } else if (et == QEvent::MouseMove) {
            if (!mResizeEdge.isEmpty()) {
                applyResize(me->globalPosition().toPoint());
                return true;
            }
            if (!(me->buttons() & Qt::LeftButton)) {
                auto *w = qobject_cast<QWidget *>(obj);
                if (w) {
                    mEdgeCursorWidget = w;
                    w->setCursor(cursorForEdge(edge));
                }
                return true;
            }
        } else if (et == QEvent::MouseButtonRelease) {
            if (!mResizeEdge.isEmpty()) {
                mResizeEdge.clear();
                if (mEdgeCursorWidget) {
                    mEdgeCursorWidget->unsetCursor();
                    mEdgeCursorWidget = nullptr;
                }
                return true;
            }
        }
        return false;
    }

    // ---- 非边缘 ----
    // 移出边缘：清除残留的缩放光标（恢复按钮手型/默认）
    if (et == QEvent::MouseMove && !(me->buttons() & Qt::LeftButton) && mEdgeCursorWidget) {
        mEdgeCursorWidget->unsetCursor();
        mEdgeCursorWidget = nullptr;
    }
    // 标题栏 / 状态栏空白区拖动窗口；子控件（按钮、标签）与视图正常处理
    if (obj == mTitleBar || obj == statusBar())
        return handleWindowDrag(obj, event);
    return QMainWindow::eventFilter(obj, event);
}

// 标题栏 / 状态栏空白区域按住拖动，移动整个窗口
bool MainWindow::handleWindowDrag(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);
    auto *me = static_cast<QMouseEvent *>(event);
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        if (me->button() == Qt::LeftButton && !isMaximized() && !isFullScreen()) {
            mDragWindow = true;
            mDragOffset = me->globalPosition().toPoint() - frameGeometry().topLeft();
            return true;
        }
        break;
    case QEvent::MouseMove:
        if (mDragWindow) {
            move(me->globalPosition().toPoint() - mDragOffset);
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (mDragWindow && me->button() == Qt::LeftButton) {
            mDragWindow = false;
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!handleKey(event))
        QMainWindow::keyPressEvent(event);
}

bool MainWindow::handleKey(QKeyEvent *e)
{
    if (mView->cropMode()) {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
            applyCrop();
        else if (e->key() == Qt::Key_Escape)
            cancelCrop();
        return true;
    }

    if (mView->straightenMode()) {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
            double deg = 0.0;
            if (mView->straightenAngle(deg))
                applyStraighten(deg);
        } else if (e->key() == Qt::Key_Escape) {
            cancelStraighten();
        } else if (e->key() == Qt::Key_H) {
            if (!mView->straightenHoriz())
                mView->toggleStraightenAxis();
        } else if (e->key() == Qt::Key_V) {
            if (mView->straightenHoriz())
                mView->toggleStraightenAxis();
        }
        return true;
    }

    if (mView->perspectiveMode()) {
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
            applyPerspective();
        else if (e->key() == Qt::Key_Escape)
            cancelPerspective();
        return true;
    }

    const int k = e->key();
    const Qt::KeyboardModifiers mods = e->modifiers();
    const bool ctrl = mods & Qt::ControlModifier;
    const bool shift = mods & Qt::ShiftModifier;

    if (ctrl) {
        switch (k) {
        case Qt::Key_O: openDialog(); return true;
        case Qt::Key_S:
            if (shift) saveAsCurrent(); else saveCurrent();
            return true;
        case Qt::Key_C:
            if (!mImage.isNull())
                QApplication::clipboard()->setImage(mImage);
            return true;
        case Qt::Key_Q:
        case Qt::Key_W: close(); return true;
        case Qt::Key_0: mView->fitToWindow(); return true;
        case Qt::Key_1: mView->setActualSize(); return true;
        case Qt::Key_Plus:
        case Qt::Key_Equal: mView->zoomIn(); return true;
        case Qt::Key_Minus: mView->zoomOut(); return true;
        default: break;
        }
        return true;
    }

    switch (k) {
    case Qt::Key_Escape:
        if (isFullScreen())
            toggleFullscreen(false);
        return true;
    case Qt::Key_F11: toggleFullscreen(!isFullScreen()); return true;
    case Qt::Key_F: mView->fitToWindow(); return true;
    case Qt::Key_1: mView->setActualSize(); return true;
    case Qt::Key_Plus:
    case Qt::Key_Equal: mView->zoomIn(); return true;
    case Qt::Key_Minus: mView->zoomOut(); return true;
    case Qt::Key_R: rotate(shift ? -90 : 90); return true;
    case Qt::Key_H: flipH(); return true;
    case Qt::Key_V: flipV(); return true;
    case Qt::Key_C: toggleCrop(); return true;
    case Qt::Key_S: toggleStraighten(); return true;
    case Qt::Key_P: togglePerspective(); return true;
    case Qt::Key_T: toggleTop(!mAlwaysOnTop); return true;
    case Qt::Key_Left:
    case Qt::Key_Up:
    case Qt::Key_PageUp:
    case Qt::Key_Backspace: go(-1); return true;
    case Qt::Key_Right:
    case Qt::Key_Down:
    case Qt::Key_PageDown:
    case Qt::Key_Space: go(1); return true;
    default: break;
    }
    return false;
}

// ------------------------------------------------------------ 无边框缩放
QString MainWindow::edgeFromWindowPos(const QPoint &wp) const
{
    if (isMaximized() || isFullScreen() || mView->cropMode() || mView->straightenMode()
        || mView->perspectiveMode())
        return {};
    const int x = wp.x(), y = wp.y();
    const int w = width(), h = height();
    const bool left = x <= kEdge, right = x >= w - kEdge;
    const bool top = y <= kEdge, bottom = y >= h - kEdge;
    if (left && top) return QStringLiteral("tl");
    if (right && top) return QStringLiteral("tr");
    if (left && bottom) return QStringLiteral("bl");
    if (right && bottom) return QStringLiteral("br");
    if (left) return QStringLiteral("l");
    if (right) return QStringLiteral("r");
    if (top) return QStringLiteral("t");
    if (bottom) return QStringLiteral("b");
    return {};
}

Qt::CursorShape MainWindow::cursorForEdge(const QString &edge)
{
    if (edge == "tl" || edge == "br") return Qt::SizeFDiagCursor;
    if (edge == "tr" || edge == "bl") return Qt::SizeBDiagCursor;
    if (edge == "t" || edge == "b") return Qt::SizeVerCursor;
    if (edge == "l" || edge == "r") return Qt::SizeHorCursor;
    return Qt::ArrowCursor;
}

void MainWindow::applyResize(const QPoint &gpos)
{
    if (mResizeEdge.isEmpty())
        return;
    QRect g = geometry();
    int x = g.x(), y = g.y(), w = g.width(), h = g.height();
    const int minW = minimumWidth(), minH = minimumHeight();
    if (mResizeEdge.contains('l')) {
        const int nw = g.x() + g.width() - gpos.x();
        if (nw >= minW) { x = gpos.x(); w = nw; }
    }
    if (mResizeEdge.contains('r'))
        w = qMax(minW, gpos.x() - g.x());
    if (mResizeEdge.contains('t')) {
        const int nh = g.y() + g.height() - gpos.y();
        if (nh >= minH) { y = gpos.y(); h = nh; }
    }
    if (mResizeEdge.contains('b'))
        h = qMax(minH, gpos.y() - g.y());
    setGeometry(x, y, w, h);
}

// ------------------------------------------------------------------ 文件
void MainWindow::openDialog()
{
    QString start = mSettings.lastDir;
    if (start.isEmpty())
        start = mPath.isEmpty() ? QDir::homePath() : QFileInfo(mPath).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开图片"), start,
        QStringLiteral("所有支持的图片 (*.jpg *.jpeg *.jfif *.png *.gif *.bmp *.webp *.tif *.tiff *.ico *.svg);;所有文件 (*.*)"));
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::openPath(const QString &path, bool promptIfDirty, bool preserveView)
{
    if (promptIfDirty && !confirmReplace())
        return;
    const QString abs = QFileInfo(path).absoluteFilePath();
    if (!QFileInfo(abs).isFile()) {
        QMessageBox::warning(this, QStringLiteral("KanDu"),
                             QStringLiteral("文件不存在：\n%1").arg(abs));
        return;
    }
    QImageReader reader(abs);
    reader.setAutoTransform(true); // 自动应用 EXIF 方向
    QImage img = reader.read();
    if (img.isNull())
        img = wicLoadImage(abs); // Qt 无插件（webp/tiff 等）时用系统 WIC 兜底
    if (img.isNull()) {
        QMessageBox::warning(this, QStringLiteral("KanDu"),
                             QStringLiteral("无法打开图片：\n%1\n\n%2")
                                 .arg(abs, reader.errorString()));
        return;
    }

    mImage = img;
    mPath = abs;
    mDirty = false;
    mSettings.lastDir = QFileInfo(abs).absolutePath();
    mSettings.save();
    mView->setImage(img, preserveView);
    scanFolder();
    // 切图（方向键）时保持窗口与视图状态，不重新贴合
    if (mSettings.fitOnOpen && !isFullScreen() && !preserveView)
        resizeToImage();
    updateTitle();
    updateStatus();
    refreshActions();
}

void MainWindow::scanFolder()
{
    mFolderFiles.clear();
    mFolderIndex = -1;
    if (mPath.isEmpty())
        return;
    const QDir dir(QFileInfo(mPath).absolutePath());
    QStringList files;
    const QFileInfoList list = dir.entryInfoList(QDir::Files);
    for (const QFileInfo &fi : list) {
        if (isSupportedImage(fi.fileName()))
            files << fi.fileName();
    }
    std::sort(files.begin(), files.end(), [](const QString &a, const QString &b) {
        return naturalCompare(a, b) < 0;
    });
    mFolderFiles = files;
    const QString base = QFileInfo(mPath).fileName();
    const int idx = files.indexOf(base);
    if (idx >= 0)
        mFolderIndex = idx;
}

void MainWindow::go(int delta)
{
    if (mPath.isEmpty() || mFolderFiles.isEmpty())
        return;
    if (!confirmReplace())
        return;
    const int n = mFolderFiles.size();
    const int idx = (mFolderIndex + delta + n) % n;
    openPath(QFileInfo(mPath).absolutePath() + QDir::separator() + mFolderFiles[idx],
             false, true);
}

void MainWindow::resizeToImage(bool fitView)
{
    if (mImage.isNull())
        return;
    // 实际装饰尺寸（标题栏 + 状态栏）
    const int decoW = width() - mView->width();
    const int decoH = height() - mView->height();
    const QRect avail = screen()->availableGeometry();
    const int maxW = qMax(340, avail.width() - 100);
    const int maxH = qMax(240, avail.height() - 140);
    // 按当前显示尺寸（含缩放比例）计算目标窗口，图片缩小/放大后窗口跟随
    const double dispW = mImage.width() * mView->zoom();
    const double dispH = mImage.height() * mView->zoom();
    const double longSide = qMax(dispW, dispH);
    const double up = longSide < 480 ? 480.0 / longSide : 1.0; // 显示尺寸过小时适当放大
    const double s = std::min({maxW / dispW, maxH / dispH, up});
    const int cw = qMax(int(dispW * s), 320);
    const int ch = qMax(int(dispH * s), 240);
    // 以当前窗口右下角为基准调整尺寸（右下角即“适应窗口”按钮所在处）
    const QPoint oldBottomRight(frameGeometry().right(), frameGeometry().bottom());
    resize(cw + decoW, ch + decoH);
    QRect geo = frameGeometry();
    geo.moveBottomRight(oldBottomRight);
    // 越界保护：保证标题栏与窗口主体不超出可用屏幕
    if (geo.top() < avail.top())
        geo.moveTop(avail.top());
    if (geo.left() < avail.left())
        geo.moveLeft(avail.left());
    if (geo.right() > avail.right())
        geo.moveRight(avail.right());
    if (geo.bottom() > avail.bottom())
        geo.moveBottom(avail.bottom());
    move(geo.topLeft());
    // 仅打开图片时贴合视图；按钮触发时保持当前图片位置不变
    if (fitView)
        mView->fitToWindow();
}

// ------------------------------------------------------------------ 保存
bool MainWindow::ask(const QString &text, const QString &okText, const QString &cancelText)
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("KanDu"));
    box.setText(text);
    QPushButton *ok = box.addButton(okText, QMessageBox::AcceptRole);
    box.addButton(cancelText, QMessageBox::RejectRole);
    box.setDefaultButton(ok);
    box.exec();
    return box.clickedButton() == ok;
}

bool MainWindow::saveCurrent(bool askOverwrite)
{
    if (mImage.isNull())
        return false;
    if (mPath.isEmpty() || !QFileInfo(mPath).exists())
        return saveAsCurrent();
    const QString p = mPath;
    if (!qtCanEncode(QFileInfo(p).suffix())) {
        // 原格式（WebP/TIFF/SVG 等）Qt 无写回能力：提示后转另存 PNG
        if (ask(QStringLiteral("原格式 %1 暂不支持写回，将另存为 PNG 图片。\n继续？")
                    .arg(QFileInfo(p).suffix().toUpper()),
                QStringLiteral("另存为 PNG"), QStringLiteral("取消")))
            return saveAsCurrent();
        return false;
    }
    if (askOverwrite && mSettings.confirmOverwrite
        && !ask(QStringLiteral("覆盖保存到原文件？\n%1").arg(p),
                QStringLiteral("覆盖保存"), QStringLiteral("取消")))
        return false;
    if (isAnimated(p) && !ask(QStringLiteral("原文件是动图（GIF/WebP 等）。保存后动画将丢失，变为静态图片。\n仍然继续？"),
                               QStringLiteral("仍然保存"), QStringLiteral("取消")))
        return false;
    if (mImage.save(p)) {
        mDirty = false;
        updateTitle();
        updateStatus();
        return true;
    }
    QMessageBox::warning(this, QStringLiteral("KanDu"),
                         QStringLiteral("保存失败：文件可能被占用或磁盘空间不足。"));
    return false;
}

bool MainWindow::saveAsCurrent()
{
    if (mImage.isNull())
        return false;
    const QString base = mPath.isEmpty() ? QStringLiteral("未命名.png")
                                         : QFileInfo(mPath).fileName();
    QString startDir = mPath.isEmpty() ? (mSettings.lastDir.isEmpty() ? QDir::homePath()
                                                                      : mSettings.lastDir)
                                       : QFileInfo(mPath).absolutePath();
    const QString def = startDir + QDir::separator() + base;
    QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("另存为"), def,
                                              QString::fromUtf8(SAVE_FILTER));
    if (fn.isEmpty())
        return false;
    if (QFileInfo(fn).suffix().isEmpty()) {
        const QString ext = QFileInfo(base).suffix();
        fn += (ext.isEmpty() ? QStringLiteral(".png") : QStringLiteral(".") + ext);
    }
    const QString low = fn.toLower();
    if (mImage.hasAlphaChannel() && (low.endsWith(".jpg") || low.endsWith(".jpeg"))) {
        if (!ask(QStringLiteral("图片包含透明背景，保存为 JPEG 会丢失透明信息（透明区域将变黑）。\n仍然继续？"),
                 QStringLiteral("仍然保存"), QStringLiteral("取消")))
            return false;
    }
    if (mImage.save(fn)) {
        mPath = fn;
        mDirty = false;
        mSettings.lastDir = QFileInfo(fn).absolutePath();
        mSettings.save();
        scanFolder();
        updateTitle();
        updateStatus();
        return true;
    }
    // 无编码器的格式（如 .webp/.tif）→ 提示后自动改存 PNG
    if (!qtCanEncode(QFileInfo(fn).suffix())
        && ask(QStringLiteral("格式 %1 暂不支持保存，将另存为 PNG 图片。\n继续？")
                   .arg(QFileInfo(fn).suffix().toUpper()),
               QStringLiteral("另存为 PNG"), QStringLiteral("取消"))) {
        QString png = fn;
        png.chop(QFileInfo(fn).suffix().length() + 1);
        png += QStringLiteral(".png");
        if (mImage.save(png)) {
            mPath = png;
            mDirty = false;
            mSettings.lastDir = QFileInfo(png).absolutePath();
            mSettings.save();
            scanFolder();
            updateTitle();
            updateStatus();
            return true;
        }
    }
    QMessageBox::warning(this, QStringLiteral("KanDu"),
                         QStringLiteral("保存失败：请检查文件格式与磁盘状态。"));
    return false;
}

bool MainWindow::isAnimated(const QString &path)
{
    QImageReader r(path);
    return r.imageCount() > 1;
}

// ------------------------------------------------------------------ 编辑
void MainWindow::applyEdit(const QImage &newImg)
{
    mImage = newImg;
    mDirty = true;
    mView->setImage(newImg);
    updateTitle();
    updateStatus();
}

void MainWindow::rotate(int angle)
{
    if (mImage.isNull())
        return;
    QTransform t;
    t.rotate(angle);
    applyEdit(mImage.transformed(t));
}

void MainWindow::flipH()
{
    if (mImage.isNull())
        return;
    QTransform t;
    t.translate(mImage.width(), 0).scale(-1, 1);
    applyEdit(mImage.transformed(t));
}

void MainWindow::flipV()
{
    if (mImage.isNull())
        return;
    QTransform t;
    t.translate(0, mImage.height()).scale(1, -1);
    applyEdit(mImage.transformed(t));
}

void MainWindow::toggleCrop()
{
    if (mImage.isNull())
        return;
    if (mView->cropMode())
        applyCrop();
    else
        mView->startCrop();
}

void MainWindow::applyCrop()
{
    const QImage out = mView->applyCrop();
    if (!out.isNull()) {
        mImage = out;
        mDirty = true;
        updateTitle();
        updateStatus();
    }
}

void MainWindow::cancelCrop()
{
    mView->exitCrop();
}

void MainWindow::toggleStraighten(bool horiz)
{
    if (mImage.isNull())
        return;
    if (mView->straightenMode()) {
        double deg = 0.0;
        if (mView->straightenAngle(deg))
            applyStraighten(deg);
    } else {
        mView->startStraighten(horiz);
    }
}

void MainWindow::applyStraighten(double deg)
{
    if (mImage.isNull())
        return;
    QTransform t;
    t.rotate(deg);
    QImage out = mImage.transformed(t, Qt::SmoothTransformation);
    if (out.isNull()) {
        QMessageBox::warning(this, QStringLiteral("KanDu"),
                             QStringLiteral("摆正失败：旋转图片时出错。"));
        return;
    }
    mView->exitStraighten();
    applyEdit(out);
}

void MainWindow::cancelStraighten()
{
    mView->exitStraighten();
}

void MainWindow::togglePerspective()
{
    if (mImage.isNull())
        return;
    if (mView->perspectiveMode())
        applyPerspective();
    else
        mView->startPerspective();
}

void MainWindow::applyPerspective()
{
    const QImage out = mView->applyPerspective();
    if (!out.isNull()) {
        mImage = out;
        mDirty = true;
        updateTitle();
        updateStatus();
    }
}

void MainWindow::cancelPerspective()
{
    mView->exitPerspective();
}

// ------------------------------------------------------------------ 窗口
void MainWindow::toggleTop(bool checked)
{
    mAlwaysOnTop = checked;
    actTop->setChecked(checked);
    mTitleBar->pinButton()->setChecked(checked);
    const HWND hwnd = HWND(winId());
    if (hwnd)
        setTopmost(hwnd, checked);
    updateStatus();
}

void MainWindow::toggleFullscreen(bool checked)
{
    if (checked) {
        mTitleBar->hide();
        statusBar()->hide();
        showFullScreen();
    } else {
        showNormal();
        mTitleBar->show();
        statusBar()->setVisible(mSettings.showStatusBar);
    }
    actFullscreen->setChecked(checked);
}

void MainWindow::toggleStatusBar(bool checked)
{
    mSettings.showStatusBar = checked;
    statusBar()->setVisible(checked);
    actStatusBar->setChecked(checked);
    mSettings.save();
}

void MainWindow::applySettings()
{
    applyTheme();
    statusBar()->setVisible(mSettings.showStatusBar);
    actStatusBar->setChecked(mSettings.showStatusBar);
}

void MainWindow::applyTheme()
{
    const Theme t = themeFor(mSettings.theme);
    mTitleBar->applyTheme(t.titleBg, t.fg, t.hover, t.sep, t.pin, t.pinActive, t.pinCheckedBg);

    QPalette sp = statusBar()->palette();
    sp.setColor(QPalette::Window, t.statusBg);
    sp.setColor(QPalette::WindowText, t.statusFg);
    statusBar()->setPalette(sp);
    statusBar()->setAutoFillBackground(true);

    // 适应窗口按钮配色跟随状态栏
    mBtnFit->setTextColor(t.statusFg);
    mBtnFit->setHoverColor(t.hover);

    // 张数导航箭头配色跟随状态栏
    mNavPrev->setTextColor(t.statusFg);
    mNavPrev->setHoverColor(t.hover);
    mNavNext->setTextColor(t.statusFg);
    mNavNext->setHoverColor(t.hover);

    // 背景色跟随主题默认值；用户手动自定义过的背景色则保留
    const QSet<QString> defaults{"#e8e8ea", "#3a3a3f", "#2b2b2b"};
    if (defaults.contains(mSettings.bgColor))
        mSettings.bgColor = t.viewBg;
    mView->setBackgroundColor(QColor(mSettings.bgColor));
}

void MainWindow::openSettingsDialog()
{
    SettingsDialog dlg(mSettings, this);
    if (dlg.exec() == QDialog::Accepted) {
        mSettings = dlg.settings();
        applySettings();
        updateStatus();
    }
}

// ------------------------------------------------------------------ 其他
void MainWindow::onZoomChanged(double zoom)
{
    if (mImage.isNull()) {
        mLblInfo->clear();
        return;
    }
    QString t = QStringLiteral("%1 × %2").arg(mImage.width()).arg(mImage.height());
    if (!mView->isFit())
        t += QStringLiteral("  %1%").arg(int(qRound(zoom * 100)));
    if (mAlwaysOnTop)
        t += QStringLiteral("  · 置顶");
    mLblInfo->setText(t);
}

void MainWindow::onDoubleClicked()
{
    const QString act = mSettings.doubleClickAction;
    if (act == QLatin1String(DBL_ALWAYS_ON_TOP))
        toggleTop(!mAlwaysOnTop);
    else if (act == QLatin1String("fit"))
        mView->fitToWindow();
    else if (act == QLatin1String("actual"))
        mView->setActualSize();
    else if (act == QLatin1String("fullscreen"))
        toggleFullscreen(!isFullScreen());
}

void MainWindow::showContextMenu(const QPoint &gpos)
{
    QMenu menu(this);
    menu.addAction(actOpen);
    menu.addAction(actSave);
    menu.addAction(actSaveAs);
    menu.addSeparator();
    menu.addAction(actFit);
    menu.addAction(actActual);
    menu.addAction(actZoomIn);
    menu.addAction(actZoomOut);
    menu.addSeparator();
    menu.addAction(actCrop);
    menu.addAction(actStraighten);
    menu.addAction(actPerspective);
    menu.addAction(actRotL);
    menu.addAction(actRotR);
    menu.addAction(actFlipH);
    menu.addAction(actFlipV);
    menu.addSeparator();
    menu.addAction(actPrev);
    menu.addAction(actNext);
    menu.addAction(actTop);
    menu.addAction(actFullscreen);
    menu.addSeparator();
    menu.addAction(actStatusBar);
    menu.addSeparator();
    menu.addAction(actSettings);
    menu.addAction(actHelp);
    menu.exec(gpos);
}

void MainWindow::showHelp()
{
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("快捷键说明 - KanDu"));
    dlg.resize(560, 520);
    auto *layout = new QVBoxLayout(&dlg);
    auto *browser = new QTextBrowser(&dlg);
    browser->setHtml(kHelpText);
    layout->addWidget(browser);
    auto *ok = new QPushButton(QStringLiteral("确定"), &dlg);
    connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
    layout->addWidget(ok, 0, Qt::AlignHCenter);
    dlg.exec();
}

void MainWindow::showAbout()
{
    showAboutDialog(this);
}

bool MainWindow::confirmReplace()
{
    if (!mDirty || mImage.isNull())
        return true;
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("KanDu"));
    box.setText(QStringLiteral("当前图片有未保存的修改，是否保存？"));
    QPushButton *btnSave = box.addButton(QStringLiteral("保存"), QMessageBox::AcceptRole);
    QPushButton *btnDiscard = box.addButton(QStringLiteral("不保存"), QMessageBox::DestructiveRole);
    QPushButton *btnCancel = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.exec();
    const QAbstractButton *clicked = box.clickedButton();
    if (clicked == btnCancel)
        return false;
    if (clicked == btnSave)
        return saveCurrent();
    return true;
}

void MainWindow::updateTitle()
{
    if (mImage.isNull()) {
        setWindowTitle(QStringLiteral("KanDu"));
    } else {
        const QString name = mPath.isEmpty() ? QStringLiteral("剪贴板图片")
                                             : QFileInfo(mPath).fileName();
        const QString mark = mDirty ? QStringLiteral(" ●") : QString();
        setWindowTitle(QStringLiteral("KanDu — %1（%2×%3）%4")
                           .arg(name).arg(mImage.width()).arg(mImage.height()).arg(mark));
    }
    // 标题栏只显示软件名（文件名与尺寸在底部状态栏）
    mTitleBar->setTitle(QStringLiteral("KanDu"));
}

void MainWindow::updateStatus()
{
    if (mImage.isNull()) {
        mLblFile->clear();
        mLblInfo->clear();
        mLblIdx->clear();
        mNavPrev->hide();
        mNavNext->hide();
        return;
    }
    const QString name = mPath.isEmpty() ? QStringLiteral("剪贴板图片")
                                         : QFileInfo(mPath).fileName();
    const QString dirtyMark = mDirty ? QStringLiteral(" ●") : QString();
    const QFontMetrics fm = mLblFile->fontMetrics();
    mLblFile->setText(fm.elidedText(name + dirtyMark, Qt::ElideMiddle, 300));
    // 多张图片才显示张数与翻页箭头
    const bool multi = mFolderFiles.size() > 1;
    if (!mFolderFiles.isEmpty())
        mLblIdx->setText(QStringLiteral("%1 / %2").arg(mFolderIndex + 1).arg(mFolderFiles.size()));
    else
        mLblIdx->clear();
    mNavPrev->setVisible(multi);
    mNavNext->setVisible(multi);
    onZoomChanged(mView->zoom()); // 尺寸 + 缩放百分比 + 置顶标记
}

void MainWindow::refreshActions()
{
    const bool has = !mImage.isNull();
    const QList<QAction *> acts = {actSave, actSaveAs, actCrop, actStraighten, actPerspective,
                                   actRotR, actRotL,
                                   actFlipH, actFlipV, actFit, actActual,
                                   actZoomIn, actZoomOut, actPrev, actNext};
    for (QAction *a : acts)
        a->setEnabled(has);

    mTitleBar->button("rot")->setEnabled(has);
    mTitleBar->button("flip")->setEnabled(has);
    mTitleBar->button("crop")->setEnabled(has);
    mTitleBar->button("straighten")->setEnabled(has);
    mTitleBar->button("persp")->setEnabled(has);
    mTitleBar->button("save")->setEnabled(has);
}

// ------------------------------------------------------------------ 生命周期
void MainWindow::restoreGeometryFromSettings()
{
    const QList<int> g = mSettings.windowGeometry;
    const QRect scr = screen()->availableGeometry();
    if (g.size() == 4 && g[2] >= 480 && g[3] >= 240
        && g[0] < scr.right() - 80 && g[1] < scr.bottom() - 40
        && g[0] + g[2] > scr.left() + 40 && g[1] + g[3] > scr.top() + 40) {
        setGeometry(g[0], g[1], g[2], g[3]);
        return;
    }
    resize(880, 640);
    move((scr.width() - width()) / 2, (scr.height() - height()) / 3);
}

void MainWindow::saveGeometryToSettings()
{
    if (!isMaximized() && !isFullScreen()) {
        const QRect g = geometry();
        mSettings.windowGeometry = {g.x(), g.y(), g.width(), g.height()};
        mSettings.save();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmReplace()) {
        event->ignore();
        return;
    }
    saveGeometryToSettings();
    event->accept();
}

} // namespace kandu
