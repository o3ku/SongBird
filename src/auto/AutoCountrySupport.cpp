#include "auto/AutoCountrySupport.h"

#include <QHash>

namespace {

const QStringList& fixedCountryCodes()
{
    static const QStringList codes{
        QStringLiteral("US"),
        QStringLiteral("HK"),
        QStringLiteral("TW"),
        QStringLiteral("JP"),
        QStringLiteral("KR"),
        QStringLiteral("AU"),
        QStringLiteral("SG"),
        QStringLiteral("MY"),
        QStringLiteral("DE"),
        QStringLiteral("GB"),
        QStringLiteral("ID"),
        QStringLiteral("UNKNOWN")
    };
    return codes;
}

} // namespace

QStringList autoFixedCountryCodes()
{
    return fixedCountryCodes();
}

bool isAutoFixedCountryCode(const QString& countryCode)
{
    return fixedCountryCodes().contains(countryCode.trimmed().toUpper());
}

QString normalizeAutoCountryCode(const QString& value)
{
    const QString code = value.trimmed().toUpper();
    if (code.isEmpty()) {
        return {};
    }
    return isAutoFixedCountryCode(code) ? code : autoUnknownCountryCode();
}

QString autoUnknownCountryCode()
{
    return QStringLiteral("UNKNOWN");
}

QString autoCountryDisplayName(const QString& countryCode, const QString& countryName)
{
    const QString code = normalizeAutoCountryCode(countryCode);
    if (code == autoUnknownCountryCode()) {
        return QStringLiteral("Unknown");
    }
    static const QHash<QString, QString> names{
        {QStringLiteral("AU"), QStringLiteral("Australia")},
        {QStringLiteral("DE"), QStringLiteral("Germany")},
        {QStringLiteral("GB"), QStringLiteral("United Kingdom")},
        {QStringLiteral("HK"), QStringLiteral("Hong Kong")},
        {QStringLiteral("ID"), QStringLiteral("Indonesia")},
        {QStringLiteral("JP"), QStringLiteral("Japan")},
        {QStringLiteral("KR"), QStringLiteral("South Korea")},
        {QStringLiteral("MY"), QStringLiteral("Malaysia")},
        {QStringLiteral("SG"), QStringLiteral("Singapore")},
        {QStringLiteral("TW"), QStringLiteral("Taiwan")},
        {QStringLiteral("US"), QStringLiteral("United States")},
    };

    if (code.isEmpty()) {
        return countryName.trimmed().isEmpty() ? QStringLiteral("Unknown") : countryName.trimmed();
    }

    const QString mappedName = names.value(code);
    if (!mappedName.isEmpty()) {
        return QStringLiteral("%1 | %2").arg(code, mappedName);
    }

    const QString trimmedName = countryName.trimmed();
    return trimmedName.isEmpty()
        ? code
        : QStringLiteral("%1 | %2").arg(code, trimmedName);
}
