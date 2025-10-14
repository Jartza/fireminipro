// src/ProcessHandling.cpp
#include "ProcessHandling.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>

// Constructor
ProcessHandling::ProcessHandling(QObject *parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &ProcessHandling::handleStdout);
    connect(&process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProcessHandling::handleFinished);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e){ 
        // if process was killed, do not log an error
        if (e == QProcess::ProcessError::Crashed) return;
        emit errorLine(QString("[QProcess error] %1").arg(static_cast<int>(e)));
        emit finished(-1, QProcess::CrashExit);
    });

    mode_ = Mode::Idle;
    stdoutBuffer_.clear();
}

QString ProcessHandling::resolveMiniproPath() {
    QString bin = QStandardPaths::findExecutable(QStringLiteral("minipro"));

    // Resolve bundled share directory so we can point MINIPRO_HOME to it.
    auto bundledShareDir = []() -> QString {
        const QString appDirEnv = qEnvironmentVariable("APPDIR");
        if (!appDirEnv.isEmpty()) {
            return appDirEnv + "/usr/share/minipro";
        }
        const QString execDir = QCoreApplication::applicationDirPath();
        if (!execDir.isEmpty()) {
            QDir dir(execDir);
            if (dir.cd("../share/minipro")) {
                return dir.canonicalPath();
            }
        }
        return {};
    }();
    if (!bundledShareDir.isEmpty()) {
        qputenv("MINIPRO_HOME", bundledShareDir.toUtf8());
    }

    QStringList candidates;

    // In AppImage/runtime environments APPDIR points to the mounted bundle.
    const QString appDir = qEnvironmentVariable("APPDIR");
    if (!appDir.isEmpty()) {
        candidates << (appDir + "/usr/bin/minipro");
    }

    // Fall back to the directory containing the executable (covers local bundles).
    const QString execDir = QCoreApplication::applicationDirPath();
    if (!execDir.isEmpty()) {
        candidates << (execDir + "/minipro");
#ifdef Q_OS_MAC
        candidates << (execDir + "/../Resources/minipro");
#endif
    }

    candidates << QStringLiteral("/opt/homebrew/bin/minipro")
               << QStringLiteral("/usr/local/bin/minipro")
               << QStringLiteral("/usr/bin/minipro");

    for (const QString &candidate : candidates) {
        QFileInfo info(candidate);
        if (info.exists() && info.isExecutable()) {
            bin = info.absoluteFilePath();
            break;
        }
    }

    if (bin.isEmpty()) bin = "minipro";
    return bin;
}

// Parse programmer list output from minipro -k
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
    QRegularExpression reOld(R"(^\s*([^\s:]+)\s*:\s*[^\n;]+)", QRegularExpression::MultilineOption);
    auto it2 = reOld.globalMatch(text);
    while (it2.hasNext()) {
        auto m = it2.next();
        QString name = m.captured(1).trimmed();
        name = name.toUpper();
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

// Parse chip info output from minipro -q <programmer> -d "<dev>"
ProcessHandling::ChipInfo ProcessHandling::parseChipInfo(const QString &text) const
{
    ChipInfo ci;
    ci.raw = text;

    auto rxLine = [](const QString &label){
        return QRegularExpression("^\\s*" + QRegularExpression::escape(label) + "\\s*:\\s*(.+)\\s*$",
                                  QRegularExpression::MultilineOption);
    };
    auto cap1 = [&](const QRegularExpression &rx) -> QString {
        auto m = rx.match(text);
        return m.hasMatch() ? m.captured(1).trimmed() : QString{};
    };

    // Name: "AM2764A@DIP28" or just "AM2764A"
    {
        const QString nameLine = cap1(rxLine("Name"));
        if (!nameLine.isEmpty()) {
            const int at = nameLine.indexOf('@');
            if (at >= 0) {
                ci.baseName = nameLine.left(at).trimmed();
                ci.package  = nameLine.mid(at + 1).trimmed();
            } else {
                ci.baseName = nameLine.trimmed();
            }
        }
    }

    // Memory: "8192 Bytes" or "262144 Words"
    {
        QRegularExpression rxMem(R"(^\s*Memory\s*:\s*([0-9]+)\s*(Bytes?|Words?)\s*$)",
                                 QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        auto m = rxMem.match(text);
        if (m.hasMatch()) {
            const qulonglong val = m.captured(1).toULongLong();
            const QString unit = m.captured(2).toLower();
            if (unit.startsWith("word")) {
                ci.bytes = val * 2;   // words -> bytes
                ci.wordBits = 16;
            } else {
                ci.bytes = val;
                ci.wordBits = 8;
            }
        }
    }

    // Logic: "Vector count: 10" (logic ICs won’t have a Memory line)
    {
        static QRegularExpression reVec(R"(^\s*Vector count:\s*([0-9]+))",
                                        QRegularExpression::MultilineOption);
        auto m = reVec.match(text);
        if (m.hasMatch()) {
            ci.isLogic = true;
            bool ok=false;
            ci.vectorCount = m.captured(1).toInt(&ok);
            if (!ok) ci.vectorCount = 0;
        }
    }

    // Protocol: "0x07"
    {
        const QString proto = cap1(rxLine("Protocol"));
        if (!proto.isEmpty()) ci.protocol = proto;
    }

    // Read/Write buffer sizes: "Read buffer size: 1024 Bytes", "Write buffer size: 128 Bytes"
    auto parseSize = [&](const QString &label) -> qulonglong {
        QRegularExpression rx(QString(R"(^\s*)") + QRegularExpression::escape(label) +
                              R"(\s*:\s*([0-9]+))",
                              QRegularExpression::MultilineOption | QRegularExpression::CaseInsensitiveOption);
        auto m = rx.match(text);
        return m.hasMatch() ? m.captured(1).toULongLong() : 0ull;
    };
    ci.readBuf  = parseSize("Read buffer size");
    ci.writeBuf = parseSize("Write buffer size");

    return ci;
}

// Start the minipro process and make sure only one instance is running
void ProcessHandling::startMinipro(Mode mode, const QStringList& args)
{
    // If something is still running, stop it (keeps current behavior)
    if (mode_ != Mode::Idle || process_.state() == QProcess::Running) {
        process_.kill();
        process_.waitForFinished(3000);
    }

    const QString bin = resolveMiniproPath();

    // Log the exact command line we’re about to run
    emit logLine(QString("[Run] %1 %2").arg(bin, args.join(' ')));

    // Set mode first, then clear any previous buffered output
    mode_ = mode;
    stdoutBuffer_.clear();

    // Unified QProcess setup
    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start();
    emit started();
}

// Helper to create a unique temp path for reading
static QString uniqueTempPath(const QString& base = "fireminipro-read")
{
    const QString tmpRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString ts = QDateTime::currentDateTime().toString("yyMMdd-hhmmss");
    return QDir(tmpRoot).filePath(base + "-" + ts + ".bin");
}

// Read from chip to a unique temp file, emit readReady() with path when done
void ProcessHandling::readChipImage(const QString& programmer,
                                    const QString& device,
                                    const QStringList& extraFlags)
{
    // Parse device name without @ending, if one exists
    QString deviceName = device.split('@').first().trimmed();
    QString outPath = uniqueTempPath(deviceName);
    pendingTempPath_ = outPath;

    // We might need extraFlags like "-y" for reading
    QStringList args;
    args << "-p" << device << "-r" << outPath;
    args << extraFlags;

    startMinipro(Mode::Reading, args);
}

// Write from a given file to chip
void ProcessHandling::writeChipImage(const QString& programmer,
                                     const QString& device,
                                     const QString& filePath,
                                     const QStringList& extraFlags)
{
    QStringList args;
    // We might need extraFlags like "-y" for writing
    args << "-p" << device << "-w" << filePath;
    args << extraFlags;

    startMinipro(Mode::Writing, args);
}

// Scan for connected programmers (minipro -k)
void ProcessHandling::scanConnectedDevices() {
    const QStringList args{ "-k" };

    startMinipro(Mode::Scan, args);
}

// Fetch supported devices for a given programmer (minipro -q <programmer> -l)
void ProcessHandling::fetchSupportedDevices(const QString &programmer)
{
    // Supported devices need programmer name: -q <programmer> -l
    const QStringList args{ "-q", programmer, "-l" };

    startMinipro(Mode::DeviceList, args);
}

// Fetch chip info for a given programmer and device (minipro -q <programmer> -d "<dev>")
void ProcessHandling::fetchChipInfo(const QString &programmer, const QString &device)
{
    // Chip info needs programmer and device: -q <programmer> -d <dev>
    QStringList args;
    if (!programmer.isEmpty())
        args << "-q" << programmer;
    args << "-d" << device;

    startMinipro(Mode::ChipInfo, args);
}

// Check if chip is blank: minipro -p <device> -b
void ProcessHandling::checkIfBlank(const QString &programmer,
                                   const QString &device,
                                   const QStringList &extraFlags)
{
    QStringList args;
    args << "-p" << device << "-b";
    args << extraFlags;

    startMinipro(Mode::Generic, args);
}

// Erase chip: minipro -p <device> -E
void ProcessHandling::eraseChip(const QString &programmer,
                                const QString &device,
                                const QStringList &extraFlags)
{
    QStringList args;
    args << "-p" << device << "-E";
    args << extraFlags;

    startMinipro(Mode::Generic, args);
}

// Test logic chip: minipro -p <device> -T
void ProcessHandling::testLogicChip(const QString &programmer,
                                     const QString &device,
                                     const QStringList &extraFlags)
{
    QStringList args;
    args << "-p" << device << "-T";
    args << extraFlags;

    startMinipro(Mode::Logic, args);
}

// Send input to the running process (for prompts).
// not used yet.
void ProcessHandling::sendResponse(const QString &input) {
    if (process_.state() == QProcess::Running) {
        QTextStream(&process_).operator<<(input + "\n");
        process_.waitForBytesWritten(100);
    }
}

// Helpers for parsing progress and stripping ANSI/VT codes
QString ProcessHandling::stripAnsi(QString s) {
    static QRegularExpression ansiRe(R"(\x1B\[[0-9;]*[A-Za-z])");
    return s.remove(ansiRe);
}

// Extract percentage from a line of text, or -1 if none found,
// for progress bar updates. Handles "xx%", "xx %", and "… OK" endings.
// OK is treated as 100%.
int ProcessHandling::extractPercent(const QString &line) {
    static const QRegularExpression okTail(
        R"((?:ms|sec)?\s*ok\s*$|verification\s*ok\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    if (okTail.match(line).hasMatch())
        return 100;

    static QRegularExpression re(R"((\d{1,3})\s*%)");
    auto m = re.match(line);
    if (!m.hasMatch()) return -1;

    bool ok = false;
    int pct = m.captured(1).toInt(&ok);
    return (ok && pct >= 0 && pct <= 100) ? pct : -1;
}

// Detect phase text from a line, e.g. "Reading" or "Writing"
// for progress bar text updates. Returns empty string if none found.
QString ProcessHandling::detectPhaseText(const QString &line) {
    static const QRegularExpression rd(R"(\bReading\s*Code\.\.\.)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression wr(R"(\bWriting\s*Code\.\.\.)",
        QRegularExpression::CaseInsensitiveOption);

    if (rd.match(line).hasMatch()) return QStringLiteral("Reading");
    if (wr.match(line).hasMatch()) return QStringLiteral("Writing");
    return {};
}

// Handle stdout+stderr (merged) from the process, parse progress and log lines.
// Parses lines for errors/warnings and emits logLine() or errorLine() as needed.
// Calls extractPercent() and detectPhaseText() to parse progress bar updates.
// Adds ANSI-stripped lines to internal stdoutBuffer_ for later parsing by
// the handleFinished() slot.
void ProcessHandling::handleStdout() {
    const QString all = QString::fromLocal8Bit(process_.readAllStandardOutput());

    const auto lines = all.split('\n');
    for (QString ln : lines) {
        if (ln.isEmpty()) continue;

        if (mode_ == Mode::Logic) {
            ln = stripAnsi(ln);
        } else {
            // Remove ANSI sequences for non-logic modes
            ln = stripAnsi(ln).trimmed();
        }
        stdoutBuffer_.append(ln + "\n");

        // We want to log only specific output
        if (ln.contains("error", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("warning", Qt::CaseInsensitive)) {
            if (!ln.contains("not yet complete", Qt::CaseInsensitive)) // ignore "not yet completed" warnings
                emit logLine(ln);
        } else if (ln.contains("invalid", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("incorrect", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("failed", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("can't", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("is blank", Qt::CaseInsensitive)) {
            emit logLine(ln);
        } else if (ln.contains("success", Qt::CaseInsensitive)) {
            emit logLine(ln);
        } else if (ln.endsWith(" ok", Qt::CaseInsensitive)) {
            emit logLine(ln);
        } else if (mode_ == Mode::Logic) {
            emit logLine(ln);
        }

        // Parse possible progress from stdout too
        const int pct = extractPercent(ln);
        const QString phase = detectPhaseText(ln);
        if (pct >= 0 && pct <= 100) {
            emit progress(pct, phase);
        }
    }
}

// Process has finished, parse final output based on mode and
// emit appropriate signals.
void ProcessHandling::handleFinished(int exitCode, QProcess::ExitStatus status) {
    // Scanning for devices
    if (mode_ == Mode::Scan) {
        const QStringList names = parseProgrammerList(stdoutBuffer_);
        mode_ = Mode::Idle;
        emit devicesScanned(names);
    // Get list of supported devices
    } else if (mode_ == Mode::DeviceList) {
        QStringList devices = stdoutBuffer_.split('\n', Qt::SkipEmptyParts);
        for (QString &s : devices) {
            s = s.trimmed();
        }
        // very light filtering of device list, for empty lines, removing duplicates etc.
        devices.erase(std::remove_if(devices.begin(), devices.end(), [](const QString &s){
            return s.isEmpty();
        }), devices.end());
        devices.removeDuplicates();
        mode_ = Mode::Idle;
        emit devicesListed(devices);
    // Get single chip info
    } else if (mode_ == Mode::ChipInfo) {
        const ChipInfo ci = parseChipInfo(stdoutBuffer_);
        mode_ = Mode::Idle;
        emit chipInfoReady(ci);
    // Logic chip test
    } else if (mode_ == Mode::Reading) {
        const bool ok = (status == QProcess::NormalExit && exitCode == 0);
        // print debug info
        if (ok) {
            mode_ = Mode::Idle;
            emit readReady(pendingTempPath_);
        } else {
            mode_ = Mode::Idle;
            emit errorLine(QString("[Read error] exit=%1").arg(exitCode));
        }
    // Chip programming
    } else if (mode_ == Mode::Writing) {
        const bool ok = (status == QProcess::NormalExit && exitCode == 0);
        if (ok) {
            mode_ = Mode::Idle;
            emit writeDone();
        } else {
            mode_ = Mode::Idle;
            emit errorLine(QString("[Write error] exit=%1").arg(exitCode));
        }
    } else {
        mode_ = Mode::Idle;
    }

    // All operations eventually end up here, send finished() signal to
    // release the UI.
    stdoutBuffer_.clear();
    emit finished(exitCode, status);
}
