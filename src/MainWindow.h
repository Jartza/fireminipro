#pragma once
#include <QMainWindow>
#include <QProcess>

class QComboBox;
class QPushButton;
class QTableView;
class QPlainTextEdit;
class QSplitter;
class QCheckBox;
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

private:
    HexView *hexModel{};        // model
    QCheckBox *chkAsciiSwap{};  // toggle (put in your options group)

private:
    // left/targets
    QComboBox *comboProgrammer{};
    QComboBox *comboDevice{};
    QPushButton *btnRescan{};

    // buffer group
    QPushButton *btnLoad{};
    QPushButton *btnSave{};
    QPushButton *btnRead{};
    QPushButton *btnWrite{};

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
    QTableView *tableHex{};
    QPlainTextEdit *log{};

    // process
    QProcess *process{};

    // simple in-memory buffer (replace with your HexView backing later)
    QByteArray buffer_;
    QString    lastPath_;

    QStringList optionFlags() const; // build flags from checkboxes
    void setUiEnabled(bool on);
};
