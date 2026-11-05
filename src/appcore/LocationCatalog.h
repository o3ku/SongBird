#pragma once

#include <QList>
#include <QString>

#include "domain/models/VmessItem.h"


// The main window's location-grouped server view ("group by location" toggle on the subscription
// tab bar). A *location* is a user-facing grouping item: FAVORITE (hand-picked nodes), the fixed
// country items below, and OTHER (everything the catalog has no item for, including an inference
// that came back UNKNOWN).
//
// Location membership is state, not configuration: it is resolved per node at display time and
// only the manual part (VmessItem::locationOverride / favorite) is persisted, through
// JsonConfigStateSerialization beside testResult. Nothing here ever writes to a subscription --
// grouping by location must not change the subscription list, the subId of a node, or the server
// order, which is what "the groups own nodes directly, subscriptions stay untouched" means.
//
// Resolution order is deliberate: a manual override (right-click move, exit-location fix) beats
// the name-based inference, and the inference is recomputed from the current name so a renamed
// node lands in the right item without anyone remembering to refresh anything.
//
// Pure and I/O-free so it is unit-testable without a config file, per the repo rule for decision
// logic. Country-name inference itself lives in appcore/CountryNameInference; this catalog is the
// policy on top of it (which codes exist as items, in what order, and where the rest go).
class LocationCatalog final {
public:
    // Stable ids used in tab keys ("loc:US") and in the persisted override. Values are ISO codes
    // where a real country backs the item; the two specials are not ISO codes, which is what makes
    // an override written by a future catalog layout still unambiguous.
    static constexpr const char* kFavoriteId = "FAVORITE";
    static constexpr const char* kOtherId = "OTHER";

    struct Item {
        QString id;
        QString countryCode; // empty for FAVORITE/OTHER
    };

    // Fixed order of the location items: Favorite first, then the countries in the order the
    // product spec fixed them (US, SG, HK+TW merged as one TW item, JP, KR, GB, DE, RU, AU),
    // OTHER last. The list itself is the source of truth for the order; callers render it as-is.
    static QList<Item> items();

    static bool isKnownLocationId(const QString& locationId);
    static bool isCountryLocationId(const QString& locationId);

    // User-facing label for a location item ("Favorite", "Other", "United States"). The flag
    // emoji is deliberately not used for display: Windows fonts carry no glyphs for regional
    // indicators, so the icons come from :/flags/<id>.svg instead.
    static QString displayName(const QString& locationId);

    // The compact label the location tab uses ("US", "HK/TW", "UK"). The flag icon already
    // carries the country's identity, so the tab text can be the code; the full name lives in
    // the tab's tooltip. Empty for the two specials (Favorite/Other) -- the ui layer owns their
    // translated labels, as it does for displayName().
    static QString shortName(const QString& locationId);

    // Maps an inferred ISO country code onto a location item id. HK folds into TW (one merged
    // item); every country the catalog has no item for (FR, CA, UNKNOWN, ...) folds into OTHER.
    static QString locationIdForCountryCode(const QString& countryCode);

    // Inference entry point for a node's display name.
    static QString inferredLocationIdForNodeName(const QString& name);

    // The membership a node has right now: override if set, else inference from the name.
    static QString resolvedLocationId(const VmessItem& server);
    // Same decision for callers that hold a table row (display name + carried override) rather
    // than the full item; the VmessItem overload delegates here so there is exactly one policy.
    static QString resolvedLocationId(const QString& locationOverride, const QString& displayName);

    // An override is only meaningful if it names a real item; a stale value (catalog changed,
    // hand-edited state file) falls back to inference rather than hiding the node from every item.
    static QString normalizedOverride(const QString& locationOverride);
};
