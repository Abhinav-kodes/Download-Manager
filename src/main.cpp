#include <QApplication>
#include "downloadwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    DownloadWindow window;
    window.show();

    return app.exec();
}
