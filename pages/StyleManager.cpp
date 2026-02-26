#include "StyleManager.h"
#include <QQuickStyle>
#include <QSettings>

StyleManager::StyleManager(QObject *parent)
    : QObject(parent)
{
    m_styles << "Fusion" << "Material";

    QSettings settings;
    QString saved = settings.value("style", QQuickStyle::name()).toString();
    if (saved.isEmpty()) {
        saved = "Fusion";
    }

    m_currentIndex = m_styles.indexOf(saved);
    if (m_currentIndex < 0) {
        m_currentIndex = 0;
    }
}

QStringList StyleManager::availableStyles() const
{
    return m_styles;
}

int StyleManager::currentIndex() const
{
    return m_currentIndex;
}

QString StyleManager::currentStyle() const
{
    return m_styles.value(m_currentIndex, "Fusion");
}

void StyleManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_styles.size() || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    QSettings settings;
    settings.setValue("style", m_styles.at(index));
    QQuickStyle::setStyle(m_styles.at(index));
    emit currentIndexChanged();
    emit restartRequired();
}
