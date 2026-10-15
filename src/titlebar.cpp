#include "titlebar.h"

#include <QHBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QPushButton>
#include <QLabel>
#include <utility>

#include "icons.h"

namespace kandu {

namespace {
struct ActionDef {
    const char *kind;
    const char *tip;
};
const ActionDef kActions[] = {
    {"rot", "旋转：左键右旋 · 右键左旋 (R / Shift+R)"},
    {"flip", "镜像：左键水平 · 右键垂直 (H / V)"},
    {"crop", "裁剪 (C，Enter 应用)"},
    {"straighten", "摆正：左键=水平 · 右键=垂直 (S)"},
    {"persp", "透视校准：拖动四角调整透视 (P，Enter 应用)"},
    {"save", "覆盖保存 (Ctrl+S)"},
    {"settings", "设置"},
};
} // namespace

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("KanDuTitleBar");
    setFixedHeight(34);
    setAutoFillBackground(true);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 2, 4, 2);
    lay->setSpacing(3);

    auto *icon = new QLabel(this);
    icon->setPixmap(appIcon().pixmap(16, 16));
    lay->addWidget(icon);

    mLblTitle = new QLabel(QStringLiteral("KanDu"), this);
    lay->addWidget(mLblTitle);

    lay->addStretch(1);

    // 图标快捷按钮（按钮间加浅色分割线）
    for (int i = 0; i < int(sizeof(kActions) / sizeof(kActions[0])); ++i) {
        if (i)
            lay->addWidget(vline());
        FlatButton *b = makeActionButton(kActions[i].kind,
                                         QString::fromUtf8(kActions[i].tip));
        lay->addWidget(b);
        mIconButtons.insert(QString::fromUtf8(kActions[i].kind), b);
    }

    // 置顶（图钉）
    mBtnPin = makeWinButton(QString(), "pinTop");
    mBtnPin->setCheckable(true);
    mBtnPin->setIconSize(QSize(18, 18));
    mBtnPin->setToolTip(QStringLiteral("窗口置顶 (T)"));
    lay->addWidget(mBtnPin);

    // 图钉图标随选中状态切换（颜色由 applyTheme 提供）
    connect(mBtnPin, &QPushButton::toggled, this, [this](bool checked) {
        mBtnPin->setIcon(pinIcon(checked ? mPinActive : mPinNormal));
    });

    mBtnMin = makeWinButton(QStringLiteral("─"), "winCtrl");
    mBtnMax = makeWinButton(QStringLiteral("□"), "winCtrl");
    mBtnClose = makeWinButton(QStringLiteral("✕"), "winClose");
    mBtnClose->setDangerHover(true);
    lay->addWidget(mBtnMin);
    lay->addWidget(mBtnMax);
    lay->addWidget(mBtnClose);

    connect(mBtnMin, &QPushButton::clicked, this, &TitleBar::minimizeRequested);
    connect(mBtnMax, &QPushButton::clicked, this, &TitleBar::maximizeRequested);
    connect(mBtnClose, &QPushButton::clicked, this, &TitleBar::closeRequested);
}

FlatButton *TitleBar::makeActionButton(const QString &kind, const QString &tip)
{
    auto *b = new FlatButton(this);
    b->setObjectName("toolBtn");
    b->setFixedSize(30, 28);
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setIcon(gearIcon(mFg)); // 占位，applyTheme 会重刷
    return b;
}

QFrame *TitleBar::vline()
{
    auto *line = new QFrame(this);
    line->setObjectName("btnSep");
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedSize(1, 14);
    line->setAutoFillBackground(true);
    return line;
}

FlatButton *TitleBar::makeWinButton(const QString &glyph, const QString &objName)
{
    auto *b = new FlatButton(this);
    b->setObjectName(objName);
    b->setFixedSize(30, 28);
    b->setPaintText(true);
    if (!glyph.isEmpty())
        b->setText(glyph);
    return b;
}

void TitleBar::setMaximized(bool maximized)
{
    mBtnMax->setText(maximized ? QStringLiteral("❐") : QStringLiteral("□"));
}

void TitleBar::applyTheme(const QColor &bg, const QColor &fg, const QColor &hover,
                          const QColor &sep, const QColor &pinNormal, const QColor &pinActive,
                          const QColor &pinCheckedBg)
{
    mPinNormal = pinNormal;
    mPinActive = pinActive;
    mPinCheckedBg = pinCheckedBg;
    mHover = hover;
    mFg = fg;

    // 标题栏背景 + 文字
    QPalette pal = palette();
    pal.setColor(QPalette::Window, bg);
    pal.setColor(QPalette::WindowText, fg);
    setPalette(pal);

    // 按钮
    for (FlatButton *b : std::as_const(mIconButtons)) {
        b->setHoverColor(hover);
        b->setTextColor(fg);
    }
    mBtnPin->setHoverColor(hover);
    mBtnPin->setCheckedColor(pinCheckedBg);
    mBtnMin->setHoverColor(hover);
    mBtnMin->setTextColor(fg);
    mBtnMax->setHoverColor(hover);
    mBtnMax->setTextColor(fg);
    mBtnClose->setHoverColor(hover);
    mBtnClose->setTextColor(fg);

    // 分割线
    for (QFrame *f : findChildren<QFrame *>(QStringLiteral("btnSep"))) {
        QPalette lp;
        lp.setColor(QPalette::Window, sep);
        f->setPalette(lp);
    }

    // 按钮图标（随主题前景色重绘）
    mIconButtons["rot"]->setIcon(ringArrow(fg, true));
    mIconButtons["flip"]->setIcon(flipHIcon(fg));
    mIconButtons["crop"]->setIcon(cropIcon(fg));
    mIconButtons["straighten"]->setIcon(straightenIcon(fg));
    mIconButtons["persp"]->setIcon(perspectiveIcon(fg));
    mIconButtons["save"]->setIcon(saveIcon(fg));
    mIconButtons["settings"]->setIcon(gearIcon(fg));

    // 图钉图标（普通/激活双态；颜色随主题）
    mBtnPin->setIcon(mBtnPin->isChecked() ? pinIcon(mPinActive) : pinIcon(mPinNormal));
}

// ------------------------------------------------------------ 窗口交互
void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        mDragging = true;
        QWidget *win = window();
        mDragOffset = event->globalPosition().toPoint() - win->frameGeometry().topLeft();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (mDragging && (event->buttons() & Qt::LeftButton)) {
        QWidget *win = window();
        if (win->isMaximized())
            return;
        win->move(event->globalPosition().toPoint() - mDragOffset);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        mDragging = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeRequested();
}

} // namespace kandu
