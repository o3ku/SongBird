#include <QtTest>

#include "appcore/LocationCatalog.h"

using LocationItem = LocationCatalog::Item;

namespace {

int indexOfItem(const QString& locationId)
{
    const QList<LocationItem> items = LocationCatalog::items();
    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).id == locationId) {
            return index;
        }
    }
    return -1;
}

VmessItem serverWithRemark(const QString& remark)
{
    VmessItem server;
    server.indexId = QStringLiteral("id-%1").arg(remark);
    server.remarks = remark;
    server.address = QStringLiteral("example.com");
    server.port = 443;
    return server;
}

} // namespace

// The catalog is the single decision table for the location-grouped server view. Three families of
// behaviour are pinned here, and each can fail on its own:
//
//   * the item list itself: fixed membership, fixed order, Favorite first and Other last -- the
//     tab bar renders this list as-is, so a reordered catalog is a reordered UI,
//   * the folds: HK belongs with TW, and anything the catalog has no item for is Other -- a node
//     that lands in the wrong item is invisible in the view the user asked for, and
//   * the resolution order: a manual override outranks the name inference, and a stale override
//     (an id the catalog no longer knows) falls back to inference rather than hiding the node.
class LocationCatalogTests : public QObject {
    Q_OBJECT

private slots:
    void itemsFollowFixedOrder();
    void hongKongFoldsIntoTaiwan();
    void unmappedCountriesFoldIntoOther();
    void inferenceCoversEmojiAliasAndToken();
    void overrideBeatsInference();
    void unknownOverrideFallsBackToInference();
    void favoriteIsNotAnOverrideValue();
    void resolvedLocationIdUsesDisplayName();
};

void LocationCatalogTests::itemsFollowFixedOrder()
{
    const QList<LocationItem> items = LocationCatalog::items();
    QCOMPARE(items.size(), 11);

    QCOMPARE(items.first().id, QStringLiteral("FAVORITE"));
    QCOMPARE(items.last().id, QStringLiteral("OTHER"));

    const char* expected[] = {"US", "SG", "TW", "JP", "KR", "GB", "DE", "RU", "AU"};
    for (int index = 0; index < 9; ++index) {
        QCOMPARE(items.at(index + 1).id, QString::fromLatin1(expected[index]));
    }

    QVERIFY(LocationCatalog::isKnownLocationId(QStringLiteral("us")));
    QVERIFY(LocationCatalog::isKnownLocationId(QStringLiteral(" OTHER ")));
    QVERIFY(!LocationCatalog::isKnownLocationId(QStringLiteral("FR")));
    QVERIFY(!LocationCatalog::isKnownLocationId(QString()));

    QVERIFY(LocationCatalog::isCountryLocationId(QStringLiteral("US")));
    QVERIFY(!LocationCatalog::isCountryLocationId(QStringLiteral("FAVORITE")));
    QVERIFY(!LocationCatalog::isCountryLocationId(QStringLiteral("OTHER")));
}

void LocationCatalogTests::hongKongFoldsIntoTaiwan()
{
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QStringLiteral("HK")), QStringLiteral("TW"));
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QStringLiteral("hk")), QStringLiteral("TW"));
    // The merged item is named for both halves, because a node inside it may be either.
    QCOMPARE(LocationCatalog::displayName(QStringLiteral("TW")), QStringLiteral("Hong Kong / Taiwan"));
    // The tab's compact spelling says both halves too, in three glyphs.
    QCOMPARE(LocationCatalog::shortName(QStringLiteral("TW")), QStringLiteral("HK/TW"));
    QCOMPARE(LocationCatalog::shortName(QStringLiteral("GB")), QStringLiteral("UK"));
    QCOMPARE(LocationCatalog::shortName(QStringLiteral("US")), QStringLiteral("US"));
    // The specials own their labels to the ui layer (translated strings, not catalog data).
    QVERIFY(LocationCatalog::shortName(QStringLiteral("FAVORITE")).isEmpty());
    QVERIFY(LocationCatalog::shortName(QStringLiteral("OTHER")).isEmpty());
}

void LocationCatalogTests::unmappedCountriesFoldIntoOther()
{
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QStringLiteral("FR")), QStringLiteral("OTHER"));
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QStringLiteral("CA")), QStringLiteral("OTHER"));
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QStringLiteral("UNKNOWN")), QStringLiteral("OTHER"));
    QCOMPARE(LocationCatalog::locationIdForCountryCode(QString()), QStringLiteral("OTHER"));
}

void LocationCatalogTests::inferenceCoversEmojiAliasAndToken()
{
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("🇺🇸 Los Angeles 01")), QStringLiteral("US"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("香港 IEPL 02")), QStringLiteral("TW"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("Tokyo Premium | JP 03")), QStringLiteral("JP"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("Singapore")), QStringLiteral("SG"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("伦敦")), QStringLiteral("OTHER"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("Random Node")), QStringLiteral("OTHER"));

    // City-only names: subscription providers name nodes after cities, and the city uniquely
    // names its country. The Korean/Japanese city words matter most because their scripts carry
    // no roman letters for the token pass to catch.
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("首尔 07")), QStringLiteral("KR"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("Seoul 08")), QStringLiteral("KR"));
    QCOMPARE(LocationCatalog::inferredLocationIdForNodeName(QStringLiteral("东京 09")), QStringLiteral("JP"));
}

void LocationCatalogTests::overrideBeatsInference()
{
    VmessItem server = serverWithRemark(QStringLiteral("🇺🇸 Los Angeles 01"));
    server.locationOverride = QStringLiteral("JP");
    QCOMPARE(LocationCatalog::resolvedLocationId(server), QStringLiteral("JP"));

    // Resetting is clearing the override, not writing a special value: the name takes over again.
    server.locationOverride.clear();
    QCOMPARE(LocationCatalog::resolvedLocationId(server), QStringLiteral("US"));
}

void LocationCatalogTests::unknownOverrideFallsBackToInference()
{
    VmessItem server = serverWithRemark(QStringLiteral("🇯🇵 Osaka 02"));
    server.locationOverride = QStringLiteral("XX");
    // A stale or hand-edited override must not hide the node from every item; the fallback is the
    // inference the override would have replaced.
    QCOMPARE(LocationCatalog::resolvedLocationId(server), QStringLiteral("JP"));
}

void LocationCatalogTests::favoriteIsNotAnOverrideValue()
{
    // Favorite has its own flag; conflating the two would make "clear the location" and
    // "unfavorite" the same write.
    QVERIFY(LocationCatalog::normalizedOverride(QStringLiteral("FAVORITE")).isEmpty());
    QCOMPARE(LocationCatalog::normalizedOverride(QStringLiteral("jp")), QStringLiteral("JP"));
    QCOMPARE(LocationCatalog::normalizedOverride(QStringLiteral("OTHER")), QStringLiteral("OTHER"));
    QVERIFY(LocationCatalog::normalizedOverride(QStringLiteral("FR")).isEmpty());
}

void LocationCatalogTests::resolvedLocationIdUsesDisplayName()
{
    // A node with no remarks is named by its address, and that name is what feeds the inference --
    // the same name the table shows, so the grouping and the visible name cannot disagree.
    VmessItem server;
    server.address = QStringLiteral("us-1.example.com");
    server.port = 443;
    QCOMPARE(LocationCatalog::resolvedLocationId(server), QStringLiteral("US"));
}

QTEST_MAIN(LocationCatalogTests)
#include "LocationCatalogTests.moc"
