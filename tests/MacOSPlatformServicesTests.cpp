#include <QtTest>

#include <QDir>
#include <QStandardPaths>
#include <QXmlStreamReader>

#include "platform/macos/MacAutoRunService.h"
#include "platform/macos/MacSystemProxyService.h"

// The macOS platform implementations are added to the application build only under APPLE,
// and no test ever runs on macOS -- so without a target of their own their first
// compilation would be the release build that ships them. This target compiles them on
// every platform and drives the pure functions they expose.
//
// It deliberately stops at the pure functions. update()/isEnabled()/setEnabled() shell out
// to networksetup, osascript and launchctl, and isEnabled()/setEnabled() write the real
// HOME directory, so none of them can be called from a test. That is exactly why the argv
// builders and the plist text are free functions rather than private members.
namespace {

const QString kService = QStringLiteral("Wi-Fi");
const QString kLoopback = QStringLiteral("127.0.0.1");

// One line per command, one command per line: it pins the order and the number of
// commands, and it prints the whole expected and actual lists when it fails.
QString flattened(const QList<QStringList>& commands)
{
    QStringList lines;
    for (const QStringList& command : commands) {
        lines.append(command.join(QLatin1Char(' ')));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class MacOSPlatformServicesTests : public QObject {
    Q_OBJECT

private slots:
    void bypassDomainsSplitsOnBothSeparatorsAndTrims();
    void bypassDomainsSkipsEmptyEntries();
    void bypassDomainsKeepsLocalAndUnknownTokensAsTyped();

    void disableTurnsOffAllThreeProtocolsForEveryService();
    void enableSetsAllThreeProtocolsForEveryService();
    void enableOmitsBypassDomainsWhenThereAreNone();
    void disableIgnoresBypassDomains();
    void aServiceNameWithSpacesStaysASingleArgument();
    void noNetworkServiceProducesNoCommands();

    void launchAgentLabelAndPathAreFixed();
    void launchAgentContentsIsAWellFormedPlistThatRunsAtLoad();
    void launchAgentContentsEscapesXmlMetacharactersInTheProgramPath();
};

void MacOSPlatformServicesTests::bypassDomainsSplitsOnBothSeparatorsAndTrims()
{
    // Windows keeps these in one semicolon-separated value and its UI also accepts commas,
    // so both are separators here; the surrounding spaces are not part of the domain.
    QCOMPARE(
        macProxyBypassDomains(QStringLiteral("localhost;*.local, 10.0.0.0/8")),
        QStringList({QStringLiteral("localhost"), QStringLiteral("*.local"), QStringLiteral("10.0.0.0/8")}));
}

void MacOSPlatformServicesTests::bypassDomainsSkipsEmptyEntries()
{
    QVERIFY(macProxyBypassDomains(QString()).isEmpty());
    QVERIFY(macProxyBypassDomains(QStringLiteral(";;")).isEmpty());
    QVERIFY(macProxyBypassDomains(QStringLiteral(", , ")).isEmpty());
    QCOMPARE(
        macProxyBypassDomains(QStringLiteral("a;;b")),
        QStringList({QStringLiteral("a"), QStringLiteral("b")}));
}

void MacOSPlatformServicesTests::bypassDomainsKeepsLocalAndUnknownTokensAsTyped()
{
    // `<local>` is meaningful on both platforms and must survive verbatim; anything else the
    // user typed for the Windows field is passed through rather than dropped, so a mistake
    // shows up in the system UI instead of silently changing the bypassed set.
    QCOMPARE(
        macProxyBypassDomains(QStringLiteral("<local>;169.254/16")),
        QStringList({QStringLiteral("<local>"), QStringLiteral("169.254/16")}));
}

void MacOSPlatformServicesTests::disableTurnsOffAllThreeProtocolsForEveryService()
{
    const QList<QStringList> commands = macProxyCommands({kService}, false, 0, 0, {});

    QCOMPARE(
        flattened(commands),
        QStringLiteral("-setwebproxystate Wi-Fi off\n"
                       "-setsecurewebproxystate Wi-Fi off\n"
                       "-setsocksfirewallproxystate Wi-Fi off"));
    // Clearing must not need a port: the shutdown path calls this with zeros.
    QVERIFY(!flattened(commands).contains(kLoopback));
}

void MacOSPlatformServicesTests::enableSetsAllThreeProtocolsForEveryService()
{
    const QList<QStringList> commands = macProxyCommands({kService}, true, 10809, 10808, {});

    QCOMPARE(
        flattened(commands),
        QStringLiteral("-setwebproxy Wi-Fi 127.0.0.1 10809\n"
                       "-setwebproxystate Wi-Fi on\n"
                       "-setsecurewebproxy Wi-Fi 127.0.0.1 10809\n"
                       "-setsecurewebproxystate Wi-Fi on\n"
                       "-setsocksfirewallproxy Wi-Fi 127.0.0.1 10808\n"
                       "-setsocksfirewallproxystate Wi-Fi on"));
}

void MacOSPlatformServicesTests::enableOmitsBypassDomainsWhenThereAreNone()
{
    const QList<QStringList> commands = macProxyCommands({kService}, true, 10809, 10808, {});

    QCOMPARE(commands.size(), 6);
    QVERIFY(!flattened(commands).contains(QStringLiteral("-setproxybypassdomains")));

    const QList<QStringList> withBypass =
        macProxyCommands({kService}, true, 10809, 10808, {QStringLiteral("localhost")});

    QCOMPARE(withBypass.size(), 7);
    QCOMPARE(
        withBypass.last(),
        QStringList({QStringLiteral("-setproxybypassdomains"), kService, QStringLiteral("localhost")}));
}

void MacOSPlatformServicesTests::disableIgnoresBypassDomains()
{
    // The call site already passes an empty list when disabling; this pins the second line
    // of defence, because leaving a stale bypass list behind on an "off" command would be
    // invisible in the system UI.
    const QList<QStringList> commands =
        macProxyCommands({kService}, false, 0, 0, {QStringLiteral("localhost")});

    QCOMPARE(commands.size(), 3);
    QVERIFY(!flattened(commands).contains(QStringLiteral("-setproxybypassdomains")));
}

void MacOSPlatformServicesTests::aServiceNameWithSpacesStaysASingleArgument()
{
    // Real service names are not shell-safe ("Thunderbolt Bridge"); each one has to arrive as
    // exactly one argv element, which is why the commands are argv lists and not a string.
    const QList<QStringList> commands =
        macProxyCommands({QStringLiteral("Thunderbolt Bridge")}, true, 10809, 10808, {});

    QCOMPARE(commands.at(0).size(), 4);
    QCOMPARE(commands.at(0).at(1), QStringLiteral("Thunderbolt Bridge"));
}

void MacOSPlatformServicesTests::noNetworkServiceProducesNoCommands()
{
    // A machine with no active service is the case update() reports as "cannot enable".
    QVERIFY(macProxyCommands({}, true, 10809, 10808, {}).isEmpty());
    QVERIFY(macProxyCommands({}, false, 0, 0, {}).isEmpty());
}

void MacOSPlatformServicesTests::launchAgentLabelAndPathAreFixed()
{
    QCOMPARE(macLaunchAgentLabel(), QStringLiteral("com.songbird.autostart"));

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QVERIFY(!home.isEmpty());
    QCOMPARE(
        macLaunchAgentPath(),
        QDir(home).filePath(QStringLiteral("Library/LaunchAgents/com.songbird.autostart.plist")));
}

void MacOSPlatformServicesTests::launchAgentContentsIsAWellFormedPlistThatRunsAtLoad()
{
    const QString program = QStringLiteral("/Applications/SongBird.app/Contents/MacOS/SongBird");
    const QString contents = macLaunchAgentContents(program);

    QXmlStreamReader reader(contents);
    while (!reader.atEnd()) {
        reader.readNext();
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));

    // The declaration, the DTD and the root are what make the file a plist rather than any
    // other XML document; launchd is lenient about the DTD but the other tools that read a
    // LaunchAgent are not. The declaration carries no encoding attribute because Qt omits it
    // when the writer targets a QString -- harmless, since UTF-8 is XML's default and the
    // file is written as UTF-8.
    QVERIFY2(contents.startsWith(QStringLiteral("<?xml version=\"1.0\"?>")), qPrintable(contents));
    QVERIFY(contents.contains(QStringLiteral("<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\"")));
    QVERIFY(contents.contains(QStringLiteral("<plist version=\"1.0\">")));
    QVERIFY(contents.contains(QStringLiteral("<key>Label</key>")));
    QVERIFY(contents.contains(QStringLiteral("<key>ProgramArguments</key>")));
    QVERIFY(contents.contains(QStringLiteral("<string>com.songbird.autostart</string>")));
    QVERIFY(contents.contains(QStringLiteral("<string>") + program + QStringLiteral("</string>")));
    QVERIFY(contents.contains(QStringLiteral("<key>RunAtLoad</key>")));
    QVERIFY(contents.contains(QStringLiteral("<true/>")));
    // launchd starts the executable itself; going through LaunchServices would need a bundle
    // identifier that only exists once the application is installed under /Applications.
    QVERIFY(!contents.contains(QStringLiteral("open -a")));
}

void MacOSPlatformServicesTests::launchAgentContentsEscapesXmlMetacharactersInTheProgramPath()
{
    // A path is user data: a home directory named `a&b` or a folder named `<x>` would make the
    // plist unparseable and the agent would silently never load. The reader at the end is the
    // real assertion -- unescaped metacharacters make it fail.
    const QString contents = macLaunchAgentContents(QStringLiteral("/Users/a&b/<SongBird>/SongBird"));

    QVERIFY(contents.contains(QStringLiteral("/Users/a&amp;b/&lt;SongBird&gt;/SongBird")));
    QVERIFY(!contents.contains(QStringLiteral("a&b")));
    QVERIFY(!contents.contains(QStringLiteral("<SongBird>")));

    QXmlStreamReader reader(contents);
    while (!reader.atEnd()) {
        reader.readNext();
    }
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
}

QTEST_GUILESS_MAIN(MacOSPlatformServicesTests)
#include "MacOSPlatformServicesTests.moc"
