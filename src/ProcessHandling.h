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

signals:
    void logLine(const QString &text);     // Normal output
    void errorLine(const QString &text);   // Error output
    void progress(int percent);            // Parsed progress %
    void promptDetected(const QString &promptText);
    void finished(int exitCode, QProcess::ExitStatus status);

private slots:
    void handleStdout();
    void handleStderr();
    void handleFinished(int exitCode, QProcess::ExitStatus status);

private:
    void parseLine(const QString &line);
    QProcess process_;
};
