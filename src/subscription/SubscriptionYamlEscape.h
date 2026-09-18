#pragma once

#include <QString>

namespace SubscriptionYamlEscape {

// Decodes YAML escape sequences (\0 \a \b \t \n \v \f \r \e \" \/ \\ \N \_ \L \P
// \xNN \uNNNN \UNNNNNNNN and \uD83C\uDDE7 surrogate pairs) into real characters.
// Unknown or malformed escapes are kept verbatim. Only apply this to double-quoted
// scalars: per YAML, single-quoted scalars have no backslash escapes.
QString unescape(const QString& text);

} // namespace SubscriptionYamlEscape
