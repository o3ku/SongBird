#include "ui/models/LogListModel.h"

#include <QSet>
#include <QTextLayout>
#include <QTextOption>

namespace {
constexpr int kMaxLogLines = 5000;
constexpr int kTrimBatch = 500;
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
    case VisualLinesRole:
        return entry.visualLines;
    case LineCountRole:
        return entry.visualLines.size();
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

    emit layoutAboutToBeChanged();
    for (LogEntry& entry : entries_) {
        entry.visualLines = computeVisualLines(entry.text);
    }
    emit layoutChanged();
}

void LogListModel::appendLine(const QString& line)
{
    if (entries_.size() >= kMaxLogLines + kTrimBatch) {
        const int removeCount = entries_.size() - kMaxLogLines;
        beginRemoveRows(QModelIndex(), 0, removeCount - 1);
        entries_.erase(entries_.begin(), entries_.begin() + removeCount);
        endRemoveRows();
    }

    LogEntry entry;
    entry.text = line;
    entry.visualLines = computeVisualLines(line);

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

QStringList LogListModel::linesAt(const QModelIndexList& indexes) const
{
    QStringList values;
    QSet<int> seenRows;
    for (const QModelIndex& index : indexes) {
        if (!index.isValid() || seenRows.contains(index.row())) {
            continue;
        }

        seenRows.insert(index.row());
        if (index.row() >= 0 && index.row() < entries_.size()) {
            values.append(entries_.at(index.row()).text);
        }
    }

    return values;
}

QStringList LogListModel::computeVisualLines(const QString& text) const
{
    if (textWidth_ <= 0 || text.isEmpty()) {
        return {text};
    }

    QTextLayout layout(text, wrapFont_);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();

    QStringList lines;
    forever {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(textWidth_);
        lines.append(text.mid(line.textStart(), line.textLength()));
    }
    layout.endLayout();

    if (lines.isEmpty()) {
        lines.append(text);
    }

    return lines;
}
