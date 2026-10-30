#include "ui/mainwindow/LocationTabBar.h"

#include <QCursor>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionTab>
#include <QStylePainter>

#include "ui/theme/AppTheme.h"

LocationTabBar::LocationTabBar(QWidget* parent)
    : QTabBar(parent)
{
}

void LocationTabBar::setLocationFlag(int index, const QPixmap& flag)
{
    if (flag.isNull()) {
        flags_.remove(index);
    } else {
        flags_.insert(index, flag);
    }
    update();
}

void LocationTabBar::clearLocationFlags()
{
    if (flags_.isEmpty()) {
        return;
    }
    flags_.clear();
    update();
}

void LocationTabBar::mousePressEvent(QMouseEvent* event)
{
    // The pin's bookkeeping half: remember that this press belongs to the last tab, so the move
    // handler can refuse to start a drag for it. The press itself is forwarded -- clicking the
    // pinned tab still selects it, and a right-button press (context menu) is not a drag either.
    pressedOnPinnedLastTab_ = event != nullptr
        && event->button() == Qt::LeftButton
        && isMovable()
        && tabAt(event->pos()) == count() - 1;
    QTabBar::mousePressEvent(event);
}

void LocationTabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (pressedOnPinnedLastTab_) {
        // The pin's enforcement half. QTabBar starts a movable drag from the move handler once
        // the press position is far enough away; never forwarding the moves of a pinned-tab
        // press means that code never runs, and the tab stays where the rebuild put it.
        return;
    }
    QTabBar::mouseMoveEvent(event);
}

void LocationTabBar::mouseReleaseEvent(QMouseEvent* event)
{
    pressedOnPinnedLastTab_ = false;
    QTabBar::mouseReleaseEvent(event);
}

void LocationTabBar::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    if (flags_.isEmpty()) {
        // Plain subscription mode: nothing here knows better than the base class.
        QTabBar::paintEvent(event);
        return;
    }

    // Location mode paints the tabs itself: the shape (the stylesheet's selected look, through
    // CE_TabBarTabShape so every QSS rule still applies) plus one flag+label unit drawn as a
    // single token. The base class is skipped entirely because its label pass has a fixed
    // 11-12px icon/text gap that no property moves, and it would paint the label dead center
    // where the flag needs to be. The bar itself stays backgroundless: the mode's accentSoft
    // wash is the container's QSS background (ServerWorkspaceWidget::setGroupByLocationMode),
    // and this bar lets it show through behind and between the tabs.
    QStylePainter painter(this);

    for (int index = 0; index < count(); ++index) {
        QStyleOptionTab option;
        initStyleOption(&option, index);

        painter.drawControl(QStyle::CE_TabBarTabShape, option);

        const auto flagIt = flags_.constFind(index);
        const QString label = option.text;
        if (label.trimmed().isEmpty()) {
            continue;
        }

        constexpr int kFlagSize = 12;         // the spec's fixed 12x12
        constexpr int kFlagLabelSpacing = 2;  // the spec's 2px

        const QFontMetrics metrics(option.fontMetrics);
        const int labelWidth = metrics.horizontalAdvance(label);
        const int unitWidth = kFlagSize + kFlagLabelSpacing + labelWidth;
        const int unitLeft = option.rect.left() + (option.rect.width() - unitWidth) / 2;
        const int unitTop = option.rect.top() + (option.rect.height() - metrics.height()) / 2;

        // The text colors match the subscription tabs' stylesheet exactly: muted for the resting
        // state, the full text colour on hover, and the navigation-selected tint when selected.
        // The option palette was tried first and does not carry the stylesheet's color rules, so
        // the colors come from the theme palette -- the same tokens the QSS resolves.
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = !selected && underMouse()
            && tabRect(index).contains(mapFromGlobal(QCursor::pos()));
        const QColor labelColor = selected
            ? AppTheme::navigationSelectedTextColor()
            : hovered
                ? AppTheme::iconColor(true)
                : AppTheme::mutedTextColor();
        painter.setFont(font());
        painter.setPen(QPen(labelColor, 0));

        if (flagIt != flags_.constEnd()) {
            painter.drawPixmap(
                QRect(unitLeft, option.rect.top() + (option.rect.height() - kFlagSize) / 2,
                      kFlagSize, kFlagSize),
                flagIt.value(),
                flagIt.value().rect());
        }
        painter.drawText(
            QRect(unitLeft + kFlagSize + kFlagLabelSpacing,
                  unitTop,
                  option.rect.right() - (unitLeft + kFlagSize + kFlagLabelSpacing) + 1,
                  metrics.height()),
            Qt::AlignLeft | Qt::AlignVCenter,
            label);
    }
}
