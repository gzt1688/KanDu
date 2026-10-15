#include <QApplication>
#include <QTimer>

#include "mainwindow.h"
#include "singleton.h"

int main(int argc, char *argv[])
{
    // 单实例：已有 KanDu 运行时，把图片转给已有窗口并退出
    if (!kandu::acquireSingleton()) {
        if (argc > 1) {
            const QString path = QString::fromLocal8Bit(argv[1]);
            // 退出码 0=已转交并确认，2=未找到已有窗口（转发失败）
            return kandu::sendPathToExisting(path) ? 0 : 2;
        }
        return 0;
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("KanDu"));
    QApplication::setOrganizationName(QStringLiteral("KanDu"));

    kandu::MainWindow win;
    win.show();

    // 命令行图片参数（关联打开 / 拖到 exe 上）
    if (argc > 1) {
        const QString path = QString::fromLocal8Bit(argv[1]);
        QTimer::singleShot(0, &win, [&win, path] { win.openPath(path, false); });
    }

    return app.exec();
}
