#include "platform/macos/MacAutoRunService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QXmlStreamWriter>

namespace {

// launchctl is invoked by absolute path because a login agent is exactly the kind of
// process whose PATH cannot be assumed to contain /bin.
const QString kLaunchctlPath = QStringLiteral("/bin/launchctl");

// `launchctl load -w` is the legacy spelling, and it is used deliberately: the modern
// `bootstrap gui/<uid>` requires the domain target to be constructed from the uid, and a
// failure there is reported as a bare non-zero exit. `-w` additionally clears the
// disabled flag, which is what a login agent wants.
bool runLaunchctl(const QStringList& arguments)
{
    QProcess process;
    process.setProgram(kLaunchctlPath);
    process.setArguments(arguments);
    process.start();
    if (!process.waitForStarted(5000)) {
        return false;
    }
    process.waitForFinished(15000);
    return process.exitCode() == 0;
}

} // namespace

QString macLaunchAgentLabel()
{
    return QStringLiteral("com.songbird.autostart");
}

QString macLaunchAgentPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
               .filePath(QStringLiteral("Library/LaunchAgents/") + macLaunchAgentLabel() + QStringLiteral(".plist"));
}

QString macLaunchAgentContents(const QString& programPath)
{
    // Built through QXmlStreamWriter rather than a hand-written template. Two reasons: the
    // escaping of the program path -- which is user data, and a home directory named `a&b`
    // would otherwise produce a plist launchd refuses -- becomes the writer's job instead of
    // a replace() chain that has to be kept in step with the XML rules by hand; and the
    // element names below are XML vocabulary rather than user-visible prose.
    QString contents;
    QXmlStreamWriter writer(&contents);
    writer.setAutoFormatting(true);
    writer.setAutoFormattingIndent(4);
    // The declaration comes out as `<?xml version="1.0"?>`: Qt only emits an encoding
    // attribute when the writer targets a device. That is correct here -- the file is written
    // as UTF-8 and UTF-8 is what XML assumes when the declaration is silent.
    writer.writeStartDocument();

    // launchd does not require the DTD, but every plist the system itself writes carries it,
    // and keeping it is what makes the file recognizable as a plist to other tools.
    writer.writeDTD(QStringLiteral(
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"));

    writer.writeStartElement(QStringLiteral("plist"));
    writer.writeAttribute(QStringLiteral("version"), QStringLiteral("1.0"));
    writer.writeStartElement(QStringLiteral("dict"));

    writer.writeTextElement(QStringLiteral("key"), QStringLiteral("Label"));
    writer.writeTextElement(QStringLiteral("string"), macLaunchAgentLabel());

    // ProgramArguments is the executable inside the bundle, not `open -a`: launchd starts
    // the process directly, and going through LaunchServices would need a bundle
    // identifier that only exists once the application is installed under /Applications.
    writer.writeTextElement(QStringLiteral("key"), QStringLiteral("ProgramArguments"));
    writer.writeStartElement(QStringLiteral("array"));
    writer.writeTextElement(QStringLiteral("string"), programPath);
    writer.writeEndElement();

    writer.writeTextElement(QStringLiteral("key"), QStringLiteral("RunAtLoad"));
    writer.writeEmptyElement(QStringLiteral("true"));

    writer.writeEndElement(); // dict
    writer.writeEndElement(); // plist
    writer.writeEndDocument();

    return contents;
}

bool MacAutoRunService::isEnabled() const
{
    return QFile::exists(macLaunchAgentPath());
}

bool MacAutoRunService::setEnabled(bool enabled) const
{
    const QString path = macLaunchAgentPath();

    if (!enabled) {
        // Unload first and remove second. Removing the plist of a loaded agent leaves it
        // running until logout, and the caller has already reported the switch as off.
        runLaunchctl({QStringLiteral("unload"), QStringLiteral("-w"), path});
        return QFile::remove(path) || !QFile::exists(path);
    }

    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    const QByteArray contents = macLaunchAgentContents(QCoreApplication::applicationFilePath()).toUtf8();
    if (file.write(contents) != contents.size()) {
        return false;
    }
    file.close();

    // A plist that was written but never loaded still reads back as "enabled", and it
    // does take effect at the next login, so a failed load is not reported as a failed
    // enable -- it would only make the setting look like it did not stick.
    runLaunchctl({QStringLiteral("load"), QStringLiteral("-w"), path});
    return true;
}
