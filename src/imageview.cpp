#include "imageview.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QFontMetrics>
#include <QtMath>
#include <QTransform>
#include <QPolygonF>
#include <cmath>

namespace kandu {

// 单应变换下，对源图按浮点坐标双线性采样（ARGB32）
namespace {
QRgb bilinearSample(const QImage &src, qreal sx, qreal sy)
{
    const int sw = src.width(), sh = src.height();
    // 采样点落在原图之外（角点可拖出图片范围）→ 返回透明，避免把边缘像素拉成色条
    if (sx < 0.0 || sy < 0.0 || sx > sw - 1.0 || sy > sh - 1.0)
        return qRgba(0, 0, 0, 0);
    int x0 = int(sx), y0 = int(sy);
    if (x0 < 0) x0 = 0; else if (x0 > sw - 1) x0 = sw - 1;
    if (y0 < 0) y0 = 0; else if (y0 > sh - 1) y0 = sh - 1;
    int x1 = x0 + 1; if (x1 > sw - 1) x1 = sw - 1;
    int y1 = y0 + 1; if (y1 > sh - 1) y1 = sh - 1;
    const qreal fx = sx - x0, fy = sy - y0;
    const QRgb *l0 = reinterpret_cast<const QRgb *>(src.constScanLine(y0));
    const QRgb *l1 = reinterpret_cast<const QRgb *>(src.constScanLine(y1));
    const QRgb c00 = l0[x0], c01 = l0[x1], c10 = l1[x0], c11 = l1[x1];
    auto blend = [&](int a, int b, int c, int d) -> int {
        const qreal top = a * (1.0 - fx) + b * fx;
        const qreal bot = c * (1.0 - fx) + d * fx;
        return int(top * (1.0 - fy) + bot * fy + 0.5);
    };
    return qRgba(blend(qRed(c00), qRed(c01), qRed(c10), qRed(c11)),
                 blend(qGreen(c00), qGreen(c01), qGreen(c10), qGreen(c11)),
                 blend(qBlue(c00), qBlue(c01), qBlue(c10), qBlue(c11)),
                 blend(qAlpha(c00), qAlpha(c01), qAlpha(c10), qAlpha(c11)));
}

// 把源四边形透视变形铺满目标矩形：对目标矩形逐像素逆变换采样。
// 不做正向整图变换——四边形外的区域在单应下会映射到近无穷远而溢出。
// 逆变换把目标矩形映射回源四边形；若角点超出原图，越界处按透明采样。
QImage warpQuadToRect(const QImage &src, const QPolygonF &srcQuad,
                      const QRectF &dstRect)
{
    if (src.isNull() || srcQuad.size() != 4 || dstRect.isEmpty())
        return QImage();
    QPolygonF dst;
    dst << dstRect.topLeft() << dstRect.topRight()
        << dstRect.bottomRight() << dstRect.bottomLeft();
    QTransform t;
    if (!QTransform::quadToQuad(srcQuad, dst, t))
        return QImage();   // 四点退化（重合/共线/自交）
    bool ok = false;
    const QTransform inv = t.inverted(&ok);
    if (!ok)
        return QImage();

    const int oW = qMax(1, int(qRound(dstRect.width())));
    const int oH = qMax(1, int(qRound(dstRect.height())));
    const QImage srcArgb = src.convertToFormat(QImage::Format_ARGB32);
    QImage out(oW, oH, QImage::Format_ARGB32);
    // Qt 坐标约定：x'=(m11x+m21y+m31)/(m13x+m23y+m33)，y' 同理
    const double a11 = inv.m11(), a12 = inv.m12(), a13 = inv.m13();
    const double a21 = inv.m21(), a22 = inv.m22(), a23 = inv.m23();
    const double a31 = inv.m31(), a32 = inv.m32(), a33 = inv.m33();
    const double x0 = dstRect.left(), y0 = dstRect.top();
    for (int y = 0; y < oH; ++y) {
        QRgb *oline = reinterpret_cast<QRgb *>(out.scanLine(y));
        const double sy = y0 + y;
        double d = a13 * x0 + a23 * sy + a33;
        double nx = a11 * x0 + a21 * sy + a31;
        double ny = a12 * x0 + a22 * sy + a32;
        for (int x = 0; x < oW; ++x) {
            oline[x] = (qAbs(d) > 1e-9) ? bilinearSample(srcArgb, nx / d, ny / d)
                                        : qRgba(0, 0, 0, 0);
            nx += a11; ny += a12; d += a13;
        }
    }
    return out;
}
} // namespace

ImageView::ImageView(const Settings &settings, QWidget *parent)
    : QWidget(parent), mSettings(settings), mBg(QColor(settings.bgColor))
{
    setAcceptDrops(true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

// ------------------------------------------------------------------ 图片
void ImageView::refreshPixmap()
{
    mPixmap = QPixmap::fromImage(mImage);
}

void ImageView::setImage(const QImage &img, bool preserveView)
{
    mImage = img;
    refreshPixmap();
    if (preserveView && !mImage.isNull()) {
        // 保留当前视图状态（缩放与平移），仅兜底非法值
        if (mZoom <= 0.0)
            mZoom = 1.0;
    } else {
        mFit = true;
        mZoom = mImage.isNull() ? 1.0 : fitScale();
        mOffset = QPointF(0, 0);
    }
    exitCrop();
    exitStraighten();
    exitPerspective();
    emit fitChanged(mFit);
    emit zoomChanged(mZoom);
    update();
}

void ImageView::setBackgroundColor(const QColor &color)
{
    mBg = color;
    update();
}

// ------------------------------------------------------------------ 缩放
double ImageView::fitScale() const
{
    if (mImage.isNull())
        return 1.0;
    const double w = qMax(1, width());
    const double h = qMax(1, height());
    return qMin(w / double(mImage.width()), h / double(mImage.height()));
}

void ImageView::fitToWindow()
{
    if (mImage.isNull())
        return;
    mFit = true;
    mZoom = fitScale();
    mOffset = QPointF(0, 0);
    emit fitChanged(true);
    emit zoomChanged(mZoom);
    update();
}

void ImageView::setActualSize()
{
    if (mImage.isNull())
        return;
    mFit = false;
    mZoom = 1.0;
    mOffset = QPointF(0, 0);
    clampOffset();
    emit fitChanged(false);
    emit zoomChanged(1.0);
    update();
}

void ImageView::zoomIn()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), mSettings.wheelStep);
}

void ImageView::zoomOut()
{
    zoomAt(QPointF(width() / 2.0, height() / 2.0), 1.0 / mSettings.wheelStep);
}

void ImageView::zoomAt(const QPointF &pos, double factor)
{
    if (mImage.isNull())
        return;
    if (mFit) {
        mFit = false;
        mZoom = fitScale();
        mOffset = QPointF(0, 0);
        emit fitChanged(false);
    }

    const double old = mZoom;
    double neu = qMin(kMaxZoom, qMax(kMinZoom, old * factor));
    if (qAbs(neu - old) < 1e-9)
        return;

    const QPointF center(width() / 2.0, height() / 2.0);
    const QSizeF disp(mImage.width() * old, mImage.height() * old);
    const QPointF imgPt =
        (pos - center - mOffset + QPointF(disp.width() / 2.0, disp.height() / 2.0)) / old;

    mZoom = neu;
    const QSizeF nd(mImage.width() * neu, mImage.height() * neu);
    mOffset = pos - center - imgPt * neu + QPointF(nd.width() / 2.0, nd.height() / 2.0);
    clampOffset();
    emit zoomChanged(mZoom);
    update();
}

void ImageView::clampOffset()
{
    if (mImage.isNull())
        return;
    const QSizeF disp(mImage.width() * mZoom, mImage.height() * mZoom);
    const double vw = double(width()), vh = double(height());
    const double margin = 40.0;

    double left = vw / 2.0 + mOffset.x() - disp.width() / 2.0;
    if (disp.width() > vw)
        left = qMin(qMax(left, margin - disp.width()), vw - margin);
    else
        left = (vw - disp.width()) / 2.0;
    mOffset.setX(left - vw / 2.0 + disp.width() / 2.0);

    double top = vh / 2.0 + mOffset.y() - disp.height() / 2.0;
    if (disp.height() > vh)
        top = qMin(qMax(top, margin - disp.height()), vh - margin);
    else
        top = (vh - disp.height()) / 2.0;
    mOffset.setY(top - vh / 2.0 + disp.height() / 2.0);
}

// ------------------------------------------------------------------ 绘制
QRectF ImageView::imageRect() const
{
    const double scale = mFit ? fitScale() : mZoom;
    const QSizeF disp(mImage.width() * scale, mImage.height() * scale);
    QPointF tl;
    if (mFit) {
        tl = QPointF((width() - disp.width()) / 2.0, (height() - disp.height()) / 2.0);
    } else {
        tl = QPointF(width() / 2.0, height() / 2.0) + mOffset
             - QPointF(disp.width() / 2.0, disp.height() / 2.0);
    }
    return QRectF(tl, disp);
}

QPointF ImageView::toImage(const QPointF &pos) const
{
    const QRectF r = imageRect();
    if (r.width() <= 0 || r.height() <= 0 || mImage.isNull())
        return QPointF(0, 0);
    return QPointF((pos.x() - r.x()) / r.width() * mImage.width(),
                   (pos.y() - r.y()) / r.height() * mImage.height());
}

QPointF ImageView::toScreen(const QPointF &pt) const
{
    const QRectF r = imageRect();
    return QPointF(r.x() + pt.x() / mImage.width() * r.width(),
                   r.y() + pt.y() / mImage.height() * r.height());
}

void ImageView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), mBg);

    if (mImage.isNull()) {
        p.setPen(QColor(138, 138, 142));
        p.drawText(rect(), Qt::AlignCenter,
                   QStringLiteral("打开图片（Ctrl+O）或拖入图片\n右键可打开快捷菜单"));
        return;
    }

    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QRectF r = imageRect();

    double deg = 0.0;
    bool preview = false;
    if (mStraightenMode) {
        if (mStrPts.size() == 2 && straightenAngle(deg)) {
            mStrPreviewDeg = deg;
            mStrHasPreview = true;
            preview = true;
        } else if (mStrHasPreview) {
            deg = mStrPreviewDeg;
            preview = true;   // 重画期间保持上次预览角度，不回弹
        }
    }
    if (preview) {
        // 实时预览：绕图片中心旋转
        const QPointF ctr = straightenCenter();
        const QSizeF disp(mImage.width() * (mFit ? fitScale() : mZoom),
                          mImage.height() * (mFit ? fitScale() : mZoom));
        p.save();
        p.translate(ctr);
        p.rotate(deg);
        p.drawPixmap(QRectF(-disp.width() / 2.0, -disp.height() / 2.0,
                            disp.width(), disp.height()),
                     mPixmap, QRectF(mPixmap.rect()));
        p.restore();
    } else if (!mPixmap.isNull()) {
        p.drawPixmap(r, mPixmap, QRectF(mPixmap.rect()));
    }

    if (mCropMode)
        paintCrop(p, r);
    if (mStraightenMode)
        paintStraighten(p, r, preview ? deg : 0.0);
    if (mPerspMode)
        paintPerspective(p);
}

void ImageView::paintCrop(QPainter &p, const QRectF &imgRect)
{
    if (mCropRect.isEmpty() || mImage.isNull())
        return;
    const double scale = imgRect.width() / double(mImage.width());
    const QRectF sr(imgRect.x() + mCropRect.x() * scale,
                    imgRect.y() + mCropRect.y() * scale,
                    mCropRect.width() * scale,
                    mCropRect.height() * scale);

    // 裁剪区域外部压暗
    QPainterPath path;
    path.addRect(QRectF(this->rect()));
    path.addRect(sr);
    path.setFillRule(Qt::OddEvenFill);
    p.setPen(Qt::NoPen);
    p.fillPath(path, QColor(0, 0, 0, 150));

    // 边框（外黑内白虚线）
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(0, 0, 0, 220), 3));
    p.drawRect(sr);
    p.setPen(QPen(QColor(255, 255, 255), 1.5, Qt::DashLine));
    p.drawRect(sr);

    // 手柄
    const auto handles = cropHandles();
    for (const Handle &h : handles) {
        const QRectF hr(h.screenPos.x() - 4, h.screenPos.y() - 4, 8, 8);
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.drawRect(hr);
    }

    // 提示
    const QString txt = QStringLiteral("拖动选择区域 · 拖动边框调整 · Enter 应用 · Esc 取消");
    const QFontMetrics fm = p.fontMetrics();
    const double w = fm.horizontalAdvance(txt) + 24;
    const double h = fm.height() + 10;
    const double x = (width() - w) / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 120));
    p.drawRoundedRect(QRectF(x, 10, w, h), 6, 6);
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRectF(x, 10, w, h), Qt::AlignCenter, txt);
}

// ------------------------------------------------------------------ 裁剪
QList<ImageView::Handle> ImageView::cropHandles() const
{
    QList<Handle> out;
    const QRectF r = mCropRect;
    const QRectF s(toScreen(QPointF(r.left(), r.top())),
                   toScreen(QPointF(r.right(), r.bottom())));
    out << Handle{QStringLiteral("tl"), s.topLeft()}
        << Handle{QStringLiteral("t"),  QPointF(s.center().x(), s.top())}
        << Handle{QStringLiteral("tr"), s.topRight()}
        << Handle{QStringLiteral("l"),  QPointF(s.left(), s.center().y())}
        << Handle{QStringLiteral("r"),  QPointF(s.right(), s.center().y())}
        << Handle{QStringLiteral("bl"), s.bottomLeft()}
        << Handle{QStringLiteral("b"),  QPointF(s.center().x(), s.bottom())}
        << Handle{QStringLiteral("br"), s.bottomRight()};
    return out;
}

QString ImageView::hitHandle(const QPointF &pos) const
{
    for (const Handle &h : cropHandles()) {
        if ((pos - h.screenPos).manhattanLength() <= kHandleRadius)
            return h.name;
    }
    return {};
}

bool ImageView::startCrop()
{
    if (mImage.isNull())
        return false;
    mCropMode = true;
    mCropRect = QRectF(0, 0, mImage.width(), mImage.height());
    mCropPrev = mCropRect;
    mCropGrab.clear();
    setCursor(Qt::CrossCursor);
    update();
    return true;
}

void ImageView::exitCrop()
{
    mCropMode = false;
    mCropRect = QRectF();
    mCropGrab.clear();
    unsetCursor();
    update();
}

QImage ImageView::applyCrop()
{
    if (!mCropMode || mImage.isNull())
        return QImage();
    QRectF r = mCropRect.normalized();
    r = r.intersected(QRectF(mImage.rect()));
    if (r.width() < 1 || r.height() < 1) {
        exitCrop();
        return QImage();
    }
    QImage out = mImage.copy(r.toAlignedRect());
    exitCrop();
    setImage(out);
    return out;
}

void ImageView::cropPress(const QPointF &pos)
{
    const QString h = hitHandle(pos);
    if (!h.isEmpty()) {
        mCropGrab = h;
        return;
    }
    const QPointF ip = toImage(pos);
    if (mCropRect.contains(ip)) {
        mCropGrab = QStringLiteral("move");
        mCropGrabOffset = ip - mCropRect.topLeft();
    } else {
        mCropGrab = QStringLiteral("new");
        mCropAnchor = ip;
        mCropPrev = mCropRect;
    }
}

void ImageView::cropMove(const QPointF &pos)
{
    if (mImage.isNull())
        return;
    const QString grab = mCropGrab;
    if (grab.isEmpty()) {
        updateCropCursor(pos);
        return;
    }
    const QPointF ip = toImage(pos);
    const QRectF ir(0, 0, mImage.width(), mImage.height());
    if (grab == QStringLiteral("move")) {
        const QPointF tl = ip - mCropGrabOffset;
        mCropRect.moveTopLeft(QPointF(
            qMin(qMax(tl.x(), 0.0), qMax(0.0, ir.width() - mCropRect.width())),
            qMin(qMax(tl.y(), 0.0), qMax(0.0, ir.height() - mCropRect.height()))));
    } else if (grab == QStringLiteral("new")) {
        mCropRect = QRectF(mCropAnchor, ip).normalized().intersected(ir);
    } else {
        const double minS = 8.0;
        double left = mCropRect.left(), top = mCropRect.top();
        double right = mCropRect.right(), bottom = mCropRect.bottom();
        if (grab.contains('l')) left = qMin(ip.x(), right - minS);
        if (grab.contains('r')) right = qMax(ip.x(), left + minS);
        if (grab.contains('t')) top = qMin(ip.y(), bottom - minS);
        if (grab.contains('b')) bottom = qMax(ip.y(), top + minS);
        left = qMax(0.0, left);
        top = qMax(0.0, top);
        right = qMin(ir.right(), right);
        bottom = qMin(ir.bottom(), bottom);
        mCropRect = QRectF(QPointF(left, top), QPointF(right, bottom));
    }
    update();
}

void ImageView::cropRelease()
{
    if (mCropGrab == QStringLiteral("new")) {
        if (mCropRect.width() < 4 || mCropRect.height() < 4) {
            mCropRect = mCropPrev;
            update();
        }
    }
    mCropGrab.clear();
}

void ImageView::updateCropCursor(const QPointF &pos)
{
    const QString h = hitHandle(pos);
    Qt::CursorShape cur = Qt::CrossCursor;
    if (h == "tl" || h == "br") cur = Qt::SizeFDiagCursor;
    else if (h == "tr" || h == "bl") cur = Qt::SizeBDiagCursor;
    else if (h == "t" || h == "b") cur = Qt::SizeVerCursor;
    else if (h == "l" || h == "r") cur = Qt::SizeHorCursor;
    else if (mCropRect.contains(toImage(pos))) cur = Qt::SizeAllCursor;
    setCursor(cur);
}

// ------------------------------------------------------------------ 两点摆正
bool ImageView::startStraighten(bool horiz)
{
    if (mImage.isNull())
        return false;
    mStraightenMode = true;
    mStrPts.clear();
    mStrCur = QPointF();
    mStrHoriz = horiz;
    mStrHasPreview = false;
    setCursor(Qt::CrossCursor);
    update();
    return true;
}

void ImageView::exitStraighten()
{
    mStraightenMode = false;
    mStrPts.clear();
    mStrHasPreview = false;
    unsetCursor();
    update();
}

void ImageView::toggleStraightenAxis()
{
    mStrHoriz = !mStrHoriz;
    update();
}

bool ImageView::straightenAngle(double &degOut) const
{
    if (mStrPts.size() != 2 || mImage.isNull())
        return false;
    const QPointF a = mStrPts.at(0);
    const QPointF b = mStrPts.at(1);
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    if (dx * dx + dy * dy < 16.0)   // < 4 像素，视为误点
        return false;
    double ang = qRadiansToDegrees(qAtan2(dy, dx));
    // 归一化到 [-90, 90)，消除线方向歧义
    double norm = std::fmod(ang + 90.0, 180.0);
    if (norm < 0.0)
        norm += 180.0;
    norm -= 90.0;
    if (mStrHoriz) {
        degOut = -norm;
    } else {
        degOut = (norm >= 0.0) ? (90.0 - norm) : (-90.0 - norm);
    }
    return true;
}

double ImageView::currentPreviewDeg() const
{
    double d = 0.0;
    if (mStrPts.size() == 2 && straightenAngle(d))
        return d;
    return mStrHasPreview ? mStrPreviewDeg : 0.0;
}

QPointF ImageView::straightenScreenToImage(const QPointF &sp) const
{
    const double d = currentPreviewDeg();
    if (d != 0.0) {
        const QPointF s = rotateScreenPt(sp, straightenCenter(), -d);
        return toImage(s);
    }
    return toImage(sp);
}

QPointF ImageView::straightenCenter() const
{
    double cx = width() / 2.0;
    double cy = height() / 2.0;
    if (!mFit) {
        cx += mOffset.x();
        cy += mOffset.y();
    }
    return QPointF(cx, cy);
}

QPointF ImageView::rotateScreenPt(const QPointF &sp, const QPointF &center, double deg)
{
    const QPointF v = sp - center;
    const double a = qDegreesToRadians(deg);
    const double c = qCos(a), s = qSin(a);
    // 与 QPainter::rotate 对齐：屏幕点绕 center 视觉顺时针转 deg
    return QPointF(center.x() + v.x() * c - v.y() * s,
                  center.y() + v.x() * s + v.y() * c);
}

void ImageView::paintStraighten(QPainter &p, const QRectF & /*imgRect*/, double deg)
{
    const QColor c(255, 200, 40);
    const QPen thin(c, 1.5, Qt::SolidLine, Qt::RoundCap);
    const QPen dash(c, 1.2, Qt::DashLine);

    const QPointF center = straightenCenter();
    const bool rot = !mStrPts.isEmpty();   // 有点就按预览角度把点也转过去

    auto sp = [&](const QPointF &imgPt) -> QPointF {
        QPointF s = toScreen(imgPt);
        return rot ? rotateScreenPt(s, center, deg) : s;
    };

    if (mStrPts.size() == 1) {
        const QPointF cur = rot ? rotateScreenPt(toScreen(mStrCur), center, deg)
                               : toScreen(mStrCur);
        p.setPen(dash);
        p.drawLine(sp(mStrPts.at(0)), cur);
        p.setBrush(c);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1));
        p.drawEllipse(sp(mStrPts.at(0)), 4, 4);
        p.drawEllipse(cur, 4, 4);
    } else if (mStrPts.size() == 2) {
        p.setPen(thin);
        p.drawLine(sp(mStrPts.at(0)), sp(mStrPts.at(1)));
        p.setBrush(c);
        p.setPen(QPen(QColor(0, 0, 0, 200), 1));
        p.drawEllipse(sp(mStrPts.at(0)), 4, 4);
        p.drawEllipse(sp(mStrPts.at(1)), 4, 4);
    }

    // 顶部提示
    QString tip;
    if (mStrPts.size() < 2) {
        tip = QStringLiteral("在图上点两个参考点 · 再点一下可重画 · H/V 切换水平/垂直 · Esc 取消");
    } else {
        const QString axis = mStrHoriz ? QStringLiteral("水平") : QStringLiteral("垂直");
        tip = QStringLiteral("将连线摆为%1 · 旋转 %2° · H/V 切换基准 · Enter / 右键 确认 · Esc 取消")
                  .arg(axis).arg(deg, 0, 'f', 1);
    }
    const QFontMetrics fm = p.fontMetrics();
    const double w = fm.horizontalAdvance(tip) + 24;
    const double h = fm.height() + 10;
    const double x = (width() - w) / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 130));
    p.drawRoundedRect(QRectF(x, 10, w, h), 6, 6);
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRectF(x, 10, w, h), Qt::AlignCenter, tip);
}

void ImageView::straightenPress(const QPointF & /*pos*/)
{
    // 已有两点：再按下就清空，重新从第一个点开始
    if (mStrPts.size() >= 2) {
        mStrPts.clear();
        update();
    }
}

void ImageView::straightenMove(const QPointF &pos)
{
    mStrCur = straightenScreenToImage(pos);
    update();
}

void ImageView::straightenRelease(const QPointF &pos)
{
    const QPointF ip = straightenScreenToImage(pos);
    if (mStrPts.isEmpty()) {
        mStrPts.append(ip);
    } else {
        const QPointF a = mStrPts.last();
        if ((ip - a).manhattanLength() < 3.0)
            return;   // 与上一点重合，忽略
        if (mStrPts.size() == 1)
            mStrPts.append(ip);
        // size 不会到 2：press 时已清空
    }
    update();
}

// ------------------------------------------------------------------ 透视校准
bool ImageView::startPerspective()
{
    if (mImage.isNull())
        return false;
    // 进入透视模式前退出其他编辑模式，避免状态叠加
    if (mCropMode)
        exitCrop();
    if (mStraightenMode)
        exitStraighten();
    mPerspMode = true;
    mPerspPts.clear();
    const double w = mImage.width(), h = mImage.height();
    // 初始角点 = 整幅图像四角（tl,tr,br,bl）
    mPerspPts << QPointF(0, 0) << QPointF(w, 0) << QPointF(w, h) << QPointF(0, h);
    mPerspGrab = -1;
    mPerspPreview = QImage();
    mPerspPreviewDirty = true;
    setCursor(Qt::CrossCursor);
    update();
    return true;
}

void ImageView::exitPerspective()
{
    mPerspMode = false;
    mPerspPts.clear();
    mPerspGrab = -1;
    mPerspPreview = QImage();
    mPerspPreviewDirty = false;
    unsetCursor();
    update();
}

QImage ImageView::applyPerspective()
{
    if (!mPerspMode || mImage.isNull() || mPerspPts.size() != 4)
        return QImage();
    QPolygonF src;
    for (const QPointF &pt : mPerspPts)
        src << pt;
    // 源四角 → 整幅图像矩形，逐像素逆采样得到校正结果
    QImage out = warpQuadToRect(mImage, src,
                                QRectF(0, 0, double(mImage.width()), double(mImage.height())));
    if (out.isNull())
        return QImage();
    exitPerspective();
    setImage(out);
    return out;
}

int ImageView::perspHandleAt(const QPointF &pos) const
{
    for (int i = 0; i < mPerspPts.size(); ++i) {
        if ((pos - toScreen(mPerspPts.at(i))).manhattanLength() <= kHandleRadius)
            return i;
    }
    return -1;
}

void ImageView::perspPress(const QPointF &pos)
{
    mPerspGrab = perspHandleAt(pos);
    if (mPerspGrab >= 0)
        setCursor(Qt::ClosedHandCursor);
}

void ImageView::perspMove(const QPointF &pos)
{
    if (mPerspGrab < 0 || mPerspPts.size() != 4)
        return;
    // 允许角点拖到图片范围之外：部分透视校准需要扩展超出原图边界才能调到位。
    // 越出原图的部分在校正结果中按透明处理（见 bilinearSample）。
    mPerspPts[mPerspGrab] = toImage(pos);
    mPerspPreviewDirty = true;   // 角点变化，实时预览待重算
    update();
}

void ImageView::perspRelease()
{
    if (mPerspGrab >= 0)
        setCursor(Qt::CrossCursor);
    mPerspGrab = -1;
}

void ImageView::paintPerspective(QPainter &p)
{
    if (mImage.isNull() || mPerspPts.size() != 4)
        return;

    const QColor c(64, 158, 255);
    QPointF s[4];
    for (int i = 0; i < 4; ++i)
        s[i] = toScreen(mPerspPts.at(i));
    QPolygonF quad;
    quad << s[0] << s[1] << s[2] << s[3];

    // 边框（外黑内蓝）
    p.setBrush(Qt::NoBrush);
    QPen borderPen(QColor(0, 0, 0, 200), 3, Qt::SolidLine, Qt::RoundCap);
    borderPen.setJoinStyle(Qt::RoundJoin);
    p.setPen(borderPen);
    p.drawPolygon(quad);
    p.setPen(QPen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPolygon(quad);

    // 内部透视网格（等参数边插值，直观示意校正区域）
    const int div = 6;
    p.setPen(QPen(QColor(c.red(), c.green(), c.blue(), 150), 1.0,
                  Qt::SolidLine, Qt::RoundCap));
    for (int i = 1; i < div; ++i) {
        const double t = double(i) / double(div);
        p.drawLine(s[0] * (1 - t) + s[1] * t, s[3] * (1 - t) + s[2] * t); // 横线
        p.drawLine(s[0] * (1 - t) + s[3] * t, s[1] * (1 - t) + s[2] * t); // 纵线
    }

    // 四角手柄
    for (int i = 0; i < 4; ++i) {
        const QRectF hr(s[i].x() - 6, s[i].y() - 6, 12, 12);
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(QColor(40, 40, 40), 1));
        p.drawRect(hr);
    }

    // 顶部提示
    const QString tip = QStringLiteral("拖动四个角点调整透视（可拖出图片）· Enter 应用 · Esc / 右键 取消");
    const QFontMetrics fm = p.fontMetrics();
    const double w = fm.horizontalAdvance(tip) + 24;
    const double h = fm.height() + 10;
    const double x = (width() - w) / 2.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 130));
    p.drawRoundedRect(QRectF(x, 10, w, h), 6, 6);
    p.setPen(QColor(255, 255, 255));
    p.drawText(QRectF(x, 10, w, h), Qt::AlignCenter, tip);

    // 右下角实时结果预览（拖动角点时即时刷新，所见即所得）
    if (mPerspPreviewDirty) {
        const double pw = qBound(160.0, width() / 4.0, 360.0);
        const double ph = pw * double(mImage.height()) / double(mImage.width());
        QPolygonF srcQuad;
        for (const QPointF &pt : mPerspPts)
            srcQuad << pt;
        QImage prev = warpQuadToRect(mImage, srcQuad, QRectF(0, 0, pw, ph));
        if (!prev.isNull())
            mPerspPreview = prev;
        mPerspPreviewDirty = false;   // 即使本次退化也清标记，避免反复重算
    }
    if (!mPerspPreview.isNull()) {
        const double pad = 12.0;
        const double iw = double(mPerspPreview.width());
        const double ih = double(mPerspPreview.height());
        const QRectF box(width() - iw - pad, height() - ih - pad, iw, ih);
        p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
        p.setBrush(QColor(0, 0, 0, 120));
        p.drawRoundedRect(box.adjusted(-4, -4, 4, 4), 6, 6);
        p.drawImage(box, mPerspPreview);
        // 标签
        const QString lbl = QStringLiteral("结果预览");
        const double lw = fm.horizontalAdvance(lbl) + 14;
        const double lh = fm.height() + 6;
        const QRectF lb(box.left() + box.width() - lw, box.top() - lh - 2, lw, lh);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 130));
        p.drawRoundedRect(lb, 5, 5);
        p.setPen(QColor(255, 255, 255));
        p.drawText(lb, Qt::AlignCenter, lbl);
    }
}

// ------------------------------------------------------------------ 鼠标
void ImageView::mousePressEvent(QMouseEvent *event)
{
    mLastPos = event->position();
    if (mCropMode) {
        if (event->button() == Qt::RightButton) {
            exitCrop();
            emit cropCancelRequested();
        } else if (event->button() == Qt::LeftButton) {
            cropPress(event->position());
        }
        return;
    }
    if (mStraightenMode) {
        if (event->button() == Qt::RightButton) {
            // 右键确认应用（与 Enter 等效）；点未画完则忽略
            double deg = 0.0;
            if (straightenAngle(deg)) {
                mStrSuppressMenu = true;
                exitStraighten();
                emit straightenApplyRequested(deg);
            }
        } else if (event->button() == Qt::LeftButton) {
            straightenPress(event->position());
        }
        return;
    }
    if (mPerspMode) {
        if (event->button() == Qt::RightButton) {
            exitPerspective();
            emit perspectiveCancelRequested();
        } else if (event->button() == Qt::LeftButton) {
            perspPress(event->position());
        }
        return;
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        if (event->button() == Qt::LeftButton && !mSettings.dragPan)
            return;
        mDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void ImageView::mouseMoveEvent(QMouseEvent *event)
{
    if (mCropMode) {
        cropMove(event->position());
        return;
    }
    if (mStraightenMode) {
        straightenMove(event->position());
        return;
    }
    if (mPerspMode) {
        perspMove(event->position());
        return;
    }
    if (mDragging) {
        const QPointF delta = event->position() - mLastPos;
        mLastPos = event->position();
        if (!mFit) {
            mOffset += delta;
            clampOffset();
            update();
        }
    }
}

void ImageView::mouseReleaseEvent(QMouseEvent *event)
{
    if (mCropMode) {
        if (event->button() == Qt::LeftButton)
            cropRelease();
        return;
    }
    if (mStraightenMode) {
        if (event->button() == Qt::LeftButton)
            straightenRelease(event->position());
        return;
    }
    if (mPerspMode) {
        if (event->button() == Qt::LeftButton)
            perspRelease();
        return;
    }
    if (event->button() == Qt::LeftButton || event->button() == Qt::MiddleButton) {
        mDragging = false;
        unsetCursor();
    }
}

void ImageView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton
        && !mCropMode && !mStraightenMode && !mPerspMode)
        emit doubleClicked();
}

void ImageView::wheelEvent(QWheelEvent *event)
{
    if (mImage.isNull() || mCropMode || mStraightenMode || mPerspMode)
        return;
    const int delta = event->angleDelta().y();
    if (delta == 0)
        return;
    double factor = qPow(mSettings.wheelStep, delta / 120.0);
    if (mSettings.wheelInvert)
        factor = 1.0 / factor;
    zoomAt(event->position(), factor);
}

void ImageView::contextMenuEvent(QContextMenuEvent *event)
{
    if (mCropMode) {
        exitCrop();
        emit cropCancelRequested();
        return;
    }
    if (mPerspMode) {
        exitPerspective();
        emit perspectiveCancelRequested();
        return;
    }
    if (mStrSuppressMenu) {
        // 右键刚在摆正模式下确认应用，吞掉随后的 contextMenu 事件
        mStrSuppressMenu = false;
        event->accept();
        return;
    }
    if (mStraightenMode) {
        event->accept();   // 摆正模式下不弹菜单
        return;
    }
    emit contextMenuRequested(event->globalPos());
}

void ImageView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (mFit && !mImage.isNull())
        emit zoomChanged(fitScale());
    update();
}

// ------------------------------------------------------------------ 拖放
void ImageView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ImageView::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            emit fileDropped(url.toLocalFile());
            break;
        }
    }
}

} // namespace kandu
