#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "app/AppController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("Chrona"));
    QGuiApplication::setOrganizationName(QStringLiteral("Chrona"));

    QQmlApplicationEngine engine;
    AppController controller;
    if (!controller.initialize(&engine)) {
        return 1;
    }

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] {
        QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.loadFromModule("Chrona", "Main");
    return app.exec();
}
