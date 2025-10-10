// src/ProcessHandling.cpp
#include "ProcessHandling.h"
#include <QRegularExpression>
#include <QTextStream>

ProcessHandling::ProcessHandling(QObject *parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &ProcessHandling::handleStdout);
    connect(&process_, &QProcess::readyReadStandardError,
            this, &ProcessHandling::handleStderr);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessHandling::handleFinished);
}

void ProcessHandling::startCommand(const QStringList &args) {
    if (process_.state() != QProcess::NotRunning)
        process_.kill();
    emit logLine("[Run] minipro " + args.join(' '));
    process_.start("minipro", args);
}

void ProcessHandling::sendResponse(const QString &input) {
    if (process_.state() == QProcess::Running) {
        QTextStream(&process_).operator<<(input + "\n");
        process_.waitForBytesWritten(100);
    }
}

void ProcessHandling::handleStdout() {
    while (process_.canReadLine()) {
        QString line = QString::fromUtf8(process_.readLine()).trimmed();
        parseLine(line);
    }
}

void ProcessHandling::handleStderr() {
    while (process_.canReadLine()) {
        QString line = QString::fromUtf8(process_.readLine()).trimmed();
        emit errorLine(line);
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
