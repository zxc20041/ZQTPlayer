#include "SettingsController.h"

SettingsController::SettingsController(QObject *parent)
    : QObject(parent)
{
}

QString SettingsController::title() const
{
    return QStringLiteral("Settings");
}
