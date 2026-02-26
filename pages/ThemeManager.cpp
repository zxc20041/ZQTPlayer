#include "ThemeManager.h"
#include <QGuiApplication>
#include <QStyleHints>
#include <QSettings>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    QSettings settings;
    m_currentTheme = settings.value("theme", FollowSystem).toInt();
    applyTheme(m_currentTheme);
}

int ThemeManager::currentTheme() const
{
    return m_currentTheme;
}

void ThemeManager::setCurrentTheme(int theme)
{
    if (theme < 0 || theme > 2 || theme == m_currentTheme) {
        return;
    }

    m_currentTheme = theme;
    QSettings settings;
    settings.setValue("theme", theme);
    applyTheme(theme);
    emit currentThemeChanged();
}

void ThemeManager::applyFromSettings()
{
    QSettings settings;
    int theme = settings.value("theme", FollowSystem).toInt();

    auto *hints = QGuiApplication::styleHints();
    switch (theme) {
    case Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case FollowSystem:
    default:
        hints->setColorScheme(Qt::ColorScheme::Unknown);
        break;
    }
}

void ThemeManager::applyTheme(int theme)
{
    auto *hints = QGuiApplication::styleHints();
    switch (theme) {
    case Light:
        hints->setColorScheme(Qt::ColorScheme::Light);
        break;
    case Dark:
        hints->setColorScheme(Qt::ColorScheme::Dark);
        break;
    case FollowSystem:
    default:
        hints->setColorScheme(Qt::ColorScheme::Unknown);
        break;
    }
}
