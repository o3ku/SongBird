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

    QString standardOutput_;
    QString standardError_;
};
