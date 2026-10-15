#pragma once
// 轻量自绘扁平按钮：避免 QSS（Qt 6 的 QStyleSheetStyle 全树样式计算严重拖慢首屏）。
// 图标按钮 / 窗口控制按钮 / 图钉 共用；hover 与 checked 由颜色控制，无任何 QSS。
#include <QPushButton>
#include <QColor>
#include <QEnterEvent>
#include <QMouseEvent>
#include <QPainter>

namespace kandu {

class FlatButton : public QPushButton
{
public:
    explicit FlatButton(QWidget *parent = nullptr)
        : QPushButton(parent)
    {
        setFlat(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    void setHoverColor(const QColor &c) { mHover = c; }
    void setCheckedColor(const QColor &c) { mChecked = c; }
    void setTextColor(const QColor &c) { mFg = c; update(); }
    void setDangerHover(bool on) { mDanger = on; }
    void setPaintText(bool on) { mPaintText = on; }

protected:
    void enterEvent(QEnterEvent *) override { mHovered = true; update(); }
    void leaveEvent(QEvent *) override { mHovered = false; mPressed = false; update(); }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton)
            mPressed = true;
        QPushButton::mousePressEvent(e);
        update();
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        mPressed = false;
        QPushButton::mouseReleaseEvent(e);
        update();
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QColor bg;
        if (isChecked() && mChecked.isValid())
            bg = mChecked;
        else if (mPressed && mHover.isValid())
            bg = mHover.darker(112);
        else if (mHovered && mHover.isValid())
            bg = mHover;
        if (bg.isValid()) {
            p.setPen(Qt::NoPen);
            p.setBrush(bg);
            p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);
        }

        if (!icon().isNull()) {
            const QSize sz = iconSize();
            const QPixmap pm = icon().pixmap(sz);
            p.drawPixmap((width() - sz.width()) / 2, (height() - sz.height()) / 2, pm);
        } else if (mPaintText) {
            QColor c = mFg;
            if (mDanger && mHovered)
                c = QColor("#ffffff");
            p.setPen(c);
            p.setFont(font());
            p.drawText(rect(), Qt::AlignCenter, text());
        }
    }

private:
    QColor mHover;
    QColor mChecked;
    QColor mFg{QColor("#3a3a3c")};
    bool mDanger = false;
    bool mPaintText = false;
    bool mHovered = false;
    bool mPressed = false;
};

} // namespace kandu
