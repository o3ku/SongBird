#pragma once

#include <QAbstractListModel>
#include <QFont>
#include <QStringList>
#include <QVector>

class LogListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum CustomRole {
        VisualLinesRole = Qt::UserRole + 1,
        LineCountRole,
    };

    explicit LogListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    void setWrapConfiguration(const QFont& font, int textWidth);
    void appendLine(const QString& line);
    void clear();
    QStringList lines() const;
    QStringList linesAt(const QModelIndexList& indexes) const;

private:
    struct LogEntry {
        QString text;
        QStringList visualLines;
    };

    QStringList computeVisualLines(const QString& text) const;

    QVector<LogEntry> entries_;
    QFont wrapFont_;
    int textWidth_ = 0;
};
