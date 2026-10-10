#include "platform/macos/MacSystemProxyService.h"

#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>

namespace {

const QString kNetworksetupPath = QStringLiteral("/usr/sbin/networksetup");
const QString kOsascriptPath = QStringLiteral("/usr/bin/osascript");
const QString kLoopbackAddress = QStringLiteral("127.0.0.1");

struct CommandResult {
    bool started = false;
    int exitCode = -1;
    QString output;
};

CommandResult runCommand(const QString& program, const QStringList& arguments)
{
    CommandResult result;

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000)) {
        return result;
    }

    result.started = true;
    process.waitForFinished(20000);
    result.exitCode = process.exitCode();
    result.output = QString::fromUtf8(process.readAllStandardOutput());
    return result;
}

// Single-quote a value for /bin/sh. The only character that needs attention is the single
// quote itself, which is closed, escaped and reopened.
QString shellQuote(const QString& value)
{
    QString quoted = value;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString networksetupCommandLine(const QStringList& arguments)
{
    QStringList quoted;
    quoted.reserve(arguments.size() + 1);
    quoted.append(shellQuote(kNetworksetupPath));
    for (const QString& argument : arguments) {
        quoted.append(shellQuote(argument));
    }
    return quoted.join(QLatin1Char(' '));
}

// One process per command, so a failure names itself and the rest still run. The caller
// only looks at the aggregate, but a partial application is exactly the case the elevated
// retry exists for.
bool runDirectly(const QList<QStringList>& commands)
{
    bool allSucceeded = true;
    for (const QStringList& arguments : commands) {
        const CommandResult result = runCommand(kNetworksetupPath, arguments);
        if (!result.started || result.exitCode != 0) {
            allSucceeded = false;
        }
    }
    return allSucceeded;
}

// Runs the same commands through a single privileged shell so macOS shows its own
// authorization dialog once instead of once per networksetup call. The commands go into a
// temporary script rather than into the AppleScript source, because the script is the only
// place the quoting is unambiguous: osascript would need every quote of every argument
// escaped a second time for AppleScript's own string literal.
bool runElevated(const QList<QStringList>& commands)
{
    QTemporaryFile script(QDir::tempPath() + QStringLiteral("/songbird-proxy-XXXXXX"));
    if (!script.open()) {
        return false;
    }
    script.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    QByteArray content = "#!/bin/sh\n";
    for (const QStringList& arguments : commands) {
        content += networksetupCommandLine(arguments).toUtf8();
        content += '\n';
    }
    // Every networksetup exit code is already discarded by running the commands in
    // sequence; the script's own status is what says whether the privileged shell ran.
    content += "exit 0\n";

    if (script.write(content) != content.size()) {
        return false;
    }
    script.flush();

    const QString appleScript = QStringLiteral("do shell script \"/bin/sh %1\" with administrator privileges")
                                    .arg(script.fileName());
    const CommandResult result = runCommand(kOsascriptPath, {QStringLiteral("-e"), appleScript});
    return result.started && result.exitCode == 0;
}

} // namespace

QStringList macProxyBypassDomains(const QString& proxyExceptions)
{
    // Windows separates ProxyOverride entries with ';' and Windows itself also accepts ','
    // in the UI, so both are split. `<local>` is meaningful on both platforms and is kept
    // as-is; anything the user typed for the Windows field that macOS does not understand
    // is passed through rather than dropped, so a mistake is visible in the system UI
    // instead of silently changing the set of bypassed hosts.
    QStringList domains;
    const QStringList entries = proxyExceptions.split(QRegularExpression(QStringLiteral("[;,]")), Qt::SkipEmptyParts);
    for (const QString& entry : entries) {
        const QString trimmed = entry.trimmed();
        if (!trimmed.isEmpty()) {
            domains.append(trimmed);
        }
    }
    return domains;
}

QStringList MacSystemProxyService::enabledNetworkServices()
{
    const CommandResult result = runCommand(kNetworksetupPath, {QStringLiteral("-listallnetworkservices")});
    if (!result.started || result.exitCode != 0) {
        return {};
    }

    QStringList services;
    const QStringList lines = result.output.split(QLatin1Char('\n'));
    for (int index = 0; index < lines.size(); ++index) {
        const QString line = lines.at(index).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        // The first line is the "An asterisk (*) denotes that a network service is
        // disabled." header, and a leading asterisk is that marker on a real entry.
        if (index == 0 && line.startsWith(QLatin1String("An asterisk"))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('*'))) {
            continue;
        }
        services.append(line);
    }
    return services;
}

QList<QStringList> macProxyCommands(
    const QStringList& services,
    bool enable,
    int httpPort,
    int socksPort,
    const QStringList& bypassDomains)
{
    QList<QStringList> commands;
    for (const QString& service : services) {
        if (!enable) {
            commands.append({QStringLiteral("-setwebproxystate"), service, QStringLiteral("off")});
            commands.append({QStringLiteral("-setsecurewebproxystate"), service, QStringLiteral("off")});
            commands.append({QStringLiteral("-setsocksfirewallproxystate"), service, QStringLiteral("off")});
            continue;
        }

        // All three are set, not just the one the configuration calls "the" proxy: the
        // cores this application starts expose an HTTP port and a SOCKS port, and a
        // client that picks HTTPS without an HTTPS entry goes direct -- which looks like
        // "the proxy does not work" rather than like a missing setting.
        commands.append({QStringLiteral("-setwebproxy"), service, kLoopbackAddress, QString::number(httpPort)});
        commands.append({QStringLiteral("-setwebproxystate"), service, QStringLiteral("on")});
        commands.append({QStringLiteral("-setsecurewebproxy"), service, kLoopbackAddress, QString::number(httpPort)});
        commands.append({QStringLiteral("-setsecurewebproxystate"), service, QStringLiteral("on")});
        commands.append({QStringLiteral("-setsocksfirewallproxy"), service, kLoopbackAddress, QString::number(socksPort)});
        commands.append({QStringLiteral("-setsocksfirewallproxystate"), service, QStringLiteral("on")});

        if (!bypassDomains.isEmpty()) {
            QStringList bypassArguments{QStringLiteral("-setproxybypassdomains"), service};
            bypassArguments += bypassDomains;
            commands.append(bypassArguments);
        }
    }
    return commands;
}

bool MacSystemProxyService::update(
    SystemProxyMode mode,
    int httpPort,
    int socksPort,
    const QString& proxyExceptions,
    const QString& advancedProtocol) const
{
    // macOS has no equivalent of the Windows ProxyServer template this carries
    // ({ip}/{http_port}/{socks_port}); the per-service commands below express the same
    // thing directly, so the setting is deliberately ignored rather than half-applied.
    Q_UNUSED(advancedProtocol);

    const bool enable = mode == SystemProxyMode::ForcedChange;
    if (enable && httpPort <= 0) {
        return false;
    }

    const QStringList services = enabledNetworkServices();
    if (services.isEmpty()) {
        // No network service to point at a proxy. Enabling cannot be reported as success
        // -- the user would be told the proxy is on while nothing was written -- but
        // clearing is already true, so the shutdown and cleanup paths must not fail here.
        return !enable;
    }

    const QList<QStringList> commands = macProxyCommands(
        services,
        enable,
        httpPort,
        socksPort,
        enable ? macProxyBypassDomains(proxyExceptions) : QStringList());

    return runDirectly(commands) || runElevated(commands);
}

bool MacSystemProxyService::isEnabled() const
{
    const QStringList services = enabledNetworkServices();
    if (services.isEmpty()) {
        return false;
    }

    // Any service would do for the report; the first is the one the user sees as "the"
    // connection in System Settings, and asking about all of them would make the answer
    // "mixed" for a state the callers only compare against a single expectation.
    const CommandResult result = runCommand(
        kNetworksetupPath,
        {QStringLiteral("-getwebproxy"), services.first()});
    if (!result.started || result.exitCode != 0) {
        return false;
    }

    const QString prefix = QStringLiteral("Enabled:");
    const QStringList lines = result.output.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(prefix, Qt::CaseInsensitive)) {
            return trimmed.mid(prefix.size()).trimmed().compare(QStringLiteral("Yes"), Qt::CaseInsensitive) == 0;
        }
    }
    return false;
}

void MacSystemProxyService::resetOnShutdown() const
{
    // Best effort, and deliberately without a dedicated elevated path of its own: the
    // authorization the enable() call obtained is normally still cached, so this usually
    // runs silently, and a prompt at quit is a far better outcome than leaving the system
    // pointed at a core that has just stopped.
    update(SystemProxyMode::ForcedClear, 0, 0, QString(), QString());
}
