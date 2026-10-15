#include "aboutdialog.h"

#include <QApplication>
#include <QDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QObject>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

#include "icons.h"

namespace kandu {

namespace {

// 关于页里展示的收款码：设置页缩略显示，点开再放大，避免二维码被压得扫不出来。
const char *const kDonateRes[] = {
    ":/res/donate_wechat.png",
    ":/res/donate_alipay.png",
};

// 收款码在关于页里左右并排直接显示，固定尺寸、不可点击。
// 120px 是实测下限：低于约 110px 时手机上很难稳定识别。
constexpr int kDonatePx = 120;
// 关于页内容宽度：定宽可让 QLabel 换行行数固定，排版可预期
constexpr int kContentWidth = 440;

QString tipStyle()
{
    return QStringLiteral("color: #8a8a8e;");
}

// 细分隔线：代替整块背景框，视觉上分组但不占地方
QFrame *separator(QWidget *owner)
{
    auto *line = new QFrame(owner);
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral(
        "QFrame { border: none; border-top: 1px solid rgba(128,128,132,70); }"));
    line->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    return line;
}

// 按定宽算出自动换行文本所需高度并锁定。
// QLabel 开自动换行后，容器变窄时不会回头重算高度，末行会被裁掉。
int wrapHeight(QLabel *label, int textWidth)
{
    const QFontMetrics fm(label->font());
    return fm.boundingRect(QRect(0, 0, textWidth, 0), Qt::TextWordWrap,
                           label->text()).height();
}

} // namespace

QString donateResource(int index)
{
    const int n = int(sizeof(kDonateRes) / sizeof(kDonateRes[0]));
    return index >= 0 && index < n ? QString::fromLatin1(kDonateRes[index])
                                   : QString();
}

void buildAboutContent(QWidget *owner, QVBoxLayout *lay, bool withDonate)
{
    // 排版结构：头部（图标+标题）→ 一句话简介 → 分隔线
    //   → 蓝色小标题「开源免费软件」+ 正文
    //   → 蓝色小标题「支持开发」+ 正文 + 收款码（居中）
    //   → 分隔线 → 底部联系方式
    // 两个小标题区块结构完全一致，间距对称。
    constexpr int kTextWidth = kContentWidth;

    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(8);

    // ---- 头部：图标 + 标题，版本与日期收进副标题一行 ----
    auto *headRow = new QHBoxLayout;
    headRow->setSpacing(10);
    // 头部整体贴顶：否则 headRow 会在父布局里垂直居中，与下方内容脱节
    headRow->setAlignment(Qt::AlignTop);

    auto *iconLbl = new QLabel(owner);
    iconLbl->setPixmap(appIcon().pixmap(48, 48));
    iconLbl->setFixedSize(48, 48);
    iconLbl->setAlignment(Qt::AlignCenter);
    headRow->addWidget(iconLbl, 0, Qt::AlignTop);

    auto *headText = new QVBoxLayout;
    headText->setSpacing(0);
    headText->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel(QStringLiteral("<b style='font-size:15px'>KanDu</b> 简洁看图软件"), owner);
    headText->addWidget(title);
    // 注意：Qt 富文本不解析 &nbsp; 等 HTML 实体，要用 QtSupportedFormats 里的写法
    auto *meta = new QLabel(QStringLiteral("V1.2　·　2026-10-15"), owner);
    meta->setStyleSheet(tipStyle());
    headText->addWidget(meta);
    // 逐个固定纵向尺寸：否则在高度富余时会被均分拉散
    for (QLabel *l : {title, meta})
        l->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    headRow->addLayout(headText, 1);
    lay->addLayout(headRow);

    // ---- 一句话简介 ----
    auto *desc = new QLabel(
        QStringLiteral("按图片比例贴合显示，滚轮缩放、双击置顶，"
                       "支持裁剪、摆正、透视校正与旋转。"),
        owner);
    desc->setWordWrap(true);
    desc->setStyleSheet(QStringLiteral("color: #5a5a5e;"));
    desc->setFixedWidth(kTextWidth);
    desc->setFixedHeight(wrapHeight(desc, kTextWidth));
    lay->addWidget(desc);

    // ---- 开源免费说明 ----
    lay->addWidget(separator(owner));
    lay->addSpacing(2);

    // 「蓝色小标题 + 灰色正文」成对出现，两个区块结构完全一致
    auto *freeTitle = new QLabel(QStringLiteral("<b>开源免费软件</b>"), owner);
    freeTitle->setStyleSheet(QStringLiteral("color: #2f7fe0;"));
    freeTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    lay->addWidget(freeTitle);

    auto *freeText = new QLabel(
        QStringLiteral("永久免费、完全开源，<b>无广告 · 无内购 · 无捆绑 · 不联网上传</b>。<br>"
                       "所有功能不设限制，欢迎自行编译、修改与二次分发。"),
        owner);
    freeText->setWordWrap(true);
    freeText->setStyleSheet(QStringLiteral("color: #5a5a5e;"));
    freeText->setFixedWidth(kTextWidth);
    freeText->setFixedHeight(wrapHeight(freeText, kTextWidth));
    lay->addWidget(freeText);
    lay->addSpacing(4);

    // ---- 收款码：左右并排居中、固定尺寸、不可点击 ----
    if (withDonate) {
        auto *donateTitle = new QLabel(QStringLiteral("<b>支持开发</b>"), owner);
        donateTitle->setStyleSheet(QStringLiteral("color: #2f7fe0;"));
        donateTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        lay->addWidget(donateTitle);

        auto *donateHint = new QLabel(QStringLiteral("若这个软件帮到了你，欢迎自愿扫码支持（不捐也不影响使用）"), owner);
        donateHint->setStyleSheet(QStringLiteral("color: #5a5a5e;"));
        donateHint->setWordWrap(true);
        donateHint->setFixedWidth(kTextWidth);
        donateHint->setFixedHeight(wrapHeight(donateHint, kTextWidth));
        lay->addWidget(donateHint);
        lay->addSpacing(4);

        auto *qrRow = new QHBoxLayout;
        qrRow->setSpacing(10);
        qrRow->setAlignment(Qt::AlignCenter); // 两个码整体居中
        for (int i = 0; i < 2; ++i) {
            const QPixmap pm(donateResource(i));
            if (pm.isNull())
                continue;
            auto *lbl = new QLabel(owner);
            lbl->setPixmap(pm.scaled(kDonatePx, kDonatePx, Qt::KeepAspectRatio,
                                     Qt::SmoothTransformation));
            lbl->setFixedSize(kDonatePx + 2, kDonatePx + 2);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setToolTip(QStringLiteral("微信 / 支付宝扫码支持"));
            qrRow->addWidget(lbl);
        }
        lay->addLayout(qrRow);
    }

    // ---- 底部：联系方式与版权 ----
    lay->addWidget(separator(owner));

    auto *contact = new QLabel(QStringLiteral("QQ 363899135　·　© KanDu"), owner);
    contact->setAlignment(Qt::AlignCenter);
    contact->setStyleSheet(tipStyle());
    contact->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    lay->addWidget(contact);

    // 末尾留白：窗口比内容高时，把多余高度收在这里，各控件不会被拉开
    lay->addStretch(1);
}

void showAboutDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QStringLiteral("关于 KanDu"));
    dlg.setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto *root = new QVBoxLayout(&dlg);
    // 内容可能比窗口高（低分屏 / 大字体），套滚动区兜底
    auto *scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *page = new QWidget(scroll);
    auto *lay = new QVBoxLayout(page);
    lay->setContentsMargins(16, 14, 16, 14);
    buildAboutContent(page, lay);
    scroll->setWidget(page);
    root->addWidget(scroll);

    auto *ok = new QPushButton(QStringLiteral("关闭"), &dlg);
    ok->setDefault(true);
    root->addWidget(ok, 0, Qt::AlignHCenter);
    QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);

    // 按内容实际高度开窗，避免下半截留一大片空白。
    // 先定宽再让布局算出高度；上限 80% 屏高，超出仍可滚动。
    dlg.resize(kContentWidth + 72, 400);
    const int h = page->sizeHint().height() + root->contentsMargins().top()
                + root->contentsMargins().bottom() + ok->sizeHint().height()
                + root->spacing() + 24;
    const int maxH = parent ? parent->screen()->availableGeometry().height() * 4 / 5
                            : QApplication::primaryScreen()->availableGeometry().height() * 4 / 5;
    dlg.resize(kContentWidth + 72, qBound(360, h, maxH));
    dlg.exec();
}

} // namespace kandu
