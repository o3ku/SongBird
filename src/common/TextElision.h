#pragma once

#include <QFontMetrics>
#include <QString>

namespace TextElision {

// Elides text on the right to fit maximumWidth.
//
// Every label surface in the app shortens text with Qt::ElideRight; routing
// them through here keeps the elision mode (and therefore the trailing
// ellipsis users see) consistent when one of them changes. Width budgets stay
// at the call sites because each surface has its own layout constraints and
// changing one budget should not silently shift the others.
//
// maximumWidth is deliberately not guarded against non-positive values. Qt's
// elidedText() collapses to an ellipsis at width 0, which is the desired
// result inside a paint call that got a degenerate rect; callers that can run
// before the first layout pass must check the width themselves (see
// SongBirdAutoWindow::refreshLogStatusLabel).
inline QString elideRight(const QFontMetrics& fontMetrics, const QString& text, int maximumWidth)
{
    return fontMetrics.elidedText(text, Qt::ElideRight, maximumWidth);
}

} // namespace TextElision
