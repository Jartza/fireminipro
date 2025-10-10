#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QStringList>

class QComboBox;
class QPushButton;
class QTableView;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QWidget;
class HexView;
class QTableWidget;
class ProcessHandling;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void saveBufferToFile();
    void loadAtOffsetDialog();
    void onDevicesScanned(const QStringList &names);
    void onDevicesListed(const QStringList &names);

private:
    // left/targets
    QComboBox   *comboProgrammer{};
    QComboBox   *comboDevice{};
    QPushButton *btnRescan{};

    // buffer group
    QPushButton *btnClear{};
    QPushButton *btnLoad{};
    QPushButton *btnSave{};
    QPushButton *btnRead{};
    QPushButton *btnWrite{};
    QCheckBox   *chkAsciiSwap{};
    QLabel      *lblBufSize{};

    // eeprom options
    QCheckBox *chkBlankCheck{};
    QCheckBox *chkErase{};
    QCheckBox *chkSkipVerify{};
    QCheckBox *chkIgnoreId{};
    QCheckBox *chkSkipId{};
    QCheckBox *chkNoSizeErr{};
    QCheckBox *chkPinCheck{};
    QCheckBox *chkHardwareCheck{};

    // views
    QTableView     *tableHex{};
    QPlainTextEdit *log{};

    // hex model
    HexView *hexModel{};

    // in-memory buffer
    QByteArray buffer_;
    QString    lastPath_;

    // For buffer segment legend storage
    struct BufferSegment {
        qulonglong start{};
        qulonglong length{};
        QString    label;
        QString    note;
    };

    // buffer segment legend
    QList<BufferSegment> bufferSegments{};
    QTableWidget *legendTable{};

    // Process handling helper
    ProcessHandling *proc{};

    // buffer legend manipulation
    void updateLegendTable();
    void addSegmentAndRefresh(qulonglong start, qulonglong length, const QString &label);

    // helpers
    QStringList optionFlags() const;
    void setUiEnabled(bool on);
    void updateActionEnabling();

    // parsing / buffer helpers
    bool parseSizeLike(const QString &in, qulonglong &out);
    void ensureBufferSize(int newSize, char padByte);
    void patchBuffer(int offset, const QByteArray &data, char padByte);
};
