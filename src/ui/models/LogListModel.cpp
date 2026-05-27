#include "ui/models/LogListModel.h"

#include <QSet>
#include <QTextLayout>
#include <QTextOption>

namespace {
constexpr int kMaxLogLines = 500;
constexpr int kTrimBatch = 100;
constexpr int kMaxLogLineCharacters = 1536;
}

LogListModel::LogListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int LogListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : entries_.size();
}

QVariant LogListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries_.size()) {
        return {};
    }

    const LogEntry& entry = entries_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return entry.text;
    case LineCountRole:
        return entry.visualLineCount;
    default:
        return {};
    }
}

void LogListModel::setWrapConfiguration(const QFont& font, int textWidth)
{
    if (wrapFont_ == font && textWidth_ == textWidth) {
        return;
    }

    wrapFont_ = font;
    textWidth_ = textWidth;

    if (entries_.isEmpty()) {
        return;
    }

    const QModelIndex topLeft = index(0, 0);
    const QModelIndex bottomRight = index(entries_.size() - 1, 0);
    for (LogEntry& entry : entries_) {
        entry.visualLineCount = computeVisualLineCount(entry.text);
    }
    emit dataChanged(topLeft, bottomRight, {LineCountRole});
}

void LogListModel::appendLine(const QString& line)
{
    if (entries_.size() >= kMaxLogLines) {
        const int removeCount = qMin(
            entries_.size(),
            qMax(kTrimBatch, entries_.size() - kMaxLogLines + 1));
        beginRemoveRows(QModelIndex(), 0, removeCount - 1);
        entries_.erase(entries_.begin(), entries_.begin() + removeCount);
        if (entries_.capacity() > kMaxLogLines + kTrimBatch) {
            entries_.squeeze();
        }
        endRemoveRows();
    }

    LogEntry entry;
    entry.text = normalizeLine(line);
    entry.visualLineCount = computeVisualLineCount(entry.text);

    const int row = entries_.size();
    beginInsertRows(QModelIndex(), row, row);
    entries_.append(std::move(entry));
    endInsertRows();
}

void LogListModel::clear()
{
    if (entries_.isEmpty()) {
        return;
    }

    beginResetModel();
    entries_.clear();
    entries_.squeeze();
    endResetModel();
}

QStringList LogListModel::lines() const
{
    QStringList result;
    result.reserve(entries_.size());
    for (const LogEntry& entry : entries_) {
        result.append(entry.text);
    }
    return result;
}

QString LogListModel::normalizeLine(const QString& text)
{
    if (text.size() <= kMaxLogLineCharacters) {
        return text;
    }

    return text.left(kMaxLogLineCharacters) + QStringLiteral(" ... [truncated]");
}

int LogListModel::computeVisualLineCount(const QString& text) const
{
    if (textWidth_ <= 0 || text.isEmpty()) {
        return 1;
    }

    QTextLayout layout(text, wrapFont_);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();

    int lineCount = 0;
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(textWidth_);
        ++lineCount;
    }
    layout.endLayout();

    return qMax(1, lineCount);
}
