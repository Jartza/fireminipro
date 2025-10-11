// src/ProcessHandling.cpp
#include "ProcessHandling.h"
#include <QRegularExpression>
#include <QTextStream>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>

namespace {
// Remove simple ANSI CSI sequences like "\x1B[K", "\x1B[2K", etc.
static inline QString stripAnsi(QString s) {
    static QRegularExpression ansiRe(R"(\x1B\[[0-9;]*[A-Za-z])");
    return s.remove(ansiRe);
}

// Return 0–100 if a % is found, otherwise -1
static int extractPercent(const QString& line)
{
    // Treat “... OK” endings as 100%, e.g. “123.4 ms  OK”, “12.0 Sec OK”, “Verification OK”
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

static QString detectPhaseText(const QString& line)
{
    static const QRegularExpression wr(R"(\bWriting\s*Code\.\.\.)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rd(R"(\bReading\s*Code\.\.\.)",
        QRegularExpression::CaseInsensitiveOption);

    if (wr.match(line).hasMatch()) return QStringLiteral("Writing");
    if (rd.match(line).hasMatch()) return QStringLiteral("Reading");
    return {};
}
} // namespace

QString ProcessHandling::resolveMiniproPath() {
    QString bin = QStandardPaths::findExecutable("minipro");
    if (bin.isEmpty()) {
        const QStringList candidates = {
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

static QString uniqueTempPath(const QString& base = "fireminipro-read")
{
    const QString tmpRoot =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString ts = QDateTime::currentDateTime().toString("yyMMdd-hhmmss");
    return QDir(tmpRoot).filePath(base + "-" + ts + ".bin");
}

void ProcessHandling::readChipImage(const QString& programmer,
                                    const QString& device,
                                    const QStringList& extraFlags)
{
    const QString bin = resolveMiniproPath();

    // Parse device name without @ending, if one exists
    QString deviceName = device.split('@').first().trimmed();
    QString outPath = uniqueTempPath(deviceName);
    pendingTempPath_ = outPath;

    QStringList args;
    // if (!programmer.trimmed().isEmpty()) {
    //     args << "-q" << programmer.trimmed();
    // }

    args << "-p" << device << "-r" << outPath;
    args << extraFlags;

    mode_ = Mode::Reading;
    stdoutBuffer_.clear();

    emit logLine(QString("[Run] %1 %2").arg(bin).arg(args.join(' ')));

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start();
}

void ProcessHandling::writeChipImage(const QString& programmer,
                                     const QString& device,
                                     const QString& filePath,
                                     const QStringList& extraFlags)
{
    const QString bin = resolveMiniproPath();

    QStringList args;
    // For compatibility with current released minipro, avoid -q here.
    // If you want to use a specific programmer and your minipro supports it, uncomment:
    // if (!programmer.trimmed().isEmpty()) {
    //     args << "-q" << programmer.trimmed();
    // }
    args << "-p" << device << "-w" << filePath;
    args << extraFlags;

    mode_ = Mode::Writing;
    stdoutBuffer_.clear();

    emit logLine(QString("[Run] %1 %2").arg(bin).arg(args.join(' ')));

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start();
}

ProcessHandling::ProcessHandling(QObject *parent)
    : QObject(parent)
{
    connect(&process_, &QProcess::readyReadStandardOutput,
            this, &ProcessHandling::handleStdout);
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
    process_.setProcessChannelMode(QProcess::MergedChannels);
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

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
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

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start();
}

void ProcessHandling::fetchChipInfo(const QString &programmer, const QString &device)
{
    if (process_.state() != QProcess::NotRunning)
        process_.kill();

    const QString bin = resolveMiniproPath();

    // Build args. QProcess handles spaces in args, no manual quoting needed.
    QStringList args;
    if (!programmer.isEmpty())
        args << "-q" << programmer;
    args << "-d" << device;

    emit logLine("[Run] " + bin + " " + args.join(' '));

    mode_ = Mode::ChipInfo;
    stdoutBuffer_.clear();

    process_.setProgram(bin);
    process_.setArguments(args);
    process_.setProcessChannelMode(QProcess::MergedChannels);
    process_.start();
}

void ProcessHandling::sendResponse(const QString &input) {
    if (process_.state() == QProcess::Running) {
        QTextStream(&process_).operator<<(input + "\n");
        process_.waitForBytesWritten(100);
    }
}

void ProcessHandling::handleStdout() {
    const QString all = QString::fromLocal8Bit(process_.readAllStandardOutput());

    const auto lines = all.split('\n');
    for (QString ln : lines) {
        if (ln.isEmpty()) continue;

        ln = stripAnsi(ln).trimmed();
        stdoutBuffer_.append(ln + "\n");

        // We want to log only specific output
        if (ln.contains("error", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("warning", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.contains("invalid", Qt::CaseInsensitive)) {
            emit errorLine(ln);
        } else if (ln.endsWith(" ok", Qt::CaseInsensitive)) {
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

void ProcessHandling::handleFinished(int exitCode, QProcess::ExitStatus status) {
    if (mode_ == Mode::Scan) {
        const QStringList names = parseProgrammerList(stdoutBuffer_);
        mode_ = Mode::Idle;
        emit devicesScanned(names);
    } else if (mode_ == Mode::DeviceList) {
        QStringList devices = stdoutBuffer_.split('\n', Qt::SkipEmptyParts);
        for (QString &s : devices) {
            s = s.trimmed();
        }
        // very light filtering
        devices.erase(std::remove_if(devices.begin(), devices.end(), [](const QString &s){
            return s.isEmpty();
        }), devices.end());
        devices.removeDuplicates();
        mode_ = Mode::Idle;
        emit devicesListed(devices);
    } else if (mode_ == Mode::ChipInfo) {
        const ChipInfo ci = parseChipInfo(stdoutBuffer_);
        mode_ = Mode::Idle;
        emit chipInfoReady(ci);
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
    stdoutBuffer_.clear();
    emit finished(exitCode, status);
}
