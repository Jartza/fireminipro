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
#include <QLabel>
#include <QDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QLocale>
#include <QPainter>
#include <QTableWidget>
#include <QHeaderView>
#include <algorithm>

#include "MainWindow.h"
#include "HexView.h"

struct BufferSegment {
    qulonglong start{};
    qulonglong length{};
    QString    label;   // filename
    QString    note;    // "", " (partial)", " (overlap)"
};
static void updateLegendTable(QWidget *parent, const QList<BufferSegment> &segs);
static void addSegmentAndRefresh(QWidget *parent, qulonglong start, qulonglong length, const QString &label);
static QList<BufferSegment> gSegments; // simple per-process storage

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("fireminipro");
    resize(1000, 760);

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
    btnLoad      = new QPushButton("Clear && Load…", groupBuffer);
    btnSave      = new QPushButton("Save", groupBuffer);
    btnRead      = new QPushButton("Read",  groupBuffer);
    btnWrite     = new QPushButton("Write", groupBuffer);
    chkAsciiSwap = new QCheckBox("ASCII byteswap (16-bit)", groupBuffer);

    // layout (Buffer)
    // Desired order:
    //  Row 0: Clear & Load…  (spans 2 cols)
    //  Row 1: Load at offset… (left)   | Save (right)
    //  Row 2: ASCII byteswap (16-bit)  (spans 2 cols)
    //  Row 3: Size: ...                 (spans 2 cols)
    //  Row 4: Modified: ...             (spans 2 cols)

    // ensure extra actions and labels exist
    btnLoadAt    = new QPushButton("Load at offset…", groupBuffer);
    if (!lblBufSize)  lblBufSize  = new QLabel("Size: 0 (0x0)", groupBuffer);
    if (!lblBufDirty) lblBufDirty = new QLabel("Modified: 0",   groupBuffer);

    gridB->addWidget(btnLoad,      0, 0, 1, 2);
    gridB->addWidget(btnLoadAt,    1, 0, 1, 1);
    gridB->addWidget(btnSave,      1, 1, 1, 1);
    gridB->addWidget(btnRead,      2, 0, 1, 1);
    gridB->addWidget(btnWrite,     2, 1, 1, 1);
    gridB->addWidget(chkAsciiSwap, 3, 0, 1, 2);
    gridB->addWidget(lblBufSize,   4, 0, 1, 2);
    gridB->addWidget(lblBufDirty,  5, 0, 1, 2);

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

    // right side (hex + legend + log)
    auto *rightSplitter = new QSplitter(Qt::Vertical, central);
    tableHex = new QTableView(rightSplitter);

    // Buffer legend table
    auto *legendTable = new QTableWidget(rightSplitter);
    legendTable->setObjectName("bufferLegendTable");
    legendTable->setColumnCount(4);
    legendTable->setHorizontalHeaderLabels({tr("Start"), tr("End"), tr("Size"), tr("File")});
    legendTable->horizontalHeader()->setStretchLastSection(true);
    legendTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    legendTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    legendTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    legendTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    legendTable->setSelectionMode(QAbstractItemView::NoSelection);
    legendTable->setFocusPolicy(Qt::NoFocus);
    legendTable->setMinimumHeight(152);

    log = new QPlainTextEdit(rightSplitter);
    log->setReadOnly(true);

    rightSplitter->addWidget(tableHex);
    rightSplitter->addWidget(legendTable);
    rightSplitter->addWidget(log);
    rightSplitter->setStretchFactor(0, 5);
    rightSplitter->setStretchFactor(1, 1);
    rightSplitter->setStretchFactor(2, 3);

    // Hex view/model
    hexModel = new HexView(this);
    // hexModel->setBytesPerRow(16); // Default is 16
    hexModel->setBufferRef(&buffer_);
    tableHex->setModel(hexModel);
    QFont mono; mono.setFamily("Courier New"); mono.setStyleHint(QFont::Monospace);
    tableHex->setFont(mono);
    tableHex->setWordWrap(false);
    tableHex->setAlternatingRowColors(true);
    tableHex->setSelectionBehavior(QAbstractItemView::SelectItems);
    tableHex->verticalHeader()->setDefaultSectionSize(20);

    // Hexviewer header sizing
    auto *hh = tableHex->horizontalHeader();
    hh->setSectionResizeMode(QHeaderView::Fixed);
    hh->setStretchLastSection(true);

    // address column (0) to contents
    hh->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    // set a compact width for all hex byte columns (1..bytesPerRow)
    for (int c = 1; c <= hexModel->getBytesPerRow(); ++c) tableHex->setColumnWidth(c, 28);  // ~2 hex chars + some padding
    tableHex->setAlternatingRowColors(true);
    tableHex->verticalHeader()->setDefaultSectionSize(20);
    // ASCII column
    tableHex->horizontalHeader()->setStretchLastSection(true);

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
    connect(chkAsciiSwap, &QCheckBox::toggled, this, [this](bool on){
        if (hexModel) hexModel->setSwapAscii16(on);
    });

    // button wiring
    connect(btnLoad, &QPushButton::clicked, this, [this]{
        buffer_.clear();
        gSegments.clear();
        updateLegendTable(this, gSegments);
        if (hexModel) hexModel->setBufferRef(&buffer_);
        if (lblBufDirty) lblBufDirty->setText("Modified: 0");
        if (lblBufSize)  lblBufSize->setText("Size: 0 (0x0)");
        loadBufferFromFile();
    });
    connect(btnSave,  &QPushButton::clicked, this, &MainWindow::saveBufferToFile);
    connect(btnRead,  &QPushButton::clicked, this, &MainWindow::readFromDevice);
    connect(btnWrite, &QPushButton::clicked, this, &MainWindow::writeToDevice);
    connect(btnLoadAt, &QPushButton::clicked, this, &MainWindow::loadAtOffsetDialog);
    connect(comboDevice, &QComboBox::currentTextChanged,
        this, [this](const QString&){ updateActionEnabling(); });

    // initial state
    setUiEnabled(true);
    updateActionEnabling();
}

void MainWindow::setUiEnabled(bool on) {
    for (auto *w : std::vector<QWidget*>{comboProgrammer, comboDevice, btnRescan,
                                         btnLoad, btnSave, btnRead, btnWrite})
        if (w) w->setEnabled(on);
}

void MainWindow::updateActionEnabling() {
    const bool deviceSelected = !comboDevice->currentText().isEmpty();
    const bool hasBuffer      = !buffer_.isEmpty();

    if (btnRead)  btnRead->setEnabled(deviceSelected);               // Read needs device
    if (btnWrite) btnWrite->setEnabled(deviceSelected && hasBuffer); // Write needs device + buffer
    if (btnSave)  btnSave->setEnabled(hasBuffer);                    // Save needs buffer
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

    // Reset legend to this single file
    gSegments.clear();
    addSegmentAndRefresh(this, 0, static_cast<qulonglong>(buffer_.size()), QFileInfo(path).fileName());

    // Refresh Hex View
    if (hexModel) {
        hexModel->setBufferRef(&buffer_); // begin/end reset inside ensures view updates
    }

    lastPath_ = QFileInfo(path).absolutePath();
    log->appendPlainText(QString("[Loaded] %1 bytes from %2").arg(buffer_.size()).arg(path));
    updateActionEnabling();

    if (lblBufSize)
        lblBufSize->setText(QString("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
    if (lblBufDirty && hexModel)
        lblBufDirty->setText(QString("Modified: %1").arg(hexModel->dirtyCount()));
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
    updateActionEnabling();
    log->appendPlainText(QString("[Done] exit=%1").arg(exitCode));
    // You might decide to auto-load read file into buffer_ here
}

// --- helpers ---------------------------------------------------------------

// parse sizes like "0x1F000", "8192", "32k", "512K", "1M", "256KB", "2MB"
bool MainWindow::parseSizeLike(const QString &in, qulonglong &out) {
    QString s = in.trimmed();
    if (s.isEmpty()) return false;

    qulonglong mul = 1;
    if (s.endsWith("KB", Qt::CaseInsensitive)) { mul = 1024ULL; s.chop(2); }
    else if (s.endsWith("MB", Qt::CaseInsensitive)) { mul = 1024ULL*1024ULL; s.chop(2); }
    else if (!s.isEmpty() && (s.back() == QLatin1Char('k') || s.back() == QLatin1Char('K'))) { mul = 1024ULL; s.chop(1); }
    else if (!s.isEmpty() && (s.back() == QLatin1Char('m') || s.back() == QLatin1Char('M'))) { mul = 1024ULL*1024ULL; s.chop(1); }

    bool ok = false;
    qulonglong base = 0;
    s = s.trimmed();
    if (s.startsWith("0x", Qt::CaseInsensitive)) {
        base = s.mid(2).toULongLong(&ok, 16);
    } else {
        base = s.toULongLong(&ok, 10);
    }
    if (!ok) return false;
    out = base * mul;
    return true;
}

void MainWindow::ensureBufferSize(int newSize, char padByte) {
    if (newSize <= buffer_.size()) return;
    const int growBy = newSize - buffer_.size();
    buffer_.append(QByteArray(growBy, padByte));
}

void MainWindow::patchBuffer(int offset, const QByteArray &data, char padByte) {
    if (offset < 0 || data.isEmpty()) return;
    const int oldSize = buffer_.size();
    const int end = offset + data.size();
    ensureBufferSize(end, padByte);
    const bool grew = buffer_.size() > oldSize;

    std::copy(data.begin(), data.end(), buffer_.begin() + offset);

    if (hexModel) {
        if (grew) {
            // Row count changed; reset model so the view reflects new size
            hexModel->setBufferRef(&buffer_);
        } else {
            const int bpr = std::max(1, hexModel->getBytesPerRow());
            const int firstRow = offset / bpr;
            const int lastRow  = (end - 1) / bpr;
            emit hexModel->dataChanged(hexModel->index(firstRow, 0),
                                       hexModel->index(lastRow, hexModel->columnCount() - 1));
        }
    }
}

// Simple graphical preview bar for Load-at-Offset
class LoadPreviewBar : public QWidget {
public:
    explicit LoadPreviewBar(QWidget *parent=nullptr) : QWidget(parent) {
        setMinimumHeight(88);
    }
    void setParams(qulonglong bufSize, qulonglong off, qulonglong dataLen, qulonglong padLen) {
        bufSize_ = bufSize; off_ = off; dataLen_ = dataLen; padLen_ = padLen; update();
    }
protected:
    QSize sizeHint() const override { return QSize(420, 92); }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);

        const int W = width();
        const int H = height();
        const int barH = 16;
        const int y = 8; // top margin

        // Determine total span to visualize
        qulonglong total = bufSize_;
        qulonglong prePadLen = 0;
        if (off_ > bufSize_) prePadLen = off_ - bufSize_;
        const qulonglong newEnd = (off_ + dataLen_ + padLen_);
        if (newEnd > total) total = newEnd;
        if (total == 0) {
            p.fillRect(0, y, W, barH, QColor(230,230,230));
            p.setPen(Qt::gray);
            p.drawRect(0, y, W-1, barH);
            p.drawText(6, y+barH+16, "(empty)");
            return;
        }

        auto xFor = [&](qulonglong v){ return int((double(v) / double(total)) * (W-2)) + 1; };

        // Background (gap/empty) light gray
        p.fillRect(0, y, W, barH, QColor(235,235,235));
        p.setPen(QColor(180,180,180));
        p.drawRect(0, y, W-1, barH);

        // Existing buffer region [0, bufSize_)
        if (bufSize_ > 0) {
            int x0 = xFor(0), x1 = xFor(bufSize_);
            p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(180,180,180)); // darker gray
        }

        // Pre-padding from buffer end to offset (if any)
        if (prePadLen > 0) {
            int x0 = xFor(bufSize_);
            int x1 = xFor(off_);
            p.fillRect(x0, y, qMax(1, x1 - x0), barH, QColor(250, 220, 120)); // yellow
        }

        // New data region [off_, off_+dataLen_)
        if (dataLen_ > 0) {
            int x0 = xFor(qMin(off_, total));
            int x1 = xFor(qMin(off_ + dataLen_, total));
            p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(120, 200, 120)); // green
        }

        bool hasOverlap = false;

        // Overlap: portion of new data that overwrites existing buffer [0, bufSize_)
        if (dataLen_ > 0 && bufSize_ > 0) {
            const qulonglong dataStart = off_;
            const qulonglong dataEnd   = off_ + dataLen_;
            // True intersection of [dataStart, dataEnd) with [0, bufSize_)
            const qulonglong ovStart = std::max<qulonglong>(dataStart, 0);
            const qulonglong ovEnd   = std::min<qulonglong>(dataEnd,   bufSize_);
            if (ovEnd > ovStart) {
                int xr0 = xFor(ovStart);
                int xr1 = xFor(ovEnd);
                QColor red(220, 80, 80, 180); // semi-transparent red overlay
                p.fillRect(xr0, y, qMax(1, xr1 - xr0), barH, red);
                hasOverlap = true;
            }
        }

        // Padding region [off_+dataLen_, off_+dataLen_+padLen_)
        if (padLen_ > 0) {
            int x0 = xFor(qMin(off_ + dataLen_, total));
            int x1 = xFor(qMin(off_ + dataLen_ + padLen_, total));
            p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(250, 220, 120)); // yellow
        }

        // Simple tick labels
        p.setPen(Qt::black);
        QFont f = p.font(); f.setPointSizeF(f.pointSizeF()-1); p.setFont(f);
        const QString startTxt = QString("0x") + QString::number(0,16).toUpper();
        const QString offTxt   = QString("0x") + QString::number(off_,16).toUpper();
        const QString endTxt   = QString("0x") + QString::number(total ? total-1 : 0,16).toUpper();
        p.drawText(4, y+barH+14, startTxt);
        // draw off only if meaningful and inside range
        if (off_ > 0 && off_ < total) {
            int xo = xFor(off_);
            p.setPen(QColor(80,80,80));
            p.drawLine(xo, y+barH, xo, y+barH+4);
            p.setPen(Qt::black);
            p.drawText(qMin(qMax(4, xo-40), W-60), y+barH+14, offTxt);
        }
        p.drawText(W-4 - p.fontMetrics().horizontalAdvance(endTxt), y+barH+14, endTxt);

        int ly = y + barH + 30;
        auto legend = [&](QColor c, const QString &t, int &lx){
            p.fillRect(lx, ly-10, 10, 10, c);
            p.setPen(Qt::black);
            p.drawRect(lx, ly-10, 10, 10);
            p.drawText(lx+14, ly, t);
            lx += 14 + p.fontMetrics().horizontalAdvance(t) + 12;
        };
        const bool hasBuffer  = (bufSize_ > 0);
        // dataLen_ is the file portion (green) that will be written
        const bool hasData    = (dataLen_ > 0);
        const bool hasPadding = (prePadLen > 0) || (padLen_ > 0);
        int lx = 4;
        if (hasBuffer)  legend(QColor(180,180,180), tr("buffer"),  lx);
        if (hasData)    legend(QColor(120,200,120), tr("data"),    lx);
        if (hasPadding) legend(QColor(250,220,120), tr("padding"), lx);
        if (hasOverlap) legend(QColor(220, 80, 80), tr("overlap"), lx);
    }
private:
    qulonglong bufSize_{};
    qulonglong off_{};
    qulonglong dataLen_{};
    qulonglong padLen_{};
};

// --- UI: Load at offset dialog --------------------------------------------

void MainWindow::loadAtOffsetDialog() {
    // 1) Pick file FIRST so we know its size and can default length accordingly
    const QString path = QFileDialog::getOpenFileName(this, tr("Pick image"), lastPath_,
        tr("All files (*);;Binary (*.bin)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        log->appendPlainText(QString("[Error] open: %1").arg(f.errorString()));
        return;
    }
    const QByteArray file = f.readAll();
    const qulonglong fileSize = static_cast<qulonglong>(file.size());

    // 2) Ask for offset/length/pad now that we know the file size
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Load at offset"));
    dlg.setMinimumSize(620, 420);          // give headroom for large numbers
    dlg.setSizeGripEnabled(true);          // user can grow the dialog
    auto *grid = new QGridLayout(&dlg);
    auto *leftForm = new QFormLayout();
    auto *infoGroup = new QGroupBox(tr("Info"), &dlg);
    auto *infoLayout = new QVBoxLayout(infoGroup);

    // Show selected file name (no path) at the top for user context
    const QString baseName = QFileInfo(path).fileName();
    auto *lblFile = new QLabel(baseName, &dlg);
    lblFile->setToolTip(path); // show full path on hover
    lblFile->setTextInteractionFlags(Qt::TextSelectableByMouse);
    {
        QFont bf = lblFile->font();
        bf.setBold(true);
        lblFile->setFont(bf);
    }

    // Default offset = end of current buffer
    auto *editOff = new QLineEdit(&dlg);
    {
        const qulonglong defOff = static_cast<qulonglong>(buffer_.size());
        const QString hx = QString::number(defOff, 16).toUpper();
        editOff->setText(QString("0x") + hx);
    }
    // Default length = whole file
    auto *editLen = new QLineEdit(&dlg);
    editLen->setText(QString("0x%1").arg(QString::number(fileSize, 16).toUpper()));
    auto *editPad = new QLineEdit(&dlg);
    editPad->setText("0xFF");

    // Live helper labels
    auto *lblOffInfo = new QLabel(&dlg);
    auto *lblLenInfo = new QLabel(&dlg);
    auto *lblEndInfo = new QLabel(&dlg);
    for (QLabel* L : {lblOffInfo, lblLenInfo, lblEndInfo}) {
        L->setWordWrap(true);
        L->setTextFormat(Qt::RichText);   // allow <b> headings
        QPalette pal = L->palette();
        pal.setColor(QPalette::WindowText, Qt::black);
        L->setPalette(pal);
        infoLayout->addWidget(L);
    }

    // Preview widget
    auto *preview = new LoadPreviewBar(&dlg);

    // --- Layout: add widgets to left (inputs) and right (info box) ---
    leftForm->addRow(tr("File:"), lblFile);
    leftForm->addRow(tr("Offset:"), editOff);
    leftForm->addRow(tr("Length:"), editLen);
    leftForm->addRow(tr("Pad byte:"), editPad);

    grid->addLayout(leftForm, 0, 0);
    grid->addWidget(infoGroup, 0, 1);
    grid->addWidget(preview, 1, 0, 1, 2);

    // make columns feel balanced
    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 3);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    auto *okBtn = bb->button(QDialogButtonBox::Ok);
    okBtn->setEnabled(false);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    grid->addWidget(bb, 2, 0, 1, 2);

    auto updateInfo = [this, editOff, editLen, editPad, lblOffInfo, lblLenInfo, lblEndInfo, fileSize, okBtn, preview]() {
        qulonglong off=0, lenReq=0, padTmp=0xFF;
        const bool offOk = parseSizeLike(editOff->text(), off);
        bool lenOk = parseSizeLike(editLen->text(), lenReq);
        const bool padOk = parseSizeLike(editPad->text(), padTmp) && padTmp <= 0xFF;
        if (!lenOk) lenReq = 0;
        const bool haveLen = (lenOk && lenReq > 0);
        const qulonglong effLen = haveLen ? lenReq : fileSize;
        // Offset info (bold heading)
        if (offOk) {
            lblOffInfo->setText(QString("<b>Offset:</b> 0x%1 (%2)")
                                .arg(QString::number(off, 16).toUpper())
                                .arg(QLocale().toString(off)));
        } else {
            lblOffInfo->setText(QString("<b>Offset:</b> <span style='color:#c00'>invalid</span>"));
        }
        // Length + File info (bold headings, multiline)
        if (haveLen) {
            lblLenInfo->setText(QString("<b>Length:</b> 0x%1 (%2)<br/><b>File size:</b> 0x%3 (%4)")
                                .arg(QString::number(lenReq, 16).toUpper())
                                .arg(QLocale().toString(lenReq))
                                .arg(QString::number(fileSize, 16).toUpper())
                                .arg(QLocale().toString(fileSize)));
        } else {
            lblLenInfo->setText(QString("<b>Length:</b> whole file<br/><b>File size:</b> 0x%1 (%2)")
                                .arg(QString::number(fileSize, 16).toUpper())
                                .arg(QLocale().toString(fileSize)));
        }
        // End address / new buffer size (bold headings, multiline)
        if (offOk && effLen > 0) {
            const qulonglong endAddr = off + effLen - 1;
            const qulonglong newSizeIfAppend = std::max<qulonglong>(buffer_.size(), off + effLen);
            lblEndInfo->setText(QString("<b>End address:</b> 0x%1 (%2)<br/><b>New buffer size:</b> 0x%3 (%4)")
                                .arg(QString::number(endAddr, 16).toUpper())
                                .arg(QLocale().toString(endAddr))
                                .arg(QString::number(newSizeIfAppend, 16).toUpper())
                                .arg(QLocale().toString(newSizeIfAppend)));
        } else {
            lblEndInfo->clear();
        }
        // Calculate preview parts: file portion (green) and post-data padding (yellow)
        const qulonglong filePart = haveLen ? std::min<qulonglong>(lenReq, fileSize) : fileSize;
        const qulonglong postPad  = (haveLen && lenReq > fileSize) ? (lenReq - fileSize) : 0;
        const bool prePadNeeded = offOk && (off > static_cast<qulonglong>(buffer_.size()));
        editPad->setEnabled(postPad > 0 || prePadNeeded);
        preview->setParams(static_cast<qulonglong>(buffer_.size()), offOk ? off : 0, filePart, postPad);
        okBtn->setEnabled(offOk && (effLen > 0) && padOk);
    };

    QObject::connect(editOff, &QLineEdit::textChanged, &dlg, updateInfo);
    QObject::connect(editLen, &QLineEdit::textChanged, &dlg, updateInfo);
    QObject::connect(editPad, &QLineEdit::textChanged, &dlg, updateInfo);

    updateInfo();
    if (dlg.exec() != QDialog::Accepted) return;

    // Parse final values
    qulonglong off=0, len=0, pad=0xFF;
    if (!parseSizeLike(editOff->text(), off)) { log->appendPlainText("[Error] invalid offset"); return; }
    if (!editLen->text().trimmed().isEmpty() && !parseSizeLike(editLen->text(), len)) {
        log->appendPlainText("[Error] invalid length"); return;
    }
    if (!parseSizeLike(editPad->text(), pad) || pad > 0xFF) {
        log->appendPlainText("[Error] invalid pad"); return;
    }

    // Determine effective length: default to full file if length omitted/invalid
    const qulonglong effLen = (len == 0) ? fileSize : len;

    // Build a data block of effLen: file bytes then padding (if requested > file)
    QByteArray data;
    data.reserve(static_cast<int>(effLen));
    const qulonglong take = std::min<qulonglong>(effLen, fileSize);
    data.append(file.left(static_cast<int>(take)));
    if (effLen > take) data.append(QByteArray(static_cast<int>(effLen - take), char(pad & 0xFF)));

    // Patch the buffer with PAD used also for growth between current size and offset
    patchBuffer(static_cast<int>(off), data, char(pad & 0xFF));
    addSegmentAndRefresh(this, off, effLen, QFileInfo(path).fileName());
    lastPath_ = QFileInfo(path).absolutePath();

    log->appendPlainText(QString("[Loaded] %1 bytes at 0x%2 from %3")
                         .arg(QLocale().toString(effLen))
                         .arg(QString::number(off, 16).toUpper())
                         .arg(path));

    if (lblBufSize)
        lblBufSize->setText(QString("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
    if (lblBufDirty && hexModel)
        lblBufDirty->setText(QString("Modified: %1").arg(hexModel->dirtyCount()));

    updateActionEnabling();
}

// --- Buffer segment legend helpers ---
static void updateLegendTable(QWidget *parent, const QList<BufferSegment> &segs) {
    auto *legendTable = parent->findChild<QTableWidget*>("bufferLegendTable");
    if (!legendTable) return;
    legendTable->setRowCount(segs.size());
    int row = 0;
    for (const auto &s : segs) {
        const qulonglong end = s.length ? (s.start + s.length - 1) : s.start;
        auto *itStart = new QTableWidgetItem(QString("0x") + QString::number(s.start, 16));
        auto *itEnd   = new QTableWidgetItem(QString("0x") + QString::number(end,   16));
        auto *itSize  = new QTableWidgetItem(QString("%1 (0x%2)")
                                             .arg(QString::number(s.length))
                                             .arg(QString::number(s.length, 16)));
        QString label = s.label;
        if (!s.note.isEmpty()) label += s.note;   // append " (partial)" or " (overlap)"
        auto *itLabel = new QTableWidgetItem(label);
        legendTable->setItem(row, 0, itStart);
        legendTable->setItem(row, 1, itEnd);
        legendTable->setItem(row, 2, itSize);
        legendTable->setItem(row, 3, itLabel);
        ++row;
    }
    legendTable->resizeRowsToContents();
}

static void addSegmentAndRefresh(QWidget *parent, qulonglong start, qulonglong length, const QString &label) {
    const qulonglong nBeg = start;
    const qulonglong nEnd = start + length; // half-open

    QList<BufferSegment> out;
    out.reserve(gSegments.size() + 1);

    bool anyPartialOverlap = false; // becomes true only if an overlapped old segment has a remainder (left or right)

    for (const auto &seg : std::as_const(gSegments)) {
        const qulonglong sBeg = seg.start;
        const qulonglong sEnd = seg.start + seg.length; // half-open

        // No overlap -> keep as-is
        if (sEnd <= nBeg || nEnd <= sBeg) {
            out.append(seg);
            continue;
        }

        // There is some overlap. Decide if it's partial (i.e., any remainder exists).
        const bool hasLeftRemainder  = (sBeg < nBeg);
        const bool hasRightRemainder = (nEnd < sEnd);
        if (hasLeftRemainder || hasRightRemainder) {
            anyPartialOverlap = true;
        }

        // Left remainder [sBeg, nBeg)
        if (hasLeftRemainder) {
            BufferSegment left{ sBeg, nBeg - sBeg, seg.label, QStringLiteral(" (partial)") };
            out.append(left);
        }

        // Middle [max(sBeg,nBeg), min(sEnd,nEnd)) is fully covered by the new segment -> drop

        // Right remainder [nEnd, sEnd)
        if (hasRightRemainder) {
            BufferSegment right{ nEnd, sEnd - nEnd, seg.label, QStringLiteral(" (partial)") };
            out.append(right);
        }
    }

    // Add the new segment; mark as overlap only when the overlap was partial (not exact full cover)
    BufferSegment added{ nBeg, length, label, anyPartialOverlap ? QStringLiteral(" (overlap)") : QString() };
    out.append(added);

    // Sort by start
    std::sort(out.begin(), out.end(), [](const BufferSegment &a, const BufferSegment &b){
        return a.start < b.start;
    });

    // Coalesce adjacent entries that share the same label+note and are contiguous
    QList<BufferSegment> coalesced;
    for (const auto &s : std::as_const(out)) {
        if (!coalesced.isEmpty()) {
            auto &back = coalesced.last();
            const bool sameTag = (back.label == s.label) && (back.note == s.note);
            const bool contiguous = (back.start + back.length == s.start);
            if (sameTag && contiguous) {
                back.length += s.length;
                continue;
            }
        }
        coalesced.append(s);
    }

    gSegments = std::move(coalesced);
    updateLegendTable(parent, gSegments);
}
