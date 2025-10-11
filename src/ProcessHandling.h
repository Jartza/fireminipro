#pragma once
#include <QObject>
#include <QProcess>
#include <QStringList>

class ProcessHandling : public QObject {
    Q_OBJECT
public:
    explicit ProcessHandling(QObject *parent = nullptr);
    void sendResponse(const QString &input);
    // Fire-and-forget scan for connected programmers (minipro -k)
    void scanConnectedDevices();
    void fetchSupportedDevices(const QString &programmer);
    // Fetch information about selected chip (minipro -d "<dev>")
    void fetchChipInfo(const QString &programmer, const QString &device);
    // Read from chip into buffer (minipro -r <file>)
    void readChipImage(const QString& programmer,
                   const QString& device,
                   const QStringList& extraFlags = {});
    void writeChipImage(const QString& programmer,
                        const QString& device,
                        const QString& filePath,
                        const QStringList& extraFlags = {});

    struct ChipInfo {
        QString baseName;      // e.g. "AM2764A"        (may be empty)
        QString package;       // e.g. "DIP28"          (may be empty)
        quint64 bytes = 0;     // 0 if unknown
        int     wordBits = 0;  // 8 for byte-wide, 16 for 16-bit; 0 if unknown
        QString protocol;      // "0x07" or empty
        int     readBuf = 0;   // bytes; 0 if unknown
        int     writeBuf = 0;  // bytes; 0 if unknown
        QString raw;           // full captured text for debugging
        bool    isLogic{};     // true if logic chip, false if eeprom/flash
        int     vectorCount{}; // for logic chips, number of vectors
    };

signals:
    // Normal log output
    void logLine(const QString &text);
    // Error log output
    void errorLine(const QString &text);
    // Parsed progress %
    void progress(int percent, const QString& phase);
    void promptDetected(const QString &promptText);
    void finished(int exitCode, QProcess::ExitStatus status);
    // Emitted after scanConnectedDevices() completes
    void devicesScanned(const QStringList &names);
    void devicesListed(const QStringList &names);
    // Emitted when chip info is fetched
    void chipInfoReady(const ChipInfo &ci);
    // Emitted when chip reading is successful
    void readReady(const QString& tempPath);
    // Emitted when chip writing is done
    void writeDone();

private slots:
    void handleStdout();
    void handleFinished(int exitCode, QProcess::ExitStatus status);

private:
    // Internal mode to disambiguate generic runs vs scans
    enum class Mode { 
        Idle,
        Generic,
        Scan,
        DeviceList,
        ChipInfo,
        Reading,
        Writing,
    };

    Mode    mode_{Mode::Idle};
    QString stdoutBuffer_;
    QString pendingTempPath_;

    QString resolveMiniproPath();
    void startMinipro(Mode mode, const QStringList& args);
    QStringList parseProgrammerList(const QString &text) const;
    ChipInfo parseChipInfo(const QString &text) const;
    QProcess process_;
};
