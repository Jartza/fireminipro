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
    // Fetch supported devices for a given programmer (minipro -q <programmer> -l)
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
    // Check if chip is blank (minipro -b)
    void checkIfBlank(const QString &programmer,
                      const QString &device,
                      const QStringList &extraFlags = {});
    // Erase chip (minipro -e)
    void eraseChip(const QString &programmer,
                   const QString &device,
                   const QStringList &extraFlags = {});
    // Test logic chip (minipro -T)
    void testLogicChip(const QString &programmer,
                       const QString &device,
                       const QStringList &extraFlags = {});
    
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
    // Emitted when a prompt is detected from the process
    void promptDetected(const QString &promptText);
    // Emitted after scanConnectedDevices() completes
    void devicesScanned(const QStringList &names);
    void devicesListed(const QStringList &names);
    // Emitted when chip info is fetched
    void chipInfoReady(const ChipInfo &ci);
    // Emitted when chip reading is successful
    void readReady(const QString& tempPath);
    // Emitted when chip writing is done
    void writeDone();
    // Emitted when process starts
    void started();
    // Emitted when process finishes    
    void finished(int exitCode, QProcess::ExitStatus status);

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
        Logic,
    };

    Mode    mode_{Mode::Idle};
    QString stdoutBuffer_;
    QString pendingTempPath_;

    QString resolveMiniproPath();
    void startMinipro(Mode mode, const QStringList& args);
    QStringList parseProgrammerList(const QString &text) const;
    ChipInfo parseChipInfo(const QString &text) const;
    static QString stripAnsi(QString s);
    static int extractPercent(const QString &line);
    static QString detectPhaseText(const QString &line);
    QProcess process_;
};
