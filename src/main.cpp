#include "window.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QTimer>
int main(int argc,char **argv) {
#if QT_VERSION < QT_VERSION_CHECK(6,0,0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc,argv);
    app.setApplicationName("Visual Toolbox");app.setApplicationVersion("0.2.0");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont("Noto Sans CJK SC",10));
    Window window;
    const auto args=app.arguments();
    if(args.size()==3 && args[1]=="--smoke") {
        window.smoke(args[2]);return 0;
    }
    window.show();return app.exec();
}
