#include "ui/mainwindow/RoutingModeController.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QSignalBlocker>

RoutingModeController::RoutingModeController(
    QComboBox* routingModeCombo,
    std::function<void(const QString&)> routingModeSelected,
    QObject* parent)
    : QObject(parent)
    , routingModeCombo_(routingModeCombo)
    , routingModeSelected_(std::move(routingModeSelected))
{
}

void RoutingModeController::setup()
{
    if (routingModeCombo_ == nullptr) {
        return;
    }

    QObject::connect(routingModeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (routingModeCombo_ == nullptr || index < 0) {
            return;
        }

        routingModeSelected_(routingModeCombo_->itemData(index).toString());
    });
}

void RoutingModeController::setRoutingOptions(const QList<RoutingItem>& routingItems, const QString& routingModeId)
{
    if (routingModeCombo_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(routingModeCombo_);
    routingModeCombo_->clear();

    for (int index = 0; index < routingItems.size(); ++index) {
        routingModeCombo_->addItem(describeRoutingMode(routingItems.at(index), index), routingItems.at(index).id);
    }

    const int comboIndex = routingModeCombo_->findData(routingModeId);
    routingModeCombo_->setCurrentIndex(comboIndex >= 0 ? comboIndex : (routingModeCombo_->count() > 0 ? 0 : -1));
}

QString RoutingModeController::describeRoutingMode(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    return QCoreApplication::translate("MainWindow", "Routing %1").arg(index + 1);
}
