#pragma once

#include <QLocale>
#include <QString>

namespace UiLanguage {

// The UI language resolved once at startup from the configured `ui.languageCode`
// (see `loadConfiguredLanguageCode()` in `main.cpp`). It is kept here because UI
// helpers in `common/` need to know which language the application is running in,
// and that is deliberately not the same thing as the system locale.
inline QString& configuredLanguageStorage()
{
    static QString languageCode;
    return languageCode;
}

// Called once from `main()` before any UI object is constructed.
inline void setConfiguredLanguage(const QString& languageCode)
{
    configuredLanguageStorage() = languageCode;
}

inline QString configuredLanguage()
{
    return configuredLanguageStorage();
}

// Decides whether a UI running with `languageCode` is Simplified Chinese.
//
// This mirrors `installConfiguredTranslator()` in `main.cpp`: an explicit "en" never
// loads a translator, an explicit "zh_CN" always does, and an unset code means "follow
// the system default", which is what the settings page default resolves to. Only
// "zh_CN" counts as Chinese because `SongBird_zh_CN.qm` is the only translation the
// application ships.
//
// Do not replace this with a bare `QLocale().name() == "zh_CN"` check. `QLocale()`
// reports the *formatting* default locale, and `QLocale::setDefault()` is only called
// on the Chinese path, so an English UI on a Chinese Windows install would still be
// treated as Chinese and keep localizing its standard dialog buttons.
inline bool isChinese(const QString& languageCode, const QString& systemLocaleName)
{
    if (languageCode == QStringLiteral("en")) {
        return false;
    }

    if (languageCode == QStringLiteral("zh_CN")) {
        return true;
    }

    return systemLocaleName == QStringLiteral("zh_CN");
}

inline bool isChinese()
{
    return isChinese(configuredLanguageStorage(), QLocale().name());
}

} // namespace UiLanguage
