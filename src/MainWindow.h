#pragma once

#include <QMainWindow>
#include <QByteArray>
#include <QStringList>
#include <QProcess>

class QComboBox;
class QPushButton;
class QTableView;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QProcess;
class QWidget;

// Forward-declare HexView to avoid circular includes in headers
class HexView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void loadBufferFromFile();
    void saveBufferToFile();
    void readFromDevice();
    void writeToDevice();
    void handleProcessOutput();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void loadAtOffsetDialog();

private:
    // left/targets
    QComboBox   *comboProgrammer{};
    QComboBox   *comboDevice{};
    QPushButton *btnRescan{};

    // buffer group
    QPushButton *btnLoad{};     // Clear & Load
    QPushButton *btnLoadAt{};   // Load at offset
    QPushButton *btnSave{};
    QPushButton *btnRead{};
    QPushButton *btnWrite{};
    QCheckBox   *chkAsciiSwap{};
    QLabel      *lblBufSize{};
    QLabel      *lblBufDirty{};

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

    // process
    QProcess *process{};

    // hex model
    HexView *hexModel{};

    // in-memory buffer
    QByteArray buffer_;
    QString    lastPath_;

    // helpers
    QStringList optionFlags() const;
    void setUiEnabled(bool on);
    void updateActionEnabling();

    // parsing / buffer helpers
    bool parseSizeLike(const QString &in, qulonglong &out);
    void ensureBufferSize(int newSize, char padByte);
    void patchBuffer(int offset, const QByteArray &data, char padByte);
};
