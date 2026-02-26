#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QDebug>
#include "ThemeManager.h"
#include "LanguageManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("ZQT");
    app.setApplicationName("ZQTPlayer");

    // Restore saved style
    QSettings settings;
    QString style = settings.value("style", "Fusion").toString();
    QQuickStyle::setStyle(style);

    // Restore saved color scheme BEFORE QML loads
    ThemeManager::applyFromSettings();

    // Restore saved language BEFORE QML loads – so initial qsTr() strings are translated
    LanguageManager::loadInitialTranslation();

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl &url) {
            qCritical().noquote() << "Failed to create root object from:" << url.toString();
            QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    engine.loadFromModule("ZQTPlayer", "Home");
    if (engine.rootObjects().isEmpty()) 
    {
        return -1;
    }

    return app.exec();
}
