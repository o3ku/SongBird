#pragma once

#include <QRegularExpression>
#include <QSortFilterProxyModel>

class LogFilterProxyModel final : public QSortFilterProxyModel {
    Q_OBJECT

public:
    explicit LogFilterProxyModel(QObject* parent = nullptr);

    void setPattern(const QString& pattern, bool regexEnabled);
    bool hasActivePattern() const;
    bool hasValidPattern() const;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    QString pattern_;
    bool regexEnabled_ = false;
    bool validPattern_ = true;
    QRegularExpression expression_;
};
