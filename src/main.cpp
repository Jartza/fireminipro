#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Firebay refurb");
    QCoreApplication::setApplicationName("fireminipro");
    app.setWindowIcon(QIcon(":/appicon.png"));

    MainWindow w;
    w.show();
    return app.exec();
}
