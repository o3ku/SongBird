#include "runtime/CoreProcessOutputBuffer.h"

#include <QRegularExpression>

#include <utility>

namespace {

constexpr qsizetype kMaxBufferedCharactersPerChannel = 32 * 1024;
const QRegularExpression kAnsiEscape(QStringLiteral("\x1b\\[[0-9;]*[a-zA-Z]"));

void emitLine(QString line, const CoreProcessOutputBuffer::OutputCallback& outputReceived)
{
    line.remove(kAnsiEscape);
    if (!line.isEmpty()) {
        outputReceived(line);
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
    pendingBuffer.append(QString::fromUtf8(payload));
    if (pendingBuffer.size() > kMaxBufferedCharactersPerChannel) {
        pendingBuffer = pendingBuffer.right(kMaxBufferedCharactersPerChannel);
    }
    emitCompleteLines(pendingBuffer, outputReceived);
}

void CoreProcessOutputBuffer::flush(bool flushPartialLines, const OutputCallback& outputReceived)
{
    if (!outputReceived) {
        clear();
        return;
    }

    flushBuffer(standardOutput_, flushPartialLines, outputReceived);
    flushBuffer(standardError_, flushPartialLines, outputReceived);
}

void CoreProcessOutputBuffer::clear()
{
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
