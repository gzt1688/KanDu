#include "settingsdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QColorDialog>
#include <QMessageBox>
#include <QFrame>
#include <QScreen>
#include <QScrollArea>
#include <QTabBar>

#include "aboutdialog.h"
#include "defaultapp.h"

namespace kandu {

namespace {

// QComboBox 宽度贴合内容：QFormLayout 默认把输入控件拉满整行，看着松散。
// 关键是 FieldsStayAtSizeHint —— 它让控件保持 sizeHint 宽度，
// 而 AdjustToContents 又让 sizeHint 跟随最长项，两者配合即为「按内容自适应」。
// 这里只兜一个下限，防止短文本（如「浅色」）被压到箭头挤字。
void fitToContents(QComboBox *box)
{
    const QFontMetrics fm(box->font());
    int textW = 0;
    for (int i = 0; i < box->count(); ++i)
        textW = qMax(textW, fm.horizontalAdvance(box->itemText(i)));
    box->setMinimumWidth(textW + 48); // 48 = 箭头区 + 内边距 + 边框，实测不截字
    box->setMaximumWidth(box->minimumWidth()); // 同时禁止被布局拉宽
}

} // namespace

SettingsDialog::SettingsDialog(const Settings &settings, QWidget *parent)
    : QDialog(parent), mResult(settings)
{
    setWindowTitle(QStringLiteral("设置 - KanDu"));
    setMinimumWidth(440);

    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);

    // 两页：通用（鼠标 + 显示合并）、关于
    QWidget *genPage = new QWidget(tabs);
    QWidget *aboutPage = new QWidget(tabs);
    buildMousePage();
    buildAboutPage(aboutPage);

    auto *genLay = new QVBoxLayout(genPage);
    genLay->setSpacing(10);

    // 上下两块各用一个 QGroupBox 框起来，视觉上分组清晰
    auto *mouseBox = new QGroupBox(QStringLiteral("鼠标与滚轮"), genPage);
    auto *mouseForm = new QFormLayout(mouseBox);
    mouseForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint); // 输入框按内容宽度，不拉满
    mouseForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);      // 标签左对齐成一列
    mouseForm->addRow(QStringLiteral("滚轮方向反置"), mWheelInvert);
    mouseForm->addRow(QStringLiteral("滚轮缩放步进"), mWheelStep);
    mouseForm->addRow(QStringLiteral("双击图片动作"), mDoubleClick);
    mouseForm->addRow(QStringLiteral("左键拖拽平移"), mDragPan);
    genLay->addWidget(mouseBox);

    auto *dispBox = new QGroupBox(QStringLiteral("显示"), genPage);
    dispBox->setLayout(buildDisplayPage());
    genLay->addWidget(dispBox);
    genLay->addStretch(1);

    tabs->addTab(genPage, QStringLiteral("通用"));
    tabs->addTab(aboutPage, QStringLiteral("关于"));
    root->addWidget(tabs);

    // 底部按钮：左「恢复默认」+「设置为默认看图软件」，右「取消」「确定」
    auto *btns = new QHBoxLayout;
    auto *btnDefaults = new QPushButton(QStringLiteral("恢复默认"), this);
    auto *setDefault = new QPushButton(QStringLiteral("设置为默认看图软件"), this);
    setDefault->setToolTip(QStringLiteral("注册当前用户的文件关联（无需管理员）"));
    auto *btnCancel = new QPushButton(QStringLiteral("取消"), this);
    auto *btnOk = new QPushButton(QStringLiteral("确定"), this);
    btnOk->setDefault(true);
    btns->addWidget(btnDefaults);
    btns->addWidget(setDefault);
    btns->addStretch(1);
    btns->addWidget(btnCancel);
    btns->addWidget(btnOk);
    root->addLayout(btns);

    connect(btnDefaults, &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);
    connect(setDefault, &QPushButton::clicked, this, [this] {
        const auto [ok, msg] = registerDefaultViewer();
        if (ok)
            QMessageBox::information(this, QStringLiteral("KanDu"),
                                     QStringLiteral("已注册为默认看图软件：\n%1").arg(msg));
        else
            QMessageBox::warning(this, QStringLiteral("KanDu"),
                                 QStringLiteral("注册失败：\n%1").arg(msg));
    });
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnOk, &QPushButton::clicked, this, [this] {
        applyToSettings();
        accept();
    });

    // 高度按两页中较高的那页自适应：取各页 sizeHint 的最大值，
    // 避免固定高度在短页面下方留一大片空白。上限 85% 屏高，再高则页内滚动。
    for (QWidget *p : {genPage, aboutPage})
        p->adjustSize();
    tabs->setCurrentIndex(1); // 先切到较高的一页让布局算出准确高度
    tabs->setCurrentIndex(0);
    const int pageH = qMax(genPage->sizeHint().height(),
                           aboutPage->sizeHint().height());
    const int maxH = screen()->availableGeometry().height() * 85 / 100;
    const int winH = pageH + tabs->tabBar()->sizeHint().height()
                   + root->contentsMargins().top() + root->contentsMargins().bottom()
                   + btns->sizeHint().height() + root->spacing() * 2 + 12;
    resize(560, qBound(300, winH, maxH));
}

void SettingsDialog::buildMousePage()
{
    mWheelInvert = new QCheckBox(this);
    mWheelInvert->setChecked(mResult.wheelInvert);
    mWheelInvert->setToolTip(QStringLiteral("勾选后向上滚动为缩小、向下滚动为放大"));

    mWheelStep = new QDoubleSpinBox(this);
    mWheelStep->setRange(1.05, 2.0);
    mWheelStep->setSingleStep(0.05);
    mWheelStep->setDecimals(2);
    mWheelStep->setValue(mResult.wheelStep);
    mWheelStep->setToolTip(QStringLiteral("每次滚动的缩放倍率"));
    // QDoubleSpinBox 默认 sizeHint 很宽（整行），按内容收窄。
    // 箭头按钮宽度没有对应的 QStyle::PixelMetric（Qt6 已移除 PM_SpinBoxUpDownButtons），
    // 这里用一个足够显示「0.00」+ 箭头的保守宽度，避免文字被截断。
    mWheelStep->setFixedWidth(120);

    mDoubleClick = new QComboBox(this);
    mDoubleClick->addItem(QStringLiteral("切换窗口置顶"), QStringLiteral("always_on_top"));
    mDoubleClick->addItem(QStringLiteral("适应窗口"), QStringLiteral("fit"));
    mDoubleClick->addItem(QStringLiteral("实际大小"), QStringLiteral("actual"));
    mDoubleClick->addItem(QStringLiteral("全屏"), QStringLiteral("fullscreen"));
    const int idx = mDoubleClick->findData(mResult.doubleClickAction);
    mDoubleClick->setCurrentIndex(idx < 0 ? 0 : idx);
    mDoubleClick->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    fitToContents(mDoubleClick);

    mDragPan = new QCheckBox(this);
    mDragPan->setChecked(mResult.dragPan);
    mDragPan->setToolTip(QStringLiteral("勾选后按住左键拖拽可平移图片（放大时）"));

    // 显示组控件（供「通用」页的显示区使用）
    mTheme = new QComboBox(this);
    mTheme->addItem(QStringLiteral("浅色"), QStringLiteral("light"));
    mTheme->addItem(QStringLiteral("深色"), QStringLiteral("dark"));
    const int ti = mTheme->findData(mResult.theme);
    mTheme->setCurrentIndex(ti < 0 ? 0 : ti);
    mTheme->setSizeAdjustPolicy(QComboBox::AdjustToContents); // 宽度跟随最长项
    fitToContents(mTheme);

    mFitOnOpen = new QCheckBox(this);
    mFitOnOpen->setChecked(mResult.fitOnOpen);
    mShowStatusBar = new QCheckBox(this);
    mShowStatusBar->setChecked(mResult.showStatusBar);
    mConfirmOverwrite = new QCheckBox(this);
    mConfirmOverwrite->setChecked(mResult.confirmOverwrite);
}

QFormLayout *SettingsDialog::buildDisplayPage()
{
    auto *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->addRow(QStringLiteral("主题"), mTheme);
    form->addRow(QStringLiteral("打开后窗口贴合图片"), mFitOnOpen);
    form->addRow(QStringLiteral("显示状态栏"), mShowStatusBar);
    form->addRow(QStringLiteral("覆盖保存前询问"), mConfirmOverwrite);
    form->addRow(QStringLiteral("图片背景色"), buildBgColorRow());
    return form;
}

// 背景色一行：只读输入框 + 「选择…」按钮，整体随内容宽度
QWidget *SettingsDialog::buildBgColorRow()
{
    mBgColor = new QLineEdit(this);
    mBgColor->setText(mResult.bgColor);
    mBgColor->setReadOnly(true);
    mBgColor->setToolTip(QStringLiteral("点击右侧按钮选择图片周围背景色"));
    mBgColor->setFixedWidth(96); // 色值很短，固定窄宽比拉满一行更整齐

    auto *pick = new QPushButton(QStringLiteral("选择…"), this);
    pick->setToolTip(QStringLiteral("选择图片周围背景色"));
    connect(pick, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(QColor(mBgColor->text()), this,
                                                QStringLiteral("选择背景色"));
        if (c.isValid()) {
            mBgColor->setText(c.name());
            mCustomBg = c.name();
        }
    });

    auto *row = new QWidget(this);
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    lay->addWidget(mBgColor);
    lay->addWidget(pick);
    lay->addStretch(1);
    return row;
}

void SettingsDialog::buildAboutPage(QWidget *page)
{
    auto *lay = new QVBoxLayout(page);
    // 内容较长（图标 + 说明 + 两个收款码），套滚动区，低分屏也不会被裁掉
    auto *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *inner = new QWidget(scroll);
    auto *innerLay = new QVBoxLayout(inner);
    innerLay->setContentsMargins(14, 14, 14, 14);
    innerLay->setAlignment(Qt::AlignHCenter);
    buildAboutContent(inner, innerLay);
    innerLay->addStretch(1);
    scroll->setWidget(inner);
    lay->addWidget(scroll);
}

void SettingsDialog::applyToSettings()
{
    mResult.wheelInvert = mWheelInvert->isChecked();
    mResult.wheelStep = mWheelStep->value();
    mResult.doubleClickAction = mDoubleClick->currentData().toString();
    mResult.dragPan = mDragPan->isChecked();

    mResult.theme = mTheme->currentData().toString();
    mResult.fitOnOpen = mFitOnOpen->isChecked();
    mResult.showStatusBar = mShowStatusBar->isChecked();
    mResult.confirmOverwrite = mConfirmOverwrite->isChecked();
    if (!mCustomBg.isEmpty())
        mResult.bgColor = mCustomBg;
    mResult.save();
}

void SettingsDialog::restoreDefaults()
{
    const Settings def;
    mResult = def;
    mWheelInvert->setChecked(def.wheelInvert);
    mWheelStep->setValue(def.wheelStep);
    const int idx = mDoubleClick->findData(def.doubleClickAction);
    mDoubleClick->setCurrentIndex(idx < 0 ? 0 : idx);
    mDragPan->setChecked(def.dragPan);
    const int ti = mTheme->findData(def.theme);
    mTheme->setCurrentIndex(ti < 0 ? 0 : ti);
    mFitOnOpen->setChecked(def.fitOnOpen);
    mShowStatusBar->setChecked(def.showStatusBar);
    mConfirmOverwrite->setChecked(def.confirmOverwrite);
    mBgColor->setText(def.bgColor);
    mCustomBg.clear();
}

} // namespace kandu
