#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>
#include <QtQml/qqml.h>

class LanguageManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList availableLanguages READ availableLanguages CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentIndexChanged)

public:
    explicit LanguageManager(QObject *parent = nullptr);

    QStringList availableLanguages() const;
    int currentIndex() const;
    QString currentLanguage() const;

    Q_INVOKABLE void setCurrentIndex(int index);

    /// Called from main.cpp BEFORE QQmlApplicationEngine loads QML.
    /// Installs the translator for the saved locale so initial strings are translated.
    static bool loadInitialTranslation();

signals:
    void currentIndexChanged();
    void languageChanged();

private:
    struct LangEntry {
        QString displayName;
        QString locale;       // e.g. "en_US", "zh_CN"
    };

    QList<LangEntry> m_langs;
    int m_currentIndex = 0;

    // Shared translator – used by both main.cpp init and runtime switching
    static QTranslator s_translator;
};
