#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QHeaderView>
#include <QFont>
#include <QHeaderView>
#include "HexView.h"
#include "MainWindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("fireminipro");
    resize(1000, 600);

    // left column
    auto *leftBox = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(leftBox);

    // Targets
    auto *groupTargets = new QGroupBox("Targets", leftBox);
    auto *gridT = new QGridLayout(groupTargets);
    comboProgrammer = new QComboBox(groupTargets);
    comboDevice = new QComboBox(groupTargets);
    btnRescan = new QPushButton("Rescan", groupTargets);
    gridT->addWidget(comboProgrammer, 0, 0, 1, 2);
    gridT->addWidget(comboDevice,     1, 0, 1, 2);
    gridT->addWidget(btnRescan,       2, 0);
    groupTargets->setLayout(gridT);
    leftLayout->addWidget(groupTargets);

    // Buffer group
    auto *groupBuffer = new QGroupBox("Buffer", leftBox);
    auto *gridB  = new QGridLayout(groupBuffer);
    btnLoad      = new QPushButton("Load", groupBuffer);
    btnSave      = new QPushButton("Save", groupBuffer);
    btnRead      = new QPushButton("Read",  groupBuffer);
    btnWrite     = new QPushButton("Write", groupBuffer);
    chkAsciiSwap = new QCheckBox("ASCII byteswap", groupBuffer);

    // layout
    gridB->addWidget(btnLoad,      0, 0);
    gridB->addWidget(btnSave,      0, 1);
    gridB->addWidget(btnRead,      1, 0);
    gridB->addWidget(btnWrite,     1, 1);
    gridB->addWidget(chkAsciiSwap, 2, 1);
    groupBuffer->setLayout(gridB);
    leftLayout->addWidget(groupBuffer);

    // EEPROM Options
    auto *groupOpts  = new QGroupBox("EEPROM Options", leftBox);
    auto *gridO      = new QGridLayout(groupOpts);
    chkBlankCheck    = new QCheckBox("Blank check", groupOpts);
    chkErase         = new QCheckBox("Erase", groupOpts);
    chkSkipVerify    = new QCheckBox("Skip verify", groupOpts);
    chkIgnoreId      = new QCheckBox("Ignore ID error", groupOpts);
    chkSkipId        = new QCheckBox("Skip ID check", groupOpts);
    chkNoSizeErr     = new QCheckBox("Ignore size error", groupOpts);
    chkPinCheck      = new QCheckBox("Pin check", groupOpts);
    chkHardwareCheck = new QCheckBox("Hardware check", groupOpts);

    // layout options (2 columns)
    gridO->addWidget(chkBlankCheck,    0,0);
    gridO->addWidget(chkErase,         0,1);
    gridO->addWidget(chkSkipVerify,    1,0);
    gridO->addWidget(chkIgnoreId,      1,1);
    gridO->addWidget(chkSkipId,        2,0);
    gridO->addWidget(chkNoSizeErr,     2,1);
    gridO->addWidget(chkPinCheck,      3,0);
    gridO->addWidget(chkHardwareCheck, 3,1);

    groupOpts->setLayout(gridO);
    leftLayout->addWidget(groupOpts);
    leftLayout->addStretch();

    // right side (hex + log)
    auto *rightSplitter = new QSplitter(Qt::Vertical, central);
    tableHex = new QTableView(rightSplitter);
    log = new QPlainTextEdit(rightSplitter);
    log->setReadOnly(true);
    rightSplitter->addWidget(tableHex);
    rightSplitter->addWidget(log);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 2);

    // Hex view/model
    hexModel = new HexView(this);
    hexModel->setBytesPerRow(16);
    hexModel->setBufferRef(&buffer_);
    tableHex->setModel(hexModel);
    QFont mono; mono.setFamily("Courier New"); mono.setStyleHint(QFont::Monospace);
    tableHex->setFont(mono);
    tableHex->setWordWrap(false);
    tableHex->setAlternatingRowColors(true);
    tableHex->setSelectionBehavior(QAbstractItemView::SelectItems);
    tableHex->verticalHeader()->setDefaultSectionSize(20);

    // header sizing
    auto *hh = tableHex->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Fixed);        // default: fixed
    hh->setStretchLastSection(true);                      // ASCII stretches

    // address column (0) to contents
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    // set a compact width for all hex byte columns (1..bytesPerRow)
    const int bytesPerRow = 16;
    for (int c = 1; c <= bytesPerRow; ++c) tableHex->setColumnWidth(c, 28);  // ~2 hex chars + some padding
    tableHex->setAlternatingRowColors(true);
    tableHex->verticalHeader()->setDefaultSectionSize(20);
    tableHex->horizontalHeader()->setStretchLastSection(true); // ASCII column

    auto *split = new QSplitter(Qt::Horizontal, central);
    split->addWidget(leftBox);
    split->addWidget(rightSplitter);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);

    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->addWidget(split);

    // process wiring
    process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this, &MainWindow::handleProcessOutput);
    connect(process, &QProcess::readyReadStandardError,  this, &MainWindow::handleProcessOutput);
    connect(process, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::handleProcessFinished);
    connect(chkAsciiSwap, &QCheckBox::toggled, hexModel, &HexView::setSwapAscii16);

    // button wiring
    connect(btnLoad,  &QPushButton::clicked, this, &MainWindow::loadBufferFromFile);
    connect(btnSave,  &QPushButton::clicked, this, &MainWindow::saveBufferToFile);
    connect(btnRead,  &QPushButton::clicked, this, &MainWindow::readFromDevice);
    connect(btnWrite, &QPushButton::clicked, this, &MainWindow::writeToDevice);

    // initial state
    setUiEnabled(true);
    btnSave->setEnabled(false);
    btnWrite->setEnabled(false);
}

void MainWindow::setUiEnabled(bool on) {
    for (auto *w : std::vector<QWidget*>{comboProgrammer, comboDevice, btnRescan,
                                         btnLoad, btnSave, btnRead, btnWrite})
        if (w) w->setEnabled(on);
}

QStringList MainWindow::optionFlags() const {
    QStringList f;
    if (chkBlankCheck->isChecked())    f << "--blank_check";
    if (chkErase->isChecked())         f << "--erase";
    if (chkSkipVerify->isChecked())    f << "--skip_verify";
    if (chkIgnoreId->isChecked())      f << "--no_id_error";
    if (chkSkipId->isChecked())        f << "--skip_id";
    if (chkNoSizeErr->isChecked())     f << "--no_size_error";
    if (chkPinCheck->isChecked())      f << "--pin_check";
    if (chkHardwareCheck->isChecked()) f << "--hardware_check";
    return f;
}

// --- Slots ---

void MainWindow::loadBufferFromFile() {
    const QString path = QFileDialog::getOpenFileName(this, tr("Load image"), lastPath_,
        tr("All files (*);;Binary (*.bin);;Intel HEX (*.hex *.ihex *.ihx);;S-Record (*.s19 *.srec)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        log->appendPlainText(QString("[Error] open: %1").arg(f.errorString()));
        return;
    }
    buffer_ = f.readAll();

    // Refresh Hex View
    if (hexModel) {
        hexModel->setBufferRef(&buffer_); // begin/end reset inside ensures view updates
    }

    lastPath_ = QFileInfo(path).absolutePath();
    log->appendPlainText(QString("[Loaded] %1 bytes from %2").arg(buffer_.size()).arg(path));
    btnSave->setEnabled(true);
    btnWrite->setEnabled(!comboDevice->currentText().isEmpty());
    // TODO: update HexView model from buffer_
}

void MainWindow::saveBufferToFile() {
    if (buffer_.isEmpty()) { log->appendPlainText("[Info] Buffer is empty"); return; }
    const QString path = QFileDialog::getSaveFileName(this, tr("Save image"), lastPath_,
        tr("Binary (*.bin);;All files (*)"));
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        log->appendPlainText(QString("[Error] save: %1").arg(f.errorString()));
        return;
    }
    f.write(buffer_);
    log->appendPlainText(QString("[Saved] %1 bytes to %2").arg(buffer_.size()).arg(path));
    lastPath_ = QFileInfo(path).absolutePath();
}

void MainWindow::readFromDevice() {
    const QString dev = comboDevice->currentText();
    if (dev.isEmpty()) { log->appendPlainText("[Warn] Select a device first."); return; }
    // choose a temp file to read into
    const QString path = QDir::temp().filePath("fireminipro-read.bin");
    QStringList args; args << "-p" << dev << "-r" << path << optionFlags();
    log->appendPlainText("[Run] minipro " + args.join(' '));
    setUiEnabled(false);
    process->start("minipro", args);
    // on finish, you could auto-load into buffer_
}

void MainWindow::writeToDevice() {
    const QString dev = comboDevice->currentText();
    if (dev.isEmpty()) { log->appendPlainText("[Warn] Select a device first."); return; }
    if (buffer_.isEmpty()) { log->appendPlainText("[Warn] Load a buffer first."); return; }
    // write requires a file; drop to a temp path
    const QString path = QDir::temp().filePath("fireminipro-write.bin");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) { log->appendPlainText("[Error] tmp save failed"); return; }
    f.write(buffer_); f.close();

    QStringList args; args << "-p" << dev << "-w" << path << optionFlags();
    log->appendPlainText("[Run] minipro " + args.join(' '));
    setUiEnabled(false);
    process->start("minipro", args);
}

void MainWindow::handleProcessOutput() {
    const auto out = QString::fromUtf8(process->readAllStandardOutput()).trimmed();
    const auto err = QString::fromUtf8(process->readAllStandardError()).trimmed();
    if (!out.isEmpty()) log->appendPlainText(out);
    if (!err.isEmpty()) log->appendPlainText(err);
}

void MainWindow::handleProcessFinished(int exitCode, QProcess::ExitStatus) {
    setUiEnabled(true);
    log->appendPlainText(QString("[Done] exit=%1").arg(exitCode));
    // You might decide to auto-load read file into buffer_ here
}
