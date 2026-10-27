#pragma once

#include <QString>

namespace SubscriptionYamlQuoteStateMachine {

// Where the quoted scalar opened at `quoteStart` ends, and whether it closes at all.
//
// A quote does not end the scalar when it is backslash-escaped (double quotes only --
// YAML gives single-quoted text no backslash escapes) or when it is doubled (YAML folds
// `''` into one literal quote). Both rules, and the "an unterminated scalar runs to the
// end of the text" consequence, are decided here and only here.
struct Scan {
    // Index of the closing quote, or -1 when the scalar never closes.
    int quoteEndedAt = -1;
    // False for anything that is not a quoted scalar: no opening quote, a position past
    // the end, or -- when `requireEnd` is set -- a closing quote that is not followed
    // only by whitespace.
    bool ok = false;
};

struct Options {
    // Refuse a scan whose closing quote has anything but whitespace after it.
    //
    // This is the whole-value read: "the text is that quoted scalar and nothing else".
    // Comment splitting leaves it off, because it looks at a scalar embedded in a
    // longer line.
    //
    // With `requireDelimiterAfter` also set, a flow delimiter counts as the end of the
    // value: `{name: "a"}` closes at the quote and the `}` is where the value stopped,
    // so the two rules do not contradict each other.
    bool requireEnd = false;

    // Refuse a scan whose closing quote is not followed by a flow delimiter (`,` `}` `]`)
    // or the end of the text, whitespace allowed between.
    //
    // This is the flow read's rule. A closing quote followed by `b:` is not a value that
    // ends there -- it is the fragment `{name: "a" b: c}`, one bad entry -- and accepting
    // the scalar would hand back a node whose remaining fields were silently dropped.
    //
    // On its own it still accepts a quote that is the last thing in the text, which is
    // why the flow read sets `requireEnd` as well: `{name: "a" b: c` with the closing
    // brace missing ends at the text, passes this rule, and drops `b: c` all the same.
    bool requireDelimiterAfter = false;
};

// The rules:
//   - only double-quoted text carries backslash escapes; `\` consumes the next character.
//   - a doubled quote is one literal quote and does not close the scalar.
//   - an unclosed scalar reports `quoteEndedAt == -1` and `ok == false`, which callers
//     must treat as "no boundary found" rather than as "the scalar ends at the end".
Scan scanQuotedScalar(const QString& text, int quoteStart, const Options& options = {});

} // namespace SubscriptionYamlQuoteStateMachine
