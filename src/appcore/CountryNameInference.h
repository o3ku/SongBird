#pragma once

#include <QString>

// Pure country-code inference from a node's display name. Lives in appcore/ because two front
// ends need the same answer: the main window's location-grouped server list and SongBirdAuto's
// country items. It used to be auto/AutoCountryInference (an auto/-only helper), and moving the
// pure half up -- rather than letting ui/ include auto/ -- is what keeps a front end from
// including another front end.
//
// The inference walks three levels, first match wins:
//   1. a regional-indicator flag emoji anywhere in the name ("🇺🇸" -> US)
//   2. an alias substring, Chinese or English, case-insensitive ("香港", "SINGAPORE")
//   3. a standalone 2-3 letter token ("JP" in "Tokyo 01 | JP")
//
// Anything else answers UNKNOWN. The result is a raw ISO code, not an item id: mapping HK->TW or
// "not in the catalog" -> OTHER is a policy decision that belongs to the caller's catalog.

namespace CountryNameInference {

QString unknownCountryCode();
QString inferCountryCodeFromNodeName(const QString& name);

} // namespace CountryNameInference
