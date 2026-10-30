#pragma once

#include <QHash>
#include <QPixmap>
#include <QTabBar>

// A QTabBar that paints the location items' flag icons itself, 2px from the label.
//
// Why a subclass at all: the icon-to-text distance inside a stylesheet-styled QTabBar tab is a
// fixed QStyleSheetStyle layout constant (measured 11-12px regardless of icon size), and no QSS
// property, icon size or style hint moves it. The location tabs want a tight flag-label unit --
// flag 12x12, gap 2px -- and the only place that distance exists is the paint call.
//
// How: the bar is used exactly as before (labels, tabData, selection, QSS), but the location
// rebuild does NOT call setTabIcon(). Instead rebuildLocationTabs() hands each tab's flag in
// through setLocationFlag(), and paintEvent() draws it after the base class has painted the
// tab chrome and text: the flag is drawn over the tab's own left padding, immediately left of
// the text rect QStyleOptionTab reports, so flag+label read as one token. This keeps the
// selected/hover styling entirely in the stylesheet (only the text is tinted by it, and the
// flag is a flag), and keeps subscription mode untouched: with no flags registered the paint
// pass draws nothing and the bar behaves exactly like a plain QTabBar.
//
// The bar also pins its last tab. QTabBar's movability is a bar-wide switch, so a per-tab pin
// can only be enforced by the subclass: a left press on the last tab is remembered and its
// mouse moves are never forwarded -- the move handler is the only place QTabBar starts a
// drag, so the pinned tab (Ungrouped in the subscription view: not a subscription, no place
// in the persisted order, always rendered last by the rebuild) can be clicked and selected
// but never dragged. The other half of the pin -- a subscription dropped *past* the pinned
// tab -- is restored by SubscriptionViewController when the drag commits.
class LocationTabBar final : public QTabBar {
    Q_OBJECT

public:
    explicit LocationTabBar(QWidget* parent = nullptr);

    // Registers the flag drawn left of tab `index`'s label. A null pixmap clears it; clear all
    // of them before the bar is reused for subscriptions (clearLocationFlags).
    void setLocationFlag(int index, const QPixmap& flag);
    void clearLocationFlags();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QHash<int, QPixmap> flags_;
    // True between a left press on the last tab and its release; see the class comment.
    bool pressedOnPinnedLastTab_ = false;
};
