#include "runtime/CoreProcessOutputBuffer.h"

#include <QRegularExpression>
#include <QtGlobal>

#include <utility>

namespace {

constexpr qsizetype kMaxBufferedCharactersPerChannel = 32 * 1024;
// The raw byte staging buffer only ever holds a partial multi-byte sequence
// (at most 3 bytes) in normal operation; this bound just stops garbage input
// that never forms valid UTF-8 from accumulating forever.
constexpr qsizetype kMaxBufferedRawBytesPerChannel = 4 * 1024;
const QRegularExpression kAnsiEscape(QStringLiteral("\x1b\\[[0-9;]*[a-zA-Z]"));

void emitLine(QString line, const CoreProcessOutputBuffer::OutputCallback& outputReceived)
{
    line.remove(kAnsiEscape);
    if (!line.isEmpty()) {
        outputReceived(line);
    }
}

qsizetype completeUtf8PrefixLength(const QByteArray& buffer)
{
    const qsizetype size = buffer.size();
    if (size == 0) {
        return 0;
    }

    // Find the lead byte of the last sequence within the trailing 4 bytes.
    const qsizetype windowStart = size - 4 > 0 ? size - 4 : 0;
    qsizetype leadIndex = -1;
    for (qsizetype index = size - 1; index >= windowStart; --index) {
        const auto byte = static_cast<unsigned char>(buffer.at(static_cast<int>(index)));
        if ((byte & 0xC0) != 0x80) {
            leadIndex = index;
            break;
        }
    }

    if (leadIndex < 0) {
        // The whole trailing window consists of continuation bytes: malformed
        // input, nothing to wait for.
        return size;
    }

    const auto lead = static_cast<unsigned char>(buffer.at(static_cast<int>(leadIndex)));
    qsizetype sequenceLength = 1;
    if ((lead & 0xE0) == 0xC0) {
        sequenceLength = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        sequenceLength = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        sequenceLength = 4;
    } else {
        // Invalid lead byte: treat the buffer as complete and let the decoder
        // emit replacement characters for the garbage.
        return size;
    }

    if (leadIndex + sequenceLength <= size) {
        return size;
    }
    return leadIndex;
}

QString decodeUtf8Chunk(QByteArray& rawBuffer, const QByteArray& payload)
{
    rawBuffer.append(payload);
    if (rawBuffer.size() > kMaxBufferedRawBytesPerChannel) {
        // Garbage that never forms valid UTF-8: decode everything now so the
        // buffer cannot grow unbounded (invalid bytes become U+FFFD).
        const QString decoded = QString::fromUtf8(rawBuffer.constData(), rawBuffer.size());
        rawBuffer.clear();
        return decoded;
    }

    // Decode only up to the last complete UTF-8 sequence; a multi-byte
    // character split across readAll() chunks stays buffered for the next
    // append instead of turning into U+FFFD.
    const qsizetype completeLength = completeUtf8PrefixLength(rawBuffer);
    QString decoded;
    if (completeLength > 0) {
        decoded = QString::fromUtf8(rawBuffer.constData(), completeLength);
        rawBuffer.remove(0, static_cast<int>(completeLength));
    }
    return decoded;
}

QString decodeBufferedBytes(QByteArray& rawBuffer)
{
    if (rawBuffer.isEmpty()) {
        return {};
    }
    const QString decoded = QString::fromUtf8(rawBuffer.constData(), rawBuffer.size());
    rawBuffer.clear();
    return decoded;
}

void trimToCharacterLimit(QString& buffer, qsizetype limit)
{
    if (buffer.size() <= limit) {
        return;
    }
    buffer = buffer.right(limit);
    // Avoid starting the retained text on a low surrogate, which would split
    // a surrogate pair at the truncation point.
    // at(0) rather than front(): front() returns a mutable QCharRef in Qt 5,
    // which does not expose isLowSurrogate().
    if (!buffer.isEmpty() && buffer.at(0).isLowSurrogate()) {
        buffer.remove(0, 1);
    }
}

} // namespace

void CoreProcessOutputBuffer::append(
    Channel channel,
    const QByteArray& payload,
    const OutputCallback& outputReceived)
{
    if (payload.isEmpty() || !outputReceived) {
        return;
    }

    QString& pendingBuffer = channel == Channel::StandardOutput
        ? standardOutput_
        : standardError_;
    QByteArray& pendingRaw = channel == Channel::StandardOutput
        ? standardOutputRaw_
        : standardErrorRaw_;
    pendingBuffer.append(decodeUtf8Chunk(pendingRaw, payload));
    trimToCharacterLimit(pendingBuffer, kMaxBufferedCharactersPerChannel);
    emitCompleteLines(pendingBuffer, outputReceived);
}

void CoreProcessOutputBuffer::flush(bool flushPartialLines, const OutputCallback& outputReceived)
{
    // Decode any still-buffered raw bytes so nothing is lost at teardown.
    standardOutput_.append(decodeBufferedBytes(standardOutputRaw_));
    standardError_.append(decodeBufferedBytes(standardErrorRaw_));

    if (!outputReceived) {
        clear();
        return;
    }

    flushBuffer(standardOutput_, flushPartialLines, outputReceived);
    flushBuffer(standardError_, flushPartialLines, outputReceived);
}

void CoreProcessOutputBuffer::clear()
{
    standardOutputRaw_.clear();
    standardErrorRaw_.clear();
    standardOutput_.clear();
    standardError_.clear();
    standardOutput_.squeeze();
    standardError_.squeeze();
}

void CoreProcessOutputBuffer::emitCompleteLines(QString& buffer, const OutputCallback& outputReceived)
{
    qsizetype newlineIndex = buffer.indexOf(QChar('\n'));
    while (newlineIndex >= 0) {
        QString line = buffer.left(newlineIndex);
        if (line.endsWith(QChar('\r'))) {
            line.chop(1);
        }

        buffer.remove(0, newlineIndex + 1);
        emitLine(line, outputReceived);
        newlineIndex = buffer.indexOf(QChar('\n'));
    }
    if (buffer.isEmpty()) {
        buffer.squeeze();
    }
}

void CoreProcessOutputBuffer::flushBuffer(
    QString& buffer,
    bool flushPartialLines,
    const OutputCallback& outputReceived)
{
    if (buffer.isEmpty()) {
        return;
    }

    if (!flushPartialLines && !buffer.contains(QChar('\n'))) {
        return;
    }

    QString remaining = std::move(buffer);
    buffer.clear();
    remaining.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    remaining.replace(QChar('\r'), QChar('\n'));

    qsizetype start = 0;
    qsizetype newlineIndex = remaining.indexOf(QChar('\n'));
    while (newlineIndex >= 0) {
        const QString line = remaining.mid(start, newlineIndex - start);
        if (!line.isEmpty()) {
            emitLine(line, outputReceived);
        }
        start = newlineIndex + 1;
        newlineIndex = remaining.indexOf(QChar('\n'), start);
    }
    if (start < remaining.size()) {
        const QString line = remaining.mid(start);
        emitLine(line, outputReceived);
    }
}
