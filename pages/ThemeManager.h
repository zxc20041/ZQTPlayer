#pragma once

#include <QObject>
#include <QtQml/qqml.h>

class ThemeManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int currentTheme READ currentTheme WRITE setCurrentTheme NOTIFY currentThemeChanged)

public:
    // Values match the RadioButton order in Settings.qml
    enum Theme { Light = 0, Dark = 1, FollowSystem = 2 };
    Q_ENUM(Theme)

    explicit ThemeManager(QObject *parent = nullptr);

    int currentTheme() const;
    Q_INVOKABLE void setCurrentTheme(int theme);

    // Called from main.cpp before QML loads to set initial color scheme
    static void applyFromSettings();

signals:
    void currentThemeChanged();

private:
    void applyTheme(int theme);
    int m_currentTheme = FollowSystem;
};
