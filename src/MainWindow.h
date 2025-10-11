#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QStringList>
#include "ProcessHandling.h"

class QComboBox;
class QPushButton;
class QTableView;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QWidget;
class HexView;
class QTableWidget;
class QProgressBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void saveBufferToFile();
    void loadAtOffsetDialog(QString path = {});
    void onDevicesScanned(const QStringList &names);
    void onDevicesListed(const QStringList &names);

private:
    // Target and device
    QComboBox   *comboProgrammer{};
    QComboBox   *comboDevice{};
    QPushButton *btnRescan{};

    // Chip information
    QLabel      *chipName{};
    QLabel      *chipPackage{};
    QLabel      *chipMemory{};
    QLabel      *chipBusWidth{};
    QLabel      *chipProtocol{};
    QLabel      *chipReadBuf{};
    QLabel      *chipWriteBuf{};

    // Buffer group
    QPushButton *btnClear{};
    QPushButton *btnLoad{};
    QPushButton *btnSave{};
    QPushButton *btnRead{};
    QPushButton *btnWrite{};
    QCheckBox   *chkAsciiSwap{};
    QLabel      *lblBufSize{};

    // Device operations
    QPushButton *btnBlankCheck{};
    QPushButton *btnEraseDevice{};
    QPushButton *btnTestLogic{};

    // Device options
    QCheckBox *chkSkipVerify{};
    QCheckBox *chkIgnoreId{};
    QCheckBox *chkSkipId{};
    QCheckBox *chkNoSizeErr{};

    // Views
    QTableView     *tableHex{};
    QPlainTextEdit *log{};

    // Hex view model
    HexView *hexModel{};

    // Progress bar
    QProgressBar* progReadWrite{};

    // In-memory buffer
    QByteArray buffer_;
    QString    lastPath_;

    // Buffer segment legend storage
    struct BufferSegment {
        qulonglong start{};
        qulonglong length{};
        QString    label;
        QString    note;
    };

    // Buffer segment legend
    QList<BufferSegment> bufferSegments{};
    QTableWidget *legendTable{};

    // Process handling helper
    ProcessHandling *proc{};

    // Buffer legend manipulation
    void updateLegendTable();
    void addSegmentAndRefresh(qulonglong start, qulonglong length, const QString &label);

    // Helpers
    QStringList optionFlags() const;
    void setUiEnabled(bool on);
    void updateActionEnabling();
    void updateChipInfo(const ProcessHandling::ChipInfo &ci);
    void clearChipInfo();

    // parsing / buffer helpers
    bool parseSizeLike(const QString &in, qulonglong &out);
    void ensureBufferSize(int newSize, char padByte);
    void patchBuffer(int offset, const QByteArray &data, char padByte);

protected:
      bool eventFilter(QObject *obj, QEvent *event) override;

};
