#include <QApplication>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QMainWindow>
#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QPlainTextEdit>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QSplitter>
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
