#include "subscription/SubscriptionYamlQuoteStateMachine.h"

namespace SubscriptionYamlQuoteStateMachine {

Scan scanQuotedScalar(const QString& text, int quoteStart, const Options& options)
{
    Scan scan;
    if (quoteStart < 0 || quoteStart >= text.size()) {
        return scan;
    }

    const QChar quote = text.at(quoteStart);
    if (quote != QChar('"') && quote != QChar('\'')) {
        return scan;
    }

    // Per YAML, only double-quoted scalars process backslash escapes; applying them to
    // single-quoted text would corrupt names such as 'C:\folder'.
    const bool escapes = quote == QChar('"');

    for (int position = quoteStart + 1; position < text.size(); ++position) {
        const QChar current = text.at(position);
        if (escapes && current == QChar('\\')) {
            ++position;
            continue;
        }
        if (current != quote) {
            continue;
        }
        // A doubled quote folds into one literal quote rather than closing the scalar.
        if (position + 1 < text.size() && text.at(position + 1) == quote) {
            ++position;
            continue;
        }

        if (options.requireEnd || options.requireDelimiterAfter) {
            int rest = position + 1;
            while (rest < text.size() && text.at(rest).isSpace()) {
                ++rest;
            }
            const bool atEnd = rest >= text.size();
            // The two rules are asked of the same character and they do not mean the
            // same thing: `requireEnd` is "nothing else is left in this value" and
            // `requireDelimiterAfter` is "the entry continues here". A flow read wants
            // both, and asking `requireEnd` alone would then reject `{name: "a"}` --
            // the `}` is not whitespace, but it is where the value ends.
            const bool followedByDelimiter = !atEnd
                && (text.at(rest) == QChar(',') || text.at(rest) == QChar('}')
                    || text.at(rest) == QChar(']'));
            const bool delimiterEndsTheValue = options.requireDelimiterAfter && followedByDelimiter;
            if (options.requireEnd && !atEnd && !delimiterEndsTheValue) {
                return scan;
            }
            if (options.requireDelimiterAfter && !atEnd && !followedByDelimiter) {
                return scan;
            }
        }

        scan.quoteEndedAt = position;
        scan.ok = true;
        return scan;
    }

    return scan;
}

} // namespace SubscriptionYamlQuoteStateMachine
