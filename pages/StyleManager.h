#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqml.h>

class StyleManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList availableStyles READ availableStyles CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentStyle READ currentStyle NOTIFY currentIndexChanged)

public:
    explicit StyleManager(QObject *parent = nullptr);

    QStringList availableStyles() const;
    int currentIndex() const;
    QString currentStyle() const;

    Q_INVOKABLE void setCurrentIndex(int index);

signals:
    void currentIndexChanged();
    void restartRequired();

private:
    QStringList m_styles;
    int m_currentIndex = 0;
};
