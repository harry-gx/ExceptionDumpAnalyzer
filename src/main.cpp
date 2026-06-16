#include <QApplication>
#include <QIcon>

#include "ui/main_window.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ExceptionDumpAnalyzer"));
    QApplication::setOrganizationName(QStringLiteral("dev_fbl"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app_icon.png")));

    MainWindow window;
    window.show();

    return app.exec();
}
