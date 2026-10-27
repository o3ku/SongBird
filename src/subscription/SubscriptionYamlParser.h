#pragma once

#include <QJsonValue>
#include <QString>

namespace SubscriptionYamlParser {

QString scalar(const QString& text);
bool isListItem(const QString& line);
int indent(const QString& line);
// `malformedValue` (optional) is set when the text after the separator opens a quoted
// scalar that never closes -- the case where the rest of the node is sitting inside the
// value. The pair is still filled in (there is no better reading), but the caller must
// report the node: this is the "value ran to the end of the line" failure direction, and
// it has to be visible rather than inferred from a node that later lacks a field.
bool splitKeyValue(const QString& text, QString* key, QString* value, bool* malformedValue = nullptr);
QJsonValue parseValue(const QString& text);
// Position of the first "#" that starts a comment outside quotes, or -1.
//
// A quoted scalar may contain " #" -- `name: "Gemini #1"` is a value -- so a bare
// search for " #" truncates the line, and a cut that lands before the fields
// deciding a node is usable drops the node without a record.
int commentIndex(const QString& line);

// The line's text before that comment, or the line unchanged when it has none.
//
// This is what callers want, so the "-1 means no comment" check is made here once
// instead of at every call site, where forgetting it cuts a line to nothing.
QString stripComment(const QString& line);

} // namespace SubscriptionYamlParser
