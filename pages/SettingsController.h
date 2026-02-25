#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqml.h>

class SettingsController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString title READ title CONSTANT)

public:
    explicit SettingsController(QObject *parent = nullptr);

    QString title() const;
};
