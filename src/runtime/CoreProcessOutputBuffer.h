#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class CoreProcessOutputBuffer {
public:
    enum class Channel {
        StandardOutput,
        StandardError
    };

    using OutputCallback = std::function<void(const QString&)>;

    void append(Channel channel, const QByteArray& payload, const OutputCallback& outputReceived);
    void flush(bool flushPartialLines, const OutputCallback& outputReceived);
    void clear();

private:
    void emitCompleteLines(QString& buffer, const OutputCallback& outputReceived);
    void flushBuffer(QString& buffer, bool flushPartialLines, const OutputCallback& outputReceived);

    // Holds raw bytes until a complete UTF-8 sequence has arrived, so a
    // multi-byte character split across readAll() chunks decodes correctly.
    QByteArray standardOutputRaw_;
    QByteArray standardErrorRaw_;
    QString standardOutput_;
    QString standardError_;
};
