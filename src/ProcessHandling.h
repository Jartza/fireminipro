// src/ProcessHandling.h
#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

class ProcessHandling : public QObject {
    Q_OBJECT
public:
    explicit ProcessHandling(QObject *parent = nullptr);
    void startCommand(const QStringList &args);
    void sendResponse(const QString &input);
    // Fire-and-forget scan for connected programmers (minipro -k)
    void scanConnectedDevices();
    void fetchSupportedDevices(const QString &programmer);

signals:
    void logLine(const QString &text);     // Normal output
    void errorLine(const QString &text);   // Error output
    void progress(int percent);            // Parsed progress %
    void promptDetected(const QString &promptText);
    void finished(int exitCode, QProcess::ExitStatus status);
    // Emitted after scanConnectedDevices() completes
    void devicesScanned(const QStringList &names);
    void devicesListed(const QStringList &names);

private slots:
    void handleStdout();
    void handleStderr();
    void handleFinished(int exitCode, QProcess::ExitStatus status);

private:
    // Internal mode to disambiguate generic runs vs scans
    enum class Mode { Idle, Generic, Scan, DeviceList };
    Mode    mode_{Mode::Idle};
    QString stdoutBuffer_;
    QString stderrBuffer_;

    void parseLine(const QString &line);
    QString resolveMiniproPath();
    QStringList parseProgrammerList(const QString &text) const;
    QProcess process_;
};
