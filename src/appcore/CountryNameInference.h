#pragma once

#include <QString>

// Pure country-code inference from a node's display name, shared by the main window's
// location-grouped server list and the runtime's location reporting. Keeping the pure half in
// appcore/ -- rather than under a front end -- is what keeps ui/ free of a lower layer's
// dependency direction.
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
