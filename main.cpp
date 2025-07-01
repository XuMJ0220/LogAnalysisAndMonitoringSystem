#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("分布式日志监控系统");
    app.setOrganizationName("LogMonitor");
    MainWindow w;
    w.resize(1024, 768);
    w.show();
    return app.exec();
} 