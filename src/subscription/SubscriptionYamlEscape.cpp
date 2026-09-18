#include "subscription/SubscriptionYamlEscape.h"

namespace {

constexpr int kHexEscapeLength = 4;
constexpr int kLongHexEscapeLength = 8;
constexpr int kMaxCodePoint = 0x10FFFF;

bool isHexDigit(const QChar ch)
{
    const ushort value = ch.unicode();
    return (value >= '0' && value <= '9')
        || (value >= 'a' && value <= 'f')
        || (value >= 'A' && value <= 'F');
}

int hexDigitValue(const QChar ch)
{
    const ushort value = ch.unicode();
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool readHexCodePoint(const QString& text, int start, int length, uint* codePoint)
{
    if (codePoint == nullptr || start + length > text.size()) {
        return false;
    }

    uint value = 0;
    for (int index = 0; index < length; ++index) {
        const QChar ch = text.at(start + index);
        if (!isHexDigit(ch)) {
            return false;
        }
        value = (value << 4) | static_cast<uint>(hexDigitValue(ch));
    }

    *codePoint = value;
    return true;
}

bool isHighSurrogate(const uint codePoint)
{
    return codePoint >= 0xD800 && codePoint <= 0xDBFF;
}

bool isLowSurrogate(const uint codePoint)
{
    return codePoint >= 0xDC00 && codePoint <= 0xDFFF;
}

// Decodes a leading "\uXXXX\uXXXX" surrogate-pair escape into the equivalent
// single astral code point. On failure the caller falls back to decoding the
// two "\uXXXX" units separately, which yields the same UTF-16 sequence.
bool readSurrogatePairCodePoint(const QString& text, int escapeStart, uint* codePoint)
{
    // Full pattern "\uXXXX\uXXXX" spans 2 * (1 + kHexEscapeLength) chars.
    if (codePoint == nullptr
        || escapeStart + 2 * (kHexEscapeLength + 2) > text.size()) {
        return false;
    }
    if (text.at(escapeStart) != QChar('\\') || text.at(escapeStart + 1) != QChar('u')) {
        return false;
    }
    if (text.at(escapeStart + kHexEscapeLength + 2) != QChar('\\')
        || text.at(escapeStart + kHexEscapeLength + 3) != QChar('u')) {
        return false;
    }

    uint high = 0;
    uint low = 0;
    if (!readHexCodePoint(text, escapeStart + 2, kHexEscapeLength, &high)
        || !readHexCodePoint(text, escapeStart + kHexEscapeLength + 4, kHexEscapeLength, &low)) {
        return false;
    }
    if (!isHighSurrogate(high) || !isLowSurrogate(low)) {
        return false;
    }

    *codePoint = 0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00);
    return true;
}

} // namespace

QString SubscriptionYamlEscape::unescape(const QString& text)
{
    if (!text.contains(QChar('\\'))) {
        return text;
    }

    QString result;
    result.reserve(text.size());

    int position = 0;
    while (position < text.size()) {
        const QChar current = text.at(position);
        if (current != QChar('\\') || position + 1 >= text.size()) {
            result.append(current);
            ++position;
            continue;
        }

        const QChar escape = text.at(position + 1);
        switch (escape.unicode()) {
        case '0':
            result.append(QChar(0x00));
            position += 2;
            continue;
        case 'a':
            result.append(QChar(0x07));
            position += 2;
            continue;
        case 'b':
            result.append(QChar(0x08));
            position += 2;
            continue;
        case 't':
        case '\t':
            result.append(QChar(0x09));
            position += 2;
            continue;
        case 'n':
            result.append(QChar(0x0A));
            position += 2;
            continue;
        case 'v':
            result.append(QChar(0x0B));
            position += 2;
            continue;
        case 'f':
            result.append(QChar(0x0C));
            position += 2;
            continue;
        case 'r':
            result.append(QChar(0x0D));
            position += 2;
            continue;
        case 'e':
            result.append(QChar(0x1B));
            position += 2;
            continue;
        case ' ':
            result.append(QChar(' '));
            position += 2;
            continue;
        case '"':
            result.append(QChar('"'));
            position += 2;
            continue;
        case '/':
            result.append(QChar('/'));
            position += 2;
            continue;
        case '\\':
            result.append(QChar('\\'));
            position += 2;
            continue;
        case 'N':
            result.append(QChar(0x85));
            position += 2;
            continue;
        case '_':
            result.append(QChar(0xA0));
            position += 2;
            continue;
        case 'L':
            result.append(QChar(0x2028));
            position += 2;
            continue;
        case 'P':
            result.append(QChar(0x2029));
            position += 2;
            continue;
        case 'x': {
            uint codePoint = 0;
            if (readHexCodePoint(text, position + 2, 2, &codePoint)) {
                result.append(QChar(static_cast<ushort>(codePoint)));
                position += 4;
                continue;
            }
            break;
        }
        case 'u': {
            // "\uXXXX\uXXXX" encodes a single code point above the BMP.
            uint surrogatePair = 0;
            if (readSurrogatePairCodePoint(text, position, &surrogatePair)) {
                result.append(QString::fromUcs4(&surrogatePair, 1));
                position += 2 * (kHexEscapeLength + 2);
                continue;
            }

            uint codePoint = 0;
            if (readHexCodePoint(text, position + 2, kHexEscapeLength, &codePoint) && codePoint <= kMaxCodePoint) {
                result.append(QChar(static_cast<ushort>(codePoint)));
                position += kHexEscapeLength + 2;
                continue;
            }
            break;
        }
        case 'U': {
            uint codePoint = 0;
            if (readHexCodePoint(text, position + 2, kLongHexEscapeLength, &codePoint) && codePoint <= kMaxCodePoint) {
                result.append(QString::fromUcs4(&codePoint, 1));
                position += kLongHexEscapeLength + 2;
                continue;
            }
            break;
        }
        default:
            break;
        }

        // Unknown or malformed escape: keep the text verbatim so we never lose
        // information on names we do not understand.
        result.append(current);
        ++position;
    }

    return result;
}
