// src/ProcessHandling.cpp
#include "ProcessHandling.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QStandardPaths>
#include <QFileInfo>

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
}

void ProcessHandling::startCommand(const QStringList &args) {
    if (process_.state() != QProcess::NotRunning)
        process_.kill();

    // Find the minipro executable robustly
    QString bin = QStandardPaths::findExecutable("minipro");
    if (bin.isEmpty()) {
        const QStringList candidates = {
            "/opt/homebrew/bin/minipro",   // Apple Silicon Homebrew
            "/usr/local/bin/minipro",      // Intel Homebrew or manual
            "/usr/bin/minipro"             // fallback
        };
        for (const QString &c : candidates) {
            if (QFileInfo::exists(c)) { bin = c; break; }
        }
    }
    if (bin.isEmpty()) bin = "minipro"; // let PATH try

    emit logLine("[Run] " + bin + " " + args.join(' '));

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
    const QString all = QString::fromUtf8(process_.readAllStandardOutput());
    const auto lines = all.split('\n');
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty()) parseLine(t);
    }
}

void ProcessHandling::handleStderr() {
    const QString all = QString::fromUtf8(process_.readAllStandardError());
    const auto lines = all.split('\n');
    for (const QString &ln : lines) {
        const QString t = ln.trimmed();
        if (!t.isEmpty()) emit errorLine(t);
    }
}

void ProcessHandling::handleFinished(int exitCode, QProcess::ExitStatus status) {
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
