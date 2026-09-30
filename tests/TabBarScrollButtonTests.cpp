#include <QtTest>

#include <QApplication>
#include <QSet>
#include <QTabBar>
#include <QToolButton>

#include "ui/theme/AppTheme.h"

namespace {

// An overflowing tab bar gets two QToolButtons from QTabBar itself, and QTabBar lays them out only
// 16px wide. QStyleSheetStyle paints the arrow into the style rule's contents rect, so a rule that
// adds horizontal padding to QToolButton collapses that rect at this width and the button renders as
// an empty box with no arrow at all. Counting the distinct colours of the rendered button is the
// cheapest way to tell the two apart: an empty box only ever holds the fill and the 1px border,
// while a drawn arrow adds at least one more colour on top of them.
int distinctColorCount(QToolButton* button)
{
    const QImage image = button->grab().toImage();
    QSet<QRgb> colors;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            colors.insert(image.pixel(x, y));
        }
    }
    return colors.size();
}

} // namespace

class TabBarScrollButtonTests : public QObject
{
    Q_OBJECT

private slots:
    void overflowedTabBarDrawsScrollButtonArrows();
};

void TabBarScrollButtonTests::overflowedTabBarDrawsScrollButtonArrows()
{
    const QStringList themes{AppTheme::lightThemeName(), AppTheme::darkThemeName()};
    for (const QString& themeName : themes) {
        AppTheme::applyApplicationTheme(*qApp, themeName);

        QWidget host;
        host.resize(360, 60);

        auto* bar = new QTabBar(&host);
        // Mirrors ServerWorkspaceWidget::subscriptionTabBar_, the tab bar users actually overflow.
        bar->setObjectName(QStringLiteral("subscriptionTabBar"));
        bar->setDocumentMode(true);
        bar->setDrawBase(false);
        bar->setExpanding(false);
        bar->setAutoHide(false);
        bar->setUsesScrollButtons(true);
        bar->resize(340, 30);
        for (int index = 0; index < 20; ++index) {
            bar->addTab(QStringLiteral("Subscription %1").arg(index));
        }

        host.show();
        QCoreApplication::processEvents();
        QTest::qWait(150);
        QCoreApplication::processEvents();

        const QList<QToolButton*> buttons = bar->findChildren<QToolButton*>();
        QCOMPARE(buttons.size(), 2);
        for (QToolButton* button : buttons) {
            QVERIFY2(button->isVisible(), qPrintable(themeName));
            QVERIFY2(button->arrowType() == Qt::LeftArrow || button->arrowType() == Qt::RightArrow,
                     qPrintable(themeName));
            QVERIFY2(distinctColorCount(button) >= 3, qPrintable(themeName));
        }
    }

    AppTheme::applyApplicationTheme(*qApp);
}

QTEST_MAIN(TabBarScrollButtonTests)

#include "TabBarScrollButtonTests.moc"
