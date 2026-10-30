#include "appcore/LocationCatalog.h"

#include <QHash>
#include <QStringList>

#include "appcore/CountryNameInference.h"
#include "common/ServerDisplayName.h"

namespace {

struct CatalogEntry {
    const char* id;
    const char* countryCode;
};

// The fixed item list. HK has no row of its own on purpose: Hong Kong nodes fold into the merged
// TW item (see locationIdForCountryCode), because the product groups 香港/台湾 as one location.
const CatalogEntry kCatalogEntries[] = {
    {"FAVORITE", ""},
    {"US", "US"},
    {"SG", "SG"},
    {"TW", "TW"},
    {"JP", "JP"},
    {"KR", "KR"},
    {"GB", "GB"},
    {"DE", "DE"},
    {"RU", "RU"},
    {"AU", "AU"},
    {"OTHER", ""},
};

// Codes with an item of their own, plus the folds. Everything else is OTHER.
struct CountryFold {
    const char* from;
    const char* to;
};

const CountryFold kCountryFolds[] = {
    {"HK", "TW"},
};

} // namespace

QList<LocationCatalog::Item> LocationCatalog::items()
{
    QList<Item> result;
    const int count = sizeof(kCatalogEntries) / sizeof(kCatalogEntries[0]);
    result.reserve(count);
    for (const CatalogEntry& entry : kCatalogEntries) {
        result.append(Item{
            QString::fromLatin1(entry.id),
            QString::fromLatin1(entry.countryCode)});
    }
    return result;
}

bool LocationCatalog::isKnownLocationId(const QString& locationId)
{
    // Case-insensitive on purpose: tab keys, the persisted override and the state file all carry
    // the canonical upper-case form, but a hand-edited file or a future writer should not be able
    // to make a node vanish from its item by lowering its letters.
    const QString normalized = locationId.trimmed().toUpper();
    const QList<Item> catalog = items();
    for (const Item& item : catalog) {
        if (item.id == normalized) {
            return true;
        }
    }
    return false;
}

bool LocationCatalog::isCountryLocationId(const QString& locationId)
{
    if (!isKnownLocationId(locationId)) {
        return false;
    }
    const QString normalized = locationId.trimmed().toUpper();
    return normalized != QLatin1String(kFavoriteId) && normalized != QLatin1String(kOtherId);
}

QString LocationCatalog::displayName(const QString& locationId)
{
    // English-only on purpose, matching the rest of the catalog (auto/AutoCountrySupport made the
    // same call): the tab label is the item id's own name and the ui layer owns translation of the
    // two specials, which are the only strings here.
    static const QHash<QString, QString> names{
        {QStringLiteral("US"), QStringLiteral("United States")},
        {QStringLiteral("SG"), QStringLiteral("Singapore")},
        // The merged Hong Kong / Taiwan item: labeled "Hong Kong / Taiwan" because a node in it
        // may be either, and "TW" alone would read as Taiwan-only.
        {QStringLiteral("TW"), QStringLiteral("Hong Kong / Taiwan")},
        {QStringLiteral("JP"), QStringLiteral("Japan")},
        {QStringLiteral("KR"), QStringLiteral("South Korea")},
        {QStringLiteral("GB"), QStringLiteral("United Kingdom")},
        {QStringLiteral("DE"), QStringLiteral("Germany")},
        {QStringLiteral("RU"), QStringLiteral("Russia")},
        {QStringLiteral("AU"), QStringLiteral("Australia")},
    };
    return names.value(locationId.trimmed().toUpper());
}

QString LocationCatalog::shortName(const QString& locationId)
{
    // The compact spelling of each item. Deliberately short even where it diverges from the ISO
    // code's official feel: "UK" rather than "GB" (the tab is for users, not for parsers), and
    // "HK/TW" so the merged item says both halves in three glyphs. Empty for the specials: their
    // labels are translated ui strings, not catalog data.
    static const QHash<QString, QString> names{
        {QStringLiteral("US"), QStringLiteral("US")},
        {QStringLiteral("SG"), QStringLiteral("SG")},
        {QStringLiteral("TW"), QStringLiteral("HK/TW")},
        {QStringLiteral("JP"), QStringLiteral("JP")},
        {QStringLiteral("KR"), QStringLiteral("KR")},
        {QStringLiteral("GB"), QStringLiteral("UK")},
        {QStringLiteral("DE"), QStringLiteral("DE")},
        {QStringLiteral("RU"), QStringLiteral("RU")},
        {QStringLiteral("AU"), QStringLiteral("AU")},
    };
    return names.value(locationId.trimmed().toUpper());
}

QString LocationCatalog::locationIdForCountryCode(const QString& countryCode)
{
    const QString code = countryCode.trimmed().toUpper();
    if (code.isEmpty() || code == CountryNameInference::unknownCountryCode()) {
        return QLatin1String(kOtherId);
    }

    for (const CountryFold& fold : kCountryFolds) {
        if (code == QLatin1String(fold.from)) {
            return QLatin1String(fold.to);
        }
    }

    const QList<Item> catalog = items();
    for (const Item& item : catalog) {
        if (!item.countryCode.isEmpty() && item.countryCode == code) {
            return item.id;
        }
    }
    return QLatin1String(kOtherId);
}

QString LocationCatalog::inferredLocationIdForNodeName(const QString& name)
{
    return locationIdForCountryCode(CountryNameInference::inferCountryCodeFromNodeName(name));
}

QString LocationCatalog::normalizedOverride(const QString& locationOverride)
{
    // Normalized to the canonical upper-case id, not merely accepted: every comparison downstream
    // (the filter, the tab keys) uses the canonical form, and passing a lower-case value through
    // would hide the node from the very item it names.
    const QString normalized = locationOverride.trimmed().toUpper();
    // A favorite is not an override value; conflating them would make "clear the location" and
    // "unfavorite" the same write. An unknown value is treated as no override (see header).
    return isCountryLocationId(normalized) || normalized == QLatin1String(kOtherId)
        ? normalized
        : QString();
}

QString LocationCatalog::resolvedLocationId(const VmessItem& server)
{
    return resolvedLocationId(server.locationOverride, serverDisplayName(server, QString()));
}

QString LocationCatalog::resolvedLocationId(const QString& locationOverride, const QString& displayName)
{
    const QString overrideId = normalizedOverride(locationOverride);
    if (!overrideId.isEmpty()) {
        return overrideId;
    }
    return inferredLocationIdForNodeName(displayName);
}
