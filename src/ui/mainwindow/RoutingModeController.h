#pragma once

#include <functional>

#include <QList>
#include <QObject>

#include "domain/models/RoutingItem.h"

class QComboBox;

class RoutingModeController final : public QObject {
public:
    explicit RoutingModeController(
        QComboBox* routingModeCombo,
        std::function<void(const QString&)> routingModeSelected,
        QObject* parent = nullptr);

    void setup();
    void setRoutingOptions(const QList<RoutingItem>& routingItems, const QString& routingModeId);

private:
    static QString describeRoutingMode(const RoutingItem& item, int index);

    QComboBox* routingModeCombo_ = nullptr;
    std::function<void(const QString&)> routingModeSelected_;
};
