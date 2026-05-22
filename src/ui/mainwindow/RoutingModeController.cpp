#include "ui/mainwindow/RoutingModeController.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFontMetrics>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QWidget>

namespace {

constexpr int RoutingComboMinimumCharacters = 12;

int textControlMinimumWidth(const QWidget* widget, const QString& text, int minimumCharacters, int chromeWidth)
{
    if (widget == nullptr) {
        return 0;
    }

    const QFontMetrics metrics(widget->font());
    const int textWidth = metrics.horizontalAdvance(text);
    const int characterWidth = metrics.horizontalAdvance(QString(minimumCharacters, QLatin1Char('M')));
    return qMax(textWidth, characterWidth) + chromeWidth;
}

void updateContentSizedComboBox(QComboBox* comboBox, int minimumCharacters)
{
    if (comboBox == nullptr) {
        return;
    }

    comboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    if (!comboBox->window()->isVisible()) {
        return;
    }

    QString widestText = comboBox->currentText();
    for (int index = 0; index < comboBox->count(); ++index) {
        if (comboBox->fontMetrics().horizontalAdvance(comboBox->itemText(index))
            > comboBox->fontMetrics().horizontalAdvance(widestText)) {
            widestText = comboBox->itemText(index);
        }
    }

    const int comboWidth = textControlMinimumWidth(comboBox, widestText, minimumCharacters, 16);
    comboBox->setProperty("contentSizedWidth", comboWidth);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    comboBox->setFixedWidth(comboWidth);
    comboBox->updateGeometry();
}

} // namespace

RoutingModeController::RoutingModeController(
    QComboBox* routingModeCombo,
    std::function<void(int)> routingModeSelected,
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

        routingModeSelected_(routingModeCombo_->itemData(index).toInt());
    });
}

void RoutingModeController::setRoutingOptions(const QList<RoutingItem>& routingItems, int routingIndex)
{
    if (routingModeCombo_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(routingModeCombo_);
    routingModeCombo_->clear();

    for (int index = 0; index < routingItems.size(); ++index) {
        routingModeCombo_->addItem(describeRoutingMode(routingItems.at(index), index), index);
    }

    const int selectedMode = routingItems.isEmpty()
        ? -1
        : qBound(0, routingIndex, routingItems.size() - 1);
    const int comboIndex = routingModeCombo_->findData(selectedMode);
    routingModeCombo_->setCurrentIndex(comboIndex >= 0 ? comboIndex : (routingModeCombo_->count() > 0 ? 0 : -1));
    updateContentSizedComboBox(routingModeCombo_, RoutingComboMinimumCharacters);
}

QString RoutingModeController::describeRoutingMode(const RoutingItem& item, int index)
{
    const QString remarks = item.remarks.trimmed();
    if (!remarks.isEmpty()) {
        return remarks;
    }

    return QCoreApplication::translate("MainWindow", "Routing %1").arg(index + 1);
}
