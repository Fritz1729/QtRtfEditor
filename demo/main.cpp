#include "DemoWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication::setOrganizationName("QtRtfEditor");
    QApplication::setApplicationName("Demo");
    QApplication app(argc, argv);

    DemoWindow window;
    window.show();

    return app.exec();
}
