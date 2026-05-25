#include "ui/mainwindow/ToolbarWidgets.h"

#include <QAbstractItemView>
#include <QAction>
#include <QComboBox>
#include <QFontMetrics>
#include <QFrame>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionComboBox>
#include <QToolBar>
#include <QToolButton>
#include <QtMath>

#include "ui/theme/AppTheme.h"

namespace {

constexpr int ButtonHorizontalPadding = 8;
constexpr int ButtonBorderWidth = 1;

void paintChevron(QPainter& painter, const QRect& rect, bool enabled)
{
    if (!rect.isValid()) {
        return;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(AppTheme::iconColor(enabled)),
                        1.6,
                        Qt::SolidLine,
                        Qt::RoundCap,
                        Qt::RoundJoin));

    const QPointF center(rect.center().x(), rect.center().y() + 2.0);
    QPainterPath chevron;
    chevron.moveTo(center.x() - 4.0, center.y() - 1.0);
    chevron.lineTo(center.x(), center.y() + 2.5);
    chevron.lineTo(center.x() + 4.0, center.y() - 1.0);
    painter.drawPath(chevron);
}

class StyledComboBoxBase : public QComboBox {
public:
    explicit StyledComboBoxBase(QWidget* parent = nullptr)
        : QComboBox(parent)
    {
    }

    QSize sizeHint() const override
    {
        return stabilizedSizeHint(QComboBox::sizeHint());
    }

    QSize minimumSizeHint() const override
    {
        return stabilizedSizeHint(QComboBox::minimumSizeHint());
    }

protected:
    void paintChevronForOption(const QStyleOptionComboBox& option)
    {
        const QRect arrowRect = style()->subControlRect(
            QStyle::CC_ComboBox,
            &option,
            QStyle::SC_ComboBoxArrow,
            this);
        if (!arrowRect.isValid()) {
            return;
        }

        QPainter painter(this);
        paintChevron(painter, arrowRect, isEnabled());
    }

private:
    QSize stabilizedSizeHint(QSize hint) const
    {
        const int width = property("contentSizedWidth").toInt();
        if (width > 0) {
            hint.setWidth(width);
        }
        return hint;
    }
};

class StyledComboBox final : public StyledComboBoxBase {
public:
    explicit StyledComboBox(QWidget* parent = nullptr)
        : StyledComboBoxBase(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QComboBox::paintEvent(event);

        QStyleOptionComboBox option;
        initStyleOption(&option);
        paintChevronForOption(option);
    }
};

class StyledToolbarMenuButton final : public QToolButton {
public:
    explicit StyledToolbarMenuButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QToolButton::paintEvent(event);

        Q_UNUSED(event)

        if (menu() == nullptr || !property("toolbarMenuButton").toBool()) {
            return;
        }

        const QRect indicatorRect(width() - 16, ((height() - 10) / 2) - 1, 10, 10);
        QPainter painter(this);
        paintChevron(painter, indicatorRect, isEnabled());
    }
};

void syncActionButton(QToolButton* button, QAction* action)
{
    if (button == nullptr || action == nullptr) {
        return;
    }

    const auto syncState = [button, action]() {
        button->setText(action->text());
        button->setIcon(action->icon());
        button->setToolTip(action->toolTip().isEmpty() ? action->text() : action->toolTip());
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
        if (action->isCheckable()) {
            button->setChecked(action->isChecked());
        }
    };

    syncState();
    QObject::connect(action, &QAction::changed, button, syncState);
}

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

} // namespace

namespace ToolbarWidgets {

QIcon loadIcon(const QString& fileName)
{
    return AppTheme::themedSvgIcon(QStringLiteral(":/toolbar/%1").arg(fileName));
}

QComboBox* createStyledComboBox(QWidget* parent)
{
    return new StyledComboBox(parent);
}

void configureStyledComboBox(QComboBox* comboBox)
{
    if (comboBox == nullptr) {
        return;
    }

    comboBox->setProperty("styledChevron", true);
    auto* popupView = new QListView(comboBox);
    popupView->setObjectName(QStringLiteral("toolbarComboPopupView"));
    popupView->setMouseTracking(true);
    popupView->setUniformItemSizes(true);
    popupView->setSelectionRectVisible(false);
    popupView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    popupView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    popupView->setFrameShape(QFrame::NoFrame);
    comboBox->setView(popupView);
}

QToolButton* createMenuButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QMenu* menu)
{
    auto* button = new StyledToolbarMenuButton(toolBar);
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(icon);
    button->setProperty("toolbarMenuButton", true);
    button->setMenu(menu);
    button->setPopupMode(QToolButton::InstantPopup);
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    button->setFixedHeight(ControlHeight);
    toolBar->addWidget(button);
    return button;
}

QToolButton* createActionButton(
    QToolBar* toolBar,
    const QString& objectName,
    const QString& text,
    const QIcon& icon,
    QAction* action)
{
    auto* button = new QToolButton(toolBar);
    button->setObjectName(objectName);
    button->setText(text);
    button->setIcon(icon);
    button->setCheckable(action != nullptr && action->isCheckable());
    button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    button->setAutoRaise(true);
    button->setFixedHeight(ControlHeight);

    if (action != nullptr) {
        QObject::connect(button, &QToolButton::clicked, action, &QAction::trigger);
        syncActionButton(button, action);
    }

    toolBar->addWidget(button);
    return button;
}

QWidget* createSpacing(QWidget* parent, int width)
{
    auto* spacer = new QWidget(parent);
    spacer->setFixedWidth(width);
    spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    return spacer;
}

int textButtonWidth(const QWidget* widget, const QStringList& texts)
{
    if (widget == nullptr) {
        return 0;
    }

    int textWidth = 0;
    const QFontMetrics fontMetrics(widget->font());
    for (const QString& text : texts) {
        textWidth = qMax(textWidth, fontMetrics.horizontalAdvance(text));
    }

    return textWidth + (ButtonHorizontalPadding * 2) + (ButtonBorderWidth * 2);
}

void updateContentSizedComboBox(QComboBox* comboBox, int minimumCharacters)
{
    if (comboBox == nullptr) {
        return;
    }

    // Always set the policy so tests can verify it before the window is shown.
    comboBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Defer the actual width calculation until after the first show; font
    // metrics differ before the style has fully resolved.
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

} // namespace ToolbarWidgets
