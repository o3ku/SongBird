#include "ui/dialogs/RoutingBaseRouteCard.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QPainter>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionButton>

namespace {

constexpr int kBaseRouteCardMaxWidth = 220;
constexpr int kBaseRouteCardTextWidth = 210;
constexpr int kBaseRouteCardCollapsedWidth = 100;

QString elideText(const QString& value, const QFontMetrics& metrics, int width)
{
    const QString trimmed = value.trimmed();
    if (metrics.horizontalAdvance(trimmed) <= width) {
        return trimmed;
    }

    const QString ellipsis = QStringLiteral("...");
    int length = trimmed.size();
    while (length > 0) {
        const QString candidate = trimmed.left(length).trimmed() + ellipsis;
        if (metrics.horizontalAdvance(candidate) <= width) {
            return candidate;
        }
        --length;
    }

    return ellipsis;
}

QString compactRuleValues(const QStringList& values)
{
    QStringList normalized;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty()) {
            normalized.append(trimmed);
        }
    }
    return normalized.join(QStringLiteral(", "));
}

QString elideCardLine(const QString& value, const QFontMetrics& metrics)
{
    return elideText(value, metrics, kBaseRouteCardTextWidth);
}

QStringList routingRuleMatches(const RoutingRule& rule)
{
    QStringList matches;
    const QString port = rule.port.trimmed();
    if (!port.isEmpty()) {
        matches.append(QCoreApplication::translate("RoutingBaseRouteCard", "Port: %1").arg(port));
    }

    const QString network = rule.network.trimmed();
    if (!network.isEmpty()) {
        matches.append(QCoreApplication::translate("RoutingBaseRouteCard", "Network: %1").arg(network));
    }

    const QString protocols = compactRuleValues(rule.protocol);
    if (!protocols.isEmpty()) {
        matches.append(QCoreApplication::translate("RoutingBaseRouteCard", "Protocol: %1").arg(protocols));
    }

    const QString processes = compactRuleValues(rule.process);
    if (!processes.isEmpty()) {
        matches.append(QCoreApplication::translate("RoutingBaseRouteCard", "Process: %1").arg(processes));
    }

    const QString ips = compactRuleValues(rule.ip);
    if (!ips.isEmpty()) {
        matches.append(ips);
    }

    const QString domains = compactRuleValues(rule.domain);
    if (!domains.isEmpty()) {
        matches.append(domains);
    }

    matches.removeAll(QString());
    return matches;
}

QString routingRuleActionLabel(const RoutingRule& rule)
{
    const QString outbound = rule.outboundTag.trimmed().toLower();
    if (outbound == QStringLiteral("block")) {
        return QStringLiteral("BLOCK");
    }
    if (outbound == QStringLiteral("direct")) {
        return QStringLiteral("DIRECT");
    }
    if (outbound == QStringLiteral("proxy")) {
        return QStringLiteral("PROXY");
    }
    if (!outbound.isEmpty()) {
        return outbound.toUpper();
    }
    return QCoreApplication::translate("RoutingBaseRouteCard", "Rule");
}

void appendCardRuleLine(QStringList& lines, const RoutingRule& rule, const QFontMetrics* metrics = nullptr)
{
    const QStringList matches = routingRuleMatches(rule);
    if (matches.isEmpty()) {
        return;
    }

    const QString line = QStringLiteral("%1: %2").arg(routingRuleActionLabel(rule), matches.join(QStringLiteral(", ")));
    lines.append(metrics == nullptr ? line : elideCardLine(line, *metrics));
}

} // namespace

RoutingBaseRouteCardButton::RoutingBaseRouteCardButton(QWidget* parent)
    : QPushButton(parent)
{
    setProperty("routeTitleBold", true);
}

QSize RoutingBaseRouteCardButton::sizeHint() const
{
    return cardSizeHint();
}

QSize RoutingBaseRouteCardButton::minimumSizeHint() const
{
    return cardSizeHint();
}

void RoutingBaseRouteCardButton::paintEvent(QPaintEvent*)
{
    QPainter painter(this);

    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();
    style()->drawControl(QStyle::CE_PushButtonBevel, &option, &painter, this);

    const QRect textRect = rect().adjusted(8, 8, -8, -8);
    const QStringList lines = text().split(QChar('\n'));
    QFont normalFont = font();
    QFont boldFont = normalFont;
    boldFont.setBold(true);

    painter.setPen(palette().buttonText().color());
    int y = textRect.top();
    for (int index = 0; index < lines.size(); ++index) {
        painter.setFont(index == 0 ? boldFont : normalFont);
        const QFontMetrics metrics(painter.font());
        const int lineHeight = metrics.lineSpacing();
        if (y + lineHeight > textRect.bottom() + 1) {
            break;
        }
        painter.drawText(
            QRect(textRect.left(), y, textRect.width(), lineHeight),
            Qt::AlignLeft | Qt::AlignVCenter,
            elideText(lines.at(index), metrics, textRect.width()));
        y += lineHeight;
    }
}

QSize RoutingBaseRouteCardButton::cardSizeHint() const
{
    QSize hint = QPushButton::sizeHint();
    const QStringList lines = text().split(QChar('\n'));
    QFont normalFont = font();
    QFont boldFont = normalFont;
    boldFont.setBold(true);

    int textHeight = 0;
    for (int index = 0; index < lines.size(); ++index) {
        const QFontMetrics metrics(index == 0 ? boldFont : normalFont);
        textHeight += metrics.lineSpacing();
    }

    hint.setHeight(qMax(hint.height(), textHeight + 16));
    return hint;
}

QString RoutingBaseRouteCard::collapsedText(const RoutingItem& item, int index, const QFontMetrics& metrics)
{
    QStringList lines;
    const QString title = item.remarks.trimmed().isEmpty()
        ? QCoreApplication::translate("RoutingBaseRouteCard", "Route %1").arg(index + 1)
        : item.remarks.trimmed();
    lines.append(elideCardLine(title, metrics));
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }
        appendCardRuleLine(lines, rule, &metrics);
    }
    return lines.join(QChar('\n'));
}

QString RoutingBaseRouteCard::fullText(const RoutingItem& item, int index)
{
    QStringList lines;
    lines.append(item.remarks.trimmed().isEmpty()
        ? QCoreApplication::translate("RoutingBaseRouteCard", "Route %1").arg(index + 1)
        : item.remarks.trimmed());
    for (const RoutingRule& rule : item.rules) {
        if (!rule.enabled) {
            continue;
        }
        appendCardRuleLine(lines, rule);
    }
    return lines.join(QChar('\n'));
}

void RoutingBaseRouteCard::applyGeometry(QButtonGroup* buttonGroup, QHBoxLayout* layout)
{
    if (buttonGroup == nullptr) {
        return;
    }

    const int selectedId = buttonGroup->checkedId();
    for (QAbstractButton* button : buttonGroup->buttons()) {
        if (button == nullptr) {
            continue;
        }

        const bool selected = buttonGroup->id(button) == selectedId;
        if (selected) {
            button->setMinimumWidth(kBaseRouteCardMaxWidth);
            button->setMaximumWidth(QWIDGETSIZE_MAX);
            button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
            button->setText(button->property("fullText").toString());
        } else {
            button->setMinimumWidth(kBaseRouteCardCollapsedWidth);
            button->setMaximumWidth(kBaseRouteCardCollapsedWidth);
            button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            button->setText(button->property("collapsedText").toString());
        }

        if (layout != nullptr) {
            const int layoutIndex = layout->indexOf(button);
            if (layoutIndex >= 0) {
                layout->setStretch(layoutIndex, selected ? 1 : 0);
            }
        }
    }

    if (layout != nullptr) {
        layout->invalidate();
    }
}
