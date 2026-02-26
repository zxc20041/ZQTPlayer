#include "LanguageManager.h"
#include <QCoreApplication>
#include <QSettings>
#include <QQmlEngine>

// Static translator instance – lives for the entire application
QTranslator LanguageManager::s_translator;

// ── static: called from main.cpp BEFORE engine loads ──
bool LanguageManager::loadInitialTranslation()
{
    QSettings settings;
    QString locale = settings.value("language", "en_US").toString();
    if (locale.isEmpty() || locale == "en_US") {
        return true; // English is the source language – no translator needed
    }
    if (s_translator.load(":/i18n/app_" + locale + ".qm")) {
        QCoreApplication::installTranslator(&s_translator);
        return true;
    }
    return false;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent)
{
    m_langs.append(LangEntry{"English", "en_US"});
    m_langs.append(LangEntry{QString::fromUtf8("\347\256\200\344\275\223\344\270\255\346\226\207"), "zh_CN"});

    QSettings settings;
    QString saved = settings.value("language", "en_US").toString();

    m_currentIndex = 0;
    for (int i = 0; i < m_langs.size(); ++i) {
        if (m_langs[i].locale == saved) {
            m_currentIndex = i;
            break;
        }
    }
    // NOTE: translator is already installed by loadInitialTranslation() in main.cpp
}

QStringList LanguageManager::availableLanguages() const
{
    QStringList names;
    for (const auto &entry : m_langs) {
        names << entry.displayName;
    }
    return names;
}

int LanguageManager::currentIndex() const
{
    return m_currentIndex;
}

QString LanguageManager::currentLanguage() const
{
    if (m_currentIndex >= 0 && m_currentIndex < m_langs.size()) {
        return m_langs[m_currentIndex].displayName;
    }
    return "English";
}

void LanguageManager::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_langs.size() || index == m_currentIndex) {
        return;
    }

    m_currentIndex = index;
    const QString &locale = m_langs[index].locale;

    QSettings settings;
    settings.setValue("language", locale);

    // Remove current translator
    QCoreApplication::removeTranslator(&s_translator);

    // Load new translation (skip for English – it's the source language)
    if (locale != "en_US") {
        if (s_translator.load(":/i18n/app_" + locale + ".qm")) {
            QCoreApplication::installTranslator(&s_translator);
        }
    }

    // Retranslate all QML bindings using qsTr()
    auto *engine = qmlEngine(this);
    if (engine) {
        engine->retranslate();
    }

    emit currentIndexChanged();
    emit languageChanged();
}
