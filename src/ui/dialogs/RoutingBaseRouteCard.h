#pragma once

#include <QFontMetrics>
#include <QPushButton>

#include "domain/models/RoutingItem.h"

class QButtonGroup;
class QHBoxLayout;
class QPaintEvent;

class RoutingBaseRouteCardButton final : public QPushButton
{
public:
    explicit RoutingBaseRouteCardButton(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QSize cardSizeHint() const;
};

namespace RoutingBaseRouteCard {

QString collapsedText(const RoutingItem& item, int index, const QFontMetrics& metrics);
QString fullText(const RoutingItem& item, int index);
void applyGeometry(QButtonGroup* buttonGroup, QHBoxLayout* layout);

} // namespace RoutingBaseRouteCard
