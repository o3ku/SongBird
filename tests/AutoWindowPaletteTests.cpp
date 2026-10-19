#include <QtTest>

#include <QColor>
#include <QString>
#include <QStringList>

#include "auto/AutoWindowPalette.h"
#include "ui/theme/AppThemePalette.h"

// The SongBirdAuto window carries a palette of its own, deliberately: it is a small control panel
// with a blue accent where the main app uses green. These cases pin the parts of that decision
// that are easy to lose in a refactor.
//
// The colours themselves are not asserted. A test that hard-codes "#007aff" would just move the
// literal it was supposed to eliminate into a second file, and would have to be edited every time
// the design changes -- which teaches people to edit the test instead of reading it. What is
// worth pinning is that every field is present and usable, and that the two palettes stay
// distinct, because "make auto use the main palette" is the tempting simplification that would
// silently change how that window looks.
class AutoWindowPaletteTests : public QObject
{
    Q_OBJECT

private slots:
    void everyFieldIsANonEmptyColorValue();
    void onAccentIsUsableAsTextOnEveryFill();
    void autoPaletteIsNotTheMainPalette();
    void paletteIsStableAcrossCalls();
};

namespace {

// Every field of AutoThemePalette, listed by hand so adding a field without adding it here is a
// compile error in the initialiser rather than a quietly unchecked value.
QStringList autoPaletteFields()
{
    const AutoThemeInternal::AutoThemePalette& palette = AutoThemeInternal::currentPalette();
    return {
        palette.background,
        palette.text,
        palette.textMuted,
        palette.textHeader,
        palette.textStatus,
        palette.textSubtle,
        palette.surface,
        palette.control,
        palette.surfaceMuted,
        palette.surfaceHover,
        palette.surfaceDisabled,
        palette.border,
        palette.borderMuted,
        palette.accent,
        palette.accentPending,
        palette.accentToggle,
        palette.danger,
        palette.selection,
        palette.scrollbar,
        palette.scrollbarHover,
        AutoThemeInternal::onAccentColor(),
    };
}

bool looksLikeAColor(const QString& value)
{
    // #rrggbb, which is the only form this codebase stores. Qt would also accept a name, but
    // allowing names here would let a palette field be "red" and still pass.
    return value.size() == 7
        && value.startsWith(QLatin1Char('#'))
        && QColor(value).isValid();
}

} // namespace

void AutoWindowPaletteTests::everyFieldIsANonEmptyColorValue()
{
    const QStringList fields = autoPaletteFields();
    for (const QString& field : fields) {
        // An empty string in a stylesheet does not fail; it produces `background: ;`, which Qt
        // drops, and the widget silently keeps the previous colour.
        QVERIFY2(!field.isEmpty(), qPrintable(QStringLiteral("empty palette field: '%1'").arg(field)));
        QVERIFY2(looksLikeAColor(field), qPrintable(QStringLiteral("not a #rrggbb colour: '%1'").arg(field)));
    }
}

void AutoWindowPaletteTests::onAccentIsUsableAsTextOnEveryFill()
{
    // The label colour is chosen once and drawn on top of every accent and danger fill. If any of
    // them became pale, white text would stop being readable, and the fix belongs in
    // onAccentColor() rather than at each call site.
    const AutoThemeInternal::AutoThemePalette& palette = AutoThemeInternal::currentPalette();
    const QColor label(AutoThemeInternal::onAccentColor());

    const QStringList fills{
        QString::fromLatin1(palette.accent),
        QString::fromLatin1(palette.accentPending),
        QString::fromLatin1(palette.accentToggle),
        QString::fromLatin1(palette.danger),
    };
    for (const QString& fill : fills) {
        const QColor background(fill);
        const qreal contrast = qAbs(label.lightnessF() - background.lightnessF());
        // A coarse check on purpose. WCAG contrast needs a full luminance calculation, which would
        // be a lot of arithmetic to say "these two are far apart"; lightness distance catches the
        // actual regression (a field becoming near-white) without pretending to be an audit.
        QVERIFY2(contrast > 0.3,
            qPrintable(QStringLiteral("label %1 is too close to fill %2")
                           .arg(label.name(), background.name())));
    }
}

void AutoWindowPaletteTests::autoPaletteIsNotTheMainPalette()
{
    // The auto window's accent is blue and the main window's is green: they are separate by
    // design. This case exists so "just reuse the app palette" has to be a deliberate change with
    // a visual review, not a tidy-up someone makes while removing duplication.
    const QString autoAccent = QString::fromLatin1(AutoThemeInternal::currentPalette().accent);
    const QString mainAccent =
        QString::fromLatin1(AppThemeInternal::paletteForName(QStringLiteral("Light")).accent);

    QVERIFY(!autoAccent.isEmpty());
    QVERIFY(!mainAccent.isEmpty());
    QVERIFY2(autoAccent.compare(mainAccent, Qt::CaseInsensitive) != 0,
        "the auto window's accent became the main window's accent -- that is a visible change, "
        "not a refactor");
}

void AutoWindowPaletteTests::paletteIsStableAcrossCalls()
{
    // currentPalette() returns a reference, and applyCompactStyle builds its stylesheet in a
    // function-local static. Both rely on the palette not changing value under them.
    const QString first = QString::fromLatin1(AutoThemeInternal::currentPalette().background);
    const QString second = QString::fromLatin1(AutoThemeInternal::currentPalette().background);
    QCOMPARE(first, second);
}

QTEST_MAIN(AutoWindowPaletteTests)
#include "AutoWindowPaletteTests.moc"
