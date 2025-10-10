// src/ProcessHandling.cpp
#include "ProcessHandling.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QStandardPaths>
#include <QFileInfo>

QString ProcessHandling::resolveMiniproPath() {
    QString bin = QStandardPaths::findExecutable("minipro");
    //bin.clear();
    if (bin.isEmpty()) {
        const QStringList candidates = {
            "/Users/jartza/src/minipro-vtable/minipro",
            "/opt/homebrew/bin/minipro",
            "/usr/local/bin/minipro",
            "/usr/bin/minipro"
        };
        for (const QString &c : candidates) {
            if (QFileInfo::exists(c)) { bin = c; break; }
        }
    }
    if (bin.isEmpty()) bin = "minipro";
    return bin;
}

QStringList ProcessHandling::parseProgrammerList(const QString &text) const {
    QStringList out;

    // New-format: "Programmer N: TL866A; ..."
    QRegularExpression reNew(R"(^\s*Programmer\s+\d+\s*:\s*([^;]+);)", QRegularExpression::MultilineOption);
    auto it = reNew.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        const QString name = m.captured(1).trimmed();
        if (!name.isEmpty()) out << name;
    }

    // Old-format: "t48: T48" or "tl866a: TL866A"
    QRegularExpression reOld(R"(^\s*[^:\n]+:\s*([^\n;]+))", QRegularExpression::MultilineOption);
    auto it2 = reOld.globalMatch(text);
    while (it2.hasNext()) {
        auto m = it2.next();
        const QString name = m.captured(1).trimmed();
        if (!name.isEmpty() && !out.contains(name)) out << name;
    }

    // Filter obvious non-device lines
    out.erase(std::remove_if(out.begin(), out.end(), [](const QString &s){
        const QString t = s.toLower();
        return t.contains("no programmer found") || t.startsWith("share dir") || t.startsWith("supported");
    }), out.end());

    out.removeDuplicates();
    return out;
}

ProcessHandling::ProcessHandling(QObject *parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &ProcessHandling::handleStdout);
    connect(&process_, &QProcess::readyReadStandardError,
            this, &ProcessHandling::handleStderr);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessHandling::handleFinished);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e){ emit errorLine(QString("[QProcess error] %1").arg(static_cast<int>(e))); });

    mode_ = Mode::Idle;
    stdoutBuffer_.clear();
}

void ProcessHandling::startCommand(const QStringList &args) {
    if (process_.state() != QProcess::NotRunning)
        process_.kill();

    const QString bin = resolveMiniproPath();
    emit logLine("[Run] " + bin + " " + args.join(' '));

    mode_ = Mode::Generic;
    stdoutBuffer_.clear();

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.start();
}

void ProcessHandling::scanConnectedDevices() {
    if (process_.state() != QProcess::NotRunning)
        process_.kill();

    const QString bin = resolveMiniproPath();
    const QStringList args{ "-k" };
    emit logLine("[Run] " + bin + " " + args.join(' '));

    mode_ = Mode::Scan;
    stdoutBuffer_.clear();
    stderrBuffer_.clear();

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.start();
}

void ProcessHandling::fetchSupportedDevices(const QString &programmer)
{
    if (process_.state() != QProcess::NotRunning)
        process_.kill();

    const QString bin = resolveMiniproPath();
    // Order: -q <programmer> -l
    const QStringList args{ "-q", programmer, "-l" };

    emit logLine("[Run] " + bin + " " + args.join(' '));

    mode_ = Mode::DeviceList;
    stdoutBuffer_.clear();
    stderrBuffer_.clear();

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::SeparateChannels);
    process_.start();
}

void ProcessHandling::sendResponse(const QString &input) {
    if (process_.state() == QProcess::Running) {
        QTextStream(&process_).operator<<(input + "\n");
        process_.waitForBytesWritten(100);
    }
}

void ProcessHandling::handleStdout() {
    const QString raw = QString::fromLocal8Bit(process_.readAllStandardOutput());
    stdoutBuffer_.append(raw);

    //TODO remove
    return;

    const auto lines = raw.split('\n');
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty()) parseLine(t);
    }
}

void ProcessHandling::handleStderr() {
    const QString all = QString::fromLocal8Bit(process_.readAllStandardError());
    stderrBuffer_.append(all);

    //TODO remove
    return;

    const auto lines = all.split('\n');
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty()) emit errorLine(t);
    }
}

void ProcessHandling::handleFinished(int exitCode, QProcess::ExitStatus status) {
    emit logLine(QString("[Process finished] exit code %1, status %2")
                 .arg(exitCode)
                 .arg(status == QProcess::NormalExit ? "normal" : "crashed"));
    emit logLine(QString("Mode = %1").arg(static_cast<int>(mode_)));
    if (mode_ == Mode::Scan) {
        emit logLine("[Scan output]");
        const QStringList names = parseProgrammerList(stderrBuffer_);

        mode_ = Mode::Idle;
        emit devicesScanned(names);
    } else if (mode_ == Mode::DeviceList) {
        emit logLine("[Device list output]");
        QStringList devices = stdoutBuffer_.split('\n', Qt::SkipEmptyParts);
        for (QString &s : devices) {
            emit logLine("[Device] " + s);
            s = s.trimmed();
        }
        // very light filtering
        devices.erase(std::remove_if(devices.begin(), devices.end(), [](const QString &s){
            return s.isEmpty();
        }), devices.end());
        devices.removeDuplicates();

        mode_ = Mode::Idle;
        emit devicesListed(devices);
    } else {
        mode_ = Mode::Idle;
    }
    stderrBuffer_.clear();
    stdoutBuffer_.clear();
    emit finished(exitCode, status);
}

void ProcessHandling::parseLine(const QString &line) {
    emit logLine(line);

    // Match progress lines like "Writing: 25.0%"
    static QRegularExpression progressRe(R"(.*?(\d+(?:\.\d+)?)%)");
    auto m = progressRe.match(line);
    if (m.hasMatch()) {
        bool ok = false;
        int pct = int(m.captured(1).toDouble(&ok));
        if (ok) emit progress(pct);
    }

    // Detect prompt: anything ending with [y/n]
    if (line.contains("[y/n]", Qt::CaseInsensitive)) {
        emit promptDetected(line);
    }
}
