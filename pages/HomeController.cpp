#include "HomeController.h"

HomeController::HomeController(QObject *parent)
    : QObject(parent)
{
}

QString HomeController::title() const
{
    return QStringLiteral("Home");
}
