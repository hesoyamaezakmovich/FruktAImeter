#include <auroraapp.h>
#include <QtQuick>
#include "Detector.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> application(Aurora::Application::application(argc, argv));
    application->setOrganizationName(QStringLiteral("ru.tk"));
    application->setApplicationName(QStringLiteral("FruktAImeter"));

    // Регистрируем Detector для использования в QML
    qmlRegisterType<Detector>("ru.tk.FruktAImeter", 1, 0, "Detector");

    QScopedPointer<QQuickView> view(Aurora::Application::createView());
    view->setSource(Aurora::Application::pathTo(QStringLiteral("qml/FruktAImeter.qml")));
    view->show();

    return application->exec();
}
