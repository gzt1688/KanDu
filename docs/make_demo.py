# 生成 README 演示风景图
from PySide6.QtGui import (QImage, QPainter, QColor, QLinearGradient,
                           QPolygonF, QBrush, QPen)
from PySide6.QtCore import QPointF, QRectF, Qt

img = QImage(1600, 1000, QImage.Format.Format_RGB32)
p = QPainter(img)
p.setRenderHint(QPainter.RenderHint.Antialiasing)
g = QLinearGradient(0, 0, 0, 1000)
g.setColorAt(0, QColor('#2c3e8f'))
g.setColorAt(0.55, QColor('#e08a5a'))
g.setColorAt(1, QColor('#f2cf9a'))
p.fillRect(QRectF(0, 0, 1600, 1000), QBrush(g))
p.setPen(Qt.PenStyle.NoPen)
p.setBrush(QColor('#ffd76a'))
p.drawEllipse(QPointF(1220, 300), 85, 85)
p.setBrush(QColor('#5b6e9e'))
p.drawPolygon(QPolygonF([QPointF(0, 620), QPointF(300, 380), QPointF(700, 600),
                         QPointF(1100, 360), QPointF(1600, 640),
                         QPointF(1600, 1000), QPointF(0, 1000)]))
p.setBrush(QColor('#31456e'))
p.drawPolygon(QPolygonF([QPointF(0, 760), QPointF(450, 540), QPointF(900, 780),
                         QPointF(1300, 560), QPointF(1600, 800),
                         QPointF(1600, 1000), QPointF(0, 1000)]))
p.setBrush(QColor('#f2b36b'))
for i in range(7):
    p.drawEllipse(QPointF(1160 + i * 38, 690 + i * 28), 36, 7)
p.end()
img.save(r'F:\softai\KanDuCpp\docs\demo.png', 'PNG')
print('demo ok')
