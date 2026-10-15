#include "icons.h"

#include <QtMath>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QPointF>
#include <QRectF>
#include <utility>

namespace kandu {

static QIcon renderIcon(double size, const std::function<void(QPainter &, double)> &fn)
{
    QPixmap pm(qRound(size), qRound(size));
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    fn(p, size);
    p.end();
    return QIcon(pm);
}

QIcon ringArrow(const QColor &c, bool clockwise, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        QRectF rect(s * 0.22, s * 0.22, s * 0.56, s * 0.56);
        const double start = clockwise ? 35.0 : 145.0;
        const double span  = clockwise ? -270.0 : 270.0;

        QPainterPath path;
        path.arcMoveTo(rect, start);
        path.arcTo(rect, start, span);
        QPen pen(c, s * 0.085, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);

        // 端部大箭头（约 2 倍大）
        const double endAng = qDegreesToRadians(start + span);
        const QPointF center = rect.center();
        const double r = rect.width() / 2.0;
        const double ex = center.x() + r * qCos(endAng);
        const double ey = center.y() - r * qSin(endAng);
        const double sign = span > 0 ? 1.0 : -1.0;
        double tx = -sign * qSin(endAng);
        double ty = -sign * qCos(endAng);
        const double n = qSqrt(tx * tx + ty * ty);
        tx /= n; ty /= n;

        const double len = s * 0.52;
        const double bx = ex - tx * len;
        const double by = ey - ty * len;
        const double px = -ty, py = tx;
        const double w = len * 0.46;

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        QPolygonF tri;
        tri << QPointF(ex, ey)
            << QPointF(bx + px * w, by + py * w)
            << QPointF(bx - px * w, by - py * w);
        p.drawPolygon(tri);
    });
}

QIcon flipHIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        p.setPen(QPen(c, s * 0.09, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(s * 0.50, s * 0.14), QPointF(s * 0.50, s * 0.86));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        QPolygonF l, r;
        // 三角放大：顶点更靠外、更宽
        l << QPointF(s * 0.08, s * 0.28) << QPointF(s * 0.46, s * 0.50) << QPointF(s * 0.08, s * 0.72);
        r << QPointF(s * 0.92, s * 0.28) << QPointF(s * 0.54, s * 0.50) << QPointF(s * 0.92, s * 0.72);
        p.drawPolygon(l);
        p.drawPolygon(r);
    });
}

QIcon flipVIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        p.setPen(QPen(c, s * 0.09, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(s * 0.14, s * 0.50), QPointF(s * 0.86, s * 0.50));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        QPolygonF t, b;
        t << QPointF(s * 0.28, s * 0.08) << QPointF(s * 0.50, s * 0.46) << QPointF(s * 0.72, s * 0.08);
        b << QPointF(s * 0.28, s * 0.92) << QPointF(s * 0.50, s * 0.54) << QPointF(s * 0.72, s * 0.92);
        p.drawPolygon(t);
        p.drawPolygon(b);
    });
}

QIcon cropIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        // 对角 L 形裁剪标记（左上 + 右下），非闭合矩形
        QPen pen(c, s * 0.09, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const double arm = s * 0.32;     // 臂长
        const double inset = s * 0.20;   // 角位置
        // 左上 L
        p.drawLine(QPointF(inset, inset), QPointF(inset + arm, inset));
        p.drawLine(QPointF(inset, inset), QPointF(inset, inset + arm));
        // 右下 L
        p.drawLine(QPointF(s - inset, s - inset), QPointF(s - inset - arm, s - inset));
        p.drawLine(QPointF(s - inset, s - inset), QPointF(s - inset, s - inset - arm));
    });
}

QIcon straightenIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        QPen base(c, s * 0.055, Qt::DashLine, Qt::RoundCap);
        p.setPen(base);
        p.drawLine(QPointF(s * 0.14, s * 0.50), QPointF(s * 0.86, s * 0.50));
        p.setPen(QPen(c, s * 0.09, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(s * 0.22, s * 0.66), QPointF(s * 0.78, s * 0.34));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(s * 0.22, s * 0.66), s * 0.07, s * 0.07);
        p.drawEllipse(QPointF(s * 0.78, s * 0.34), s * 0.07, s * 0.07);
    });
}

QIcon perspectiveIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        // 透视梯形（顶边窄、底边宽），内部透视网格线 + 四角圆点
        QPolygonF q;
        q << QPointF(s * 0.26, s * 0.24) << QPointF(s * 0.74, s * 0.24)
          << QPointF(s * 0.88, s * 0.84) << QPointF(s * 0.12, s * 0.84);
        QPen pen(c, s * 0.07, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(q);
        // 内部网格：两条水平线（沿左右边插值）+ 一条垂直中线
        for (int i = 1; i <= 2; ++i) {
            const double t = i / 3.0;
            p.drawLine(q.at(3) * (1 - t) + q.at(0) * t,
                       q.at(2) * (1 - t) + q.at(1) * t);
        }
        p.drawLine(q.at(0) * 0.5 + q.at(1) * 0.5,
                   q.at(3) * 0.5 + q.at(2) * 0.5);
        // 四角点
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (const QPointF &pt : std::as_const(q))
            p.drawEllipse(pt, s * 0.055, s * 0.055);
    });
}

QIcon saveIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        p.setPen(QPen(c, s * 0.09, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(QRectF(s * 0.15, s * 0.15, s * 0.70, s * 0.70), s * 0.09, s * 0.09);
        p.drawRect(QRectF(s * 0.28, s * 0.26, s * 0.30, s * 0.18));
        p.drawRect(QRectF(s * 0.35, s * 0.66, s * 0.30, s * 0.13));
    });
}

QIcon gearIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        QPainterPath ring;
        ring.addEllipse(QPointF(s * 0.5, s * 0.5), s * 0.28, s * 0.28);
        ring.addEllipse(QPointF(s * 0.5, s * 0.5), s * 0.12, s * 0.12);
        p.drawPath(ring);
        for (int i = 0; i < 6; ++i) {
            const double ang = qDegreesToRadians(i * 60.0);
            const double x = s * (0.5 + 0.36 * qCos(ang));
            const double y = s * (0.5 - 0.36 * qSin(ang));
            p.save();
            p.translate(x, y);
            p.rotate(i * 60.0);
            p.drawRoundedRect(QRectF(-s * 0.07, -s * 0.12, s * 0.14, s * 0.24),
                               s * 0.03, s * 0.03);
            p.restore();
        }
    });
}

QIcon pinIcon(const QColor &c, double size)
{
    return renderIcon(size, [&](QPainter &p, double s) {
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(s * 0.50, s * 0.26), s * 0.23, s * 0.10);
        QPainterPath needle;
        needle.moveTo(s * 0.46, s * 0.38);
        needle.lineTo(s * 0.54, s * 0.38);
        needle.lineTo(s * 0.50, s * 0.94);
        needle.closeSubpath();
        p.drawPath(needle);
    });
}

QIcon appIcon()
{
    return renderIcon(256.0, [](QPainter &p, double s) {
        const double m = s * 0.07;
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3B82F6"));
        p.drawRoundedRect(QRectF(m, m, s - 2 * m, s - 2 * m), s * 0.16, s * 0.16);

        p.setPen(QPen(QColor("#FFFFFF"), s * 0.085, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawLine(QPointF(s * 0.20, s * 0.26), QPointF(s * 0.20, s * 0.74));
        p.drawLine(QPointF(s * 0.20, s * 0.50), QPointF(s * 0.46, s * 0.28));
        p.drawLine(QPointF(s * 0.20, s * 0.50), QPointF(s * 0.46, s * 0.72));

        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#FFFFFF"));
        p.drawEllipse(QPointF(s * 0.64, s * 0.46), s * 0.052, s * 0.052);
        p.drawEllipse(QPointF(s * 0.77, s * 0.46), s * 0.052, s * 0.052);
    });
}

} // namespace kandu
