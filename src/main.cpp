#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Firebay refurb");
    QCoreApplication::setApplicationName("fireminipro");
    app.setWindowIcon(QIcon(":/appicon.png"));

    const QString langOverride = qEnvironmentVariable("FIREMINIPRO_LANG").trimmed();
    const QLocale targetLocale = langOverride.isEmpty() ? QLocale::system() : QLocale(langOverride);

    QTranslator qtTranslator;
    if (qtTranslator.load(targetLocale, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
        app.installTranslator(&qtTranslator);
    }

    QTranslator appTranslator;
    if (appTranslator.load(targetLocale, QStringLiteral("fireminipro"), QStringLiteral("_"),
                           QStringLiteral(":/i18n"))) {
        app.installTranslator(&appTranslator);
    }

    MainWindow w;
    w.show();
    return app.exec();
}
