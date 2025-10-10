#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QPlainTextEdit>
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
#include <QTimer>
#include <QSortFilterProxyModel>
#include <QCompleter>
#include <QProgressBar>
#include <algorithm>

#include "ProcessHandling.h"
#include "MainWindow.h"
#include "HexView.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    auto *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("fireminipro");
    resize(1000, 760);

    // left column
    auto *leftBox = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(leftBox);

    // Target
    auto *groupTargets = new QGroupBox("Target", leftBox);
    auto *gridT = new QGridLayout(groupTargets);
    comboProgrammer = new QComboBox(groupTargets);
    comboDevice = new QComboBox(groupTargets);
    auto *lblProg = new QLabel("Programmer:", groupTargets);
    auto *lblDev  = new QLabel("Device:",     groupTargets);
    QFont small = this->font();
    small.setPointSizeF(this->font().pointSizeF() - 1);
    lblProg->setFont(small);
    lblDev->setFont(small);
    btnRescan = new QPushButton("Rescan programmers", groupTargets);
    gridT->addWidget(lblProg,         0, 0);
    gridT->addWidget(comboProgrammer, 0, 1);
    gridT->addWidget(lblDev,          1, 0);
    gridT->addWidget(comboDevice,     1, 1);
    gridT->addWidget(btnRescan,       2, 0, 1, 2, Qt::AlignRight);
    gridT->setColumnStretch(0, 0);
    gridT->setColumnStretch(1, 1);
    comboProgrammer->setPlaceholderText("No programmer");
    comboDevice->setPlaceholderText("No devices");
    groupTargets->setLayout(gridT);
    leftLayout->addWidget(groupTargets);

    // Make device combobox searchable / filterable
    comboDevice->setEditable(true);
    comboDevice->setInsertPolicy(QComboBox::NoInsert);
    comboDevice->setFocusPolicy(Qt::StrongFocus);
    comboDevice->lineEdit()->setClearButtonEnabled(true);

    auto *filter_model = new QSortFilterProxyModel(comboDevice);
    filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filter_model->setSourceModel(comboDevice->model());

    auto *completer = new QCompleter(filter_model, comboDevice);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    comboDevice->setCompleter(completer);

    // When selection changes, update action enabling
    connect(comboDevice, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int idx){
            updateActionEnabling();
            if (idx < 0) {
                // Optional: if selection cleared, clear the chip info too
                clearChipInfo();
            }
        });

    // fetch only on user action
    connect(comboDevice, qOverload<int>(&QComboBox::activated),
        this, [this](int idx){
            if (idx >= 0 && proc) {
                const QString p = comboProgrammer->currentText().trimmed();
                const QString d = comboDevice->itemText(idx).trimmed();
                if (!p.isEmpty() && !d.isEmpty()) {
                    clearChipInfo();
                    proc->fetchChipInfo(p, d);
                }
            }
        });

    connect(comboDevice->lineEdit(),
      &QLineEdit::textEdited,
      this,
      [this, filter_model](const QString &text) {
        filter_model->setFilterFixedString(text);
      }
    );

    connect(comboDevice,
      &QComboBox::textActivated,
      comboDevice->lineEdit(),
      [cb = comboDevice]() {
        cb->lineEdit()->setProperty("clearOnFirstClick", true);
      }
    );

    // If user clears the text, we should clear selection
     connect(comboDevice->lineEdit(), &QLineEdit::textChanged,
        this, [this](const QString &t){
            if (t.isEmpty()) {
                QSignalBlocker block(comboDevice);
                comboDevice->setCurrentIndex(-1);
                updateActionEnabling();
            }
        });


    comboDevice->lineEdit()->installEventFilter(this);
    comboDevice->lineEdit()->setProperty("clearOnFirstClick", true);

    // Chip information (between Target and Buffer)
    auto *groupChipInfo = new QGroupBox("Chip information", leftBox);
    auto *gridC = new QGridLayout(groupChipInfo);

    // left column (keys), right column (values)
    auto addRow = [&](int r, const char *key, QLabel *&valueOut) {
        auto *k = new QLabel(QString::fromUtf8(key), groupChipInfo);
        k->setFont(small);
        k->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueOut = new QLabel("-", groupChipInfo);
        valueOut->setTextInteractionFlags(Qt::TextSelectableByMouse);
        gridC->addWidget(k, r, 0);
        gridC->addWidget(valueOut, r, 1);
    };

    // rows
    int r = 0;
    addRow(r++, "Name:",           chipName);
    addRow(r++, "Package:",        chipPackage);
    addRow(r++, "Memory:",         chipMemory);
    addRow(r++, "Mem width:",      chipBusWidth);
    addRow(r++, "Protocol:",       chipProtocol);

    gridC->setColumnStretch(0, 0);
    gridC->setColumnStretch(1, 1);
    groupChipInfo->setLayout(gridC);
    leftLayout->addWidget(groupChipInfo);

    // start clean
    clearChipInfo();

    // Buffer group
    auto *groupBuffer = new QGroupBox("Buffer", leftBox);
    auto *gridB  = new QGridLayout(groupBuffer);
    btnClear     = new QPushButton("Clear buffer", groupBuffer);
    btnSave      = new QPushButton("Save buffer", groupBuffer);
    btnRead      = new QPushButton("Read from target",  groupBuffer);
    btnWrite     = new QPushButton("Write to target", groupBuffer);
    chkAsciiSwap = new QCheckBox("ASCII byteswap (16-bit)", groupBuffer);
    btnLoad      = new QPushButton("Load file to buffer", groupBuffer);
    progReadWrite = new QProgressBar(groupBuffer);

    progReadWrite->setRange(0, 100);
    progReadWrite->setValue(0);
    progReadWrite->setTextVisible(true);


    if (!lblBufSize)  lblBufSize  = new QLabel("Size: 0 (0x0)", groupBuffer);

    gridB->addWidget(btnLoad,      0, 0, 1, 2);
    gridB->addWidget(btnClear,     1, 0, 1, 1);
    gridB->addWidget(btnSave,      1, 1, 1, 1);
    gridB->addWidget(btnRead,      2, 0, 1, 1);
    gridB->addWidget(btnWrite,     2, 1, 1, 1);
    gridB->addWidget(chkAsciiSwap, 3, 0, 1, 2);
    gridB->addWidget(lblBufSize,   4, 0, 1, 2);
    gridB->addWidget(progReadWrite,5, 0, 1, 2);

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
    legendTable = new QTableWidget(rightSplitter);
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

    // button wiring
    connect(btnClear, &QPushButton::clicked, this, [this]{
        buffer_.clear();
        bufferSegments.clear();
        updateLegendTable();
        if (hexModel) hexModel->setBufferRef(&buffer_);
        if (lblBufSize)  lblBufSize->setText("Size: 0 (0x0)");
        updateActionEnabling();
    });
    connect(btnSave,  &QPushButton::clicked, this, &MainWindow::saveBufferToFile);
    connect(btnLoad, &QPushButton::clicked, this, [this]{
        loadAtOffsetDialog();
    });
    connect(comboDevice, &QComboBox::currentTextChanged,
        this, [this](const QString&){ updateActionEnabling(); });

    // Process wiring
    proc = new ProcessHandling(this);
    connect(proc, &ProcessHandling::logLine,
            log, &QPlainTextEdit::appendPlainText);
    connect(proc, &ProcessHandling::errorLine,
            log, &QPlainTextEdit::appendPlainText);
    connect(proc, &ProcessHandling::devicesScanned, this, [this](const QStringList &names){
        if (!log) return;
        if (!names.isEmpty()) {
            for (const auto &n : names) log->appendPlainText("[Scan] " + n);
        }
    });

    // ASCII byteswap toggle
    connect(chkAsciiSwap, &QCheckBox::toggled, this, [this](bool on){
        if (hexModel) hexModel->setSwapAscii16(on);
    });

    // Trigger a device rescan
    connect(btnRescan, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        comboProgrammer->clear();
        comboProgrammer->setPlaceholderText("No programmer");
        comboProgrammer->setEnabled(false);
        comboDevice->clear();
        comboDevice->setPlaceholderText("No devices");
        comboDevice->setEnabled(false);
        proc->scanConnectedDevices();
    });

    // Kick off one initial scan on startup (after the window is up)
    QTimer::singleShot(100, this, [this]{
        if (proc) proc->scanConnectedDevices();
    });

    // When a scan completes, we already populate programmers:
    connect(proc, &ProcessHandling::devicesScanned,
            this, &MainWindow::onDevicesScanned);

    // Populate device list when fetched
    connect(proc, &ProcessHandling::devicesListed,
            this, &MainWindow::onDevicesListed);

    // When the selected programmer changes, fetch its device list
    connect(comboProgrammer, &QComboBox::currentTextChanged,
            this, [this](const QString &p){
                if (p.trimmed().isEmpty()) return;
                comboDevice->clear();
                comboDevice->setPlaceholderText("");
                comboDevice->setEnabled(false);
                proc->fetchSupportedDevices(p.trimmed());
            });

    // Fetch chip info
    connect(proc, &ProcessHandling::chipInfoReady,
        this, &MainWindow::updateChipInfo);

    // Read from target
    connect(btnRead, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->currentText().trimmed();
        if (p.isEmpty() || d.isEmpty()) return;
        proc->readChipImage(p, d, optionFlags());
    });

    // Read from target is ready
    connect(proc, &ProcessHandling::readReady, this, [this](const QString& tempPath){
        // Use the same dialog, but with a preselected path
        loadAtOffsetDialog(tempPath);
    });

    // Update the bar as progress arrives
    connect(proc, &ProcessHandling::progress, this, [this](int p) {
        if (progReadWrite) {
            if (!progReadWrite->isVisible()) progReadWrite->show();
            progReadWrite->setValue(p);
        }
    });

    // initial state
    setUiEnabled(true);
    updateActionEnabling();
}

bool MainWindow::eventFilter(QObject *obj, QEvent *e) {
  if (obj == comboDevice->lineEdit()) {
    auto *edit = comboDevice->lineEdit();

    // First click inside the edit clears it
    if (e->type() == QEvent::MouseButtonPress) {
      if (edit->property("clearOnFirstClick").toBool()) {
        edit->clear();
        edit->setProperty("clearOnFirstClick", false);
      }
    }
  }
  return QMainWindow::eventFilter(obj, e);
}


void MainWindow::setUiEnabled(bool on) {
    // Enabled at boot
    for (auto *w : std::vector<QWidget*>{btnRescan, btnClear, btnSave, btnRead, btnWrite})
        if (w) w->setEnabled(true);
    // Disabled at boot
    for (auto *w : std::vector<QWidget*>{comboProgrammer, comboDevice})
        if (w) w->setEnabled(false);
}

void MainWindow::updateActionEnabling() {
    const bool deviceSelected = (comboDevice->currentIndex() >= 0);
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
    if (chkIgnoreId->isChecked())      f << "-y";
    if (chkSkipId->isChecked())        f << "--skip_id";
    if (chkNoSizeErr->isChecked())     f << "--no_size_error";
    if (chkPinCheck->isChecked())      f << "--pin_check";
    if (chkHardwareCheck->isChecked()) f << "--hardware_check";
    return f;
}

static inline QString prettyBytes(qulonglong b) {
    if (!b) return "-";
    return QString("%1 (0x%2)")
            .arg(QLocale().toString(b))
            .arg(QString::number(b, 16).toUpper());
}

void MainWindow::updateChipInfo(const ProcessHandling::ChipInfo &ci)
{
    // graceful fallbacks for partial info
    chipName     ->setText(ci.baseName.isEmpty()   ? "-" : ci.baseName);
    chipPackage  ->setText(ci.package.isEmpty()    ? "-" : ci.package);
    chipMemory   ->setText(prettyBytes(ci.bytes));
    chipBusWidth ->setText(ci.wordBits > 0         ? QString("%1-bit").arg(ci.wordBits) : "-");
    chipProtocol ->setText(ci.protocol.isEmpty()   ? "-" : ci.protocol);
}

void MainWindow::clearChipInfo()
{
    ProcessHandling::ChipInfo blank; // all empty/zero
    updateChipInfo(blank);
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

void MainWindow::onDevicesScanned(const QStringList &names)
{
    // Refresh the programmer dropdown
    comboProgrammer->clear();

    if (names.isEmpty()) {
        comboProgrammer->setPlaceholderText("No programmer found");
        if (log) log->appendPlainText("[Error] No programmer found.");
        // (Optional) disable actions that need a programmer here
        return;
    }

    comboProgrammer->addItems(names);
    comboProgrammer->setCurrentIndex(0);
    comboProgrammer->setEnabled(true);
    if (log) log->appendPlainText(QString("[Info] Found %1 programmer(s).").arg(names.size()));
}

void MainWindow::onDevicesListed(const QStringList &names)
{
    comboDevice->clear();

    if (names.isEmpty()) {
        comboDevice->setPlaceholderText("No devices for this programmer");
        comboDevice->setEnabled(false);
        if (log) log->appendPlainText("[Error] No devices found for selected programmer.");
        return;
    }

    QStringList sorted = names;
    sorted.sort(Qt::CaseInsensitive);
    comboDevice->addItems(sorted);
    comboDevice->setCurrentIndex(-1);
    comboDevice->setEnabled(true);

    if (log) log->appendPlainText(QString("[Info] Loaded %1 devices.").arg(sorted.size()));
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

// Load file to buffer at user-specified offset, with optional padding
void MainWindow::loadAtOffsetDialog(QString path) {
    // If preset path is given, skip the file picker dialog
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, tr("Pick image"), lastPath_,
            tr("All files (*);;Binary (*.bin)"));
        if (path.isEmpty()) return;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        log->appendPlainText(QString("[Error] open: %1").arg(f.errorString()));
        return;
    }
    const QByteArray file = f.readAll();
    const qulonglong fileSize = static_cast<qulonglong>(file.size());

    // 2) Ask for offset/length/pad now that we know the file size
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Load file to buffer"));
    dlg.setMinimumSize(560, 260);          // give headroom for large numbers
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
    addSegmentAndRefresh(off, effLen, QFileInfo(path).fileName());
    lastPath_ = QFileInfo(path).absolutePath();

    log->appendPlainText(QString("[Loaded] %1 bytes at 0x%2 from %3")
                         .arg(QLocale().toString(effLen))
                         .arg(QString::number(off, 16).toUpper())
                         .arg(path));

    if (lblBufSize)
        lblBufSize->setText(QString("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));

    updateActionEnabling();
}

void MainWindow::updateLegendTable() {
    if (!legendTable) return;
    legendTable->setRowCount(bufferSegments.size());
    int row = 0;
    for (const auto &s : std::as_const(bufferSegments)) {
        const qulonglong end = s.length ? (s.start + s.length - 1) : s.start;
        auto *itStart = new QTableWidgetItem(QString("0x") + QString::number(s.start, 16));
        auto *itEnd   = new QTableWidgetItem(QString("0x") + QString::number(end,   16));
        auto *itSize  = new QTableWidgetItem(QString("%1 (0x%2)")
                                             .arg(QString::number(s.length))
                                             .arg(QString::number(s.length, 16)));
        QString label = s.label;
        if (!s.note.isEmpty()) label += s.note;
        auto *itLabel = new QTableWidgetItem(label);

        legendTable->setItem(row, 0, itStart);
        legendTable->setItem(row, 1, itEnd);
        legendTable->setItem(row, 2, itSize);
        legendTable->setItem(row, 3, itLabel);
        ++row;
    }
    legendTable->resizeRowsToContents();
}

void MainWindow::addSegmentAndRefresh(qulonglong start, qulonglong length, const QString &label) {
    const qulonglong nBeg = start;
    const qulonglong nEnd = start + length; // half-open

    QList<BufferSegment> out;
    out.reserve(bufferSegments.size() + 1);

    bool anyPartialOverlap = false;

    for (const auto &seg : std::as_const(bufferSegments)) {
        const qulonglong sBeg = seg.start;
        const qulonglong sEnd = seg.start + seg.length; // half-open

        // no overlap
        if (sEnd <= nBeg || nEnd <= sBeg) {
            out.append(seg);
            continue;
        }

        const bool hasLeftRemainder  = (sBeg < nBeg);
        const bool hasRightRemainder = (nEnd < sEnd);
        if (hasLeftRemainder || hasRightRemainder) anyPartialOverlap = true;

        if (hasLeftRemainder)
            out.append(BufferSegment{ sBeg, nBeg - sBeg, seg.label, QStringLiteral(" (partial)") });

        // middle overlapped portion is dropped

        if (hasRightRemainder)
            out.append(BufferSegment{ nEnd, sEnd - nEnd, seg.label, QStringLiteral(" (partial)") });
    }

    out.append(BufferSegment{ nBeg, length, label,
                              anyPartialOverlap ? QStringLiteral(" (overlap)") : QString() });

    std::sort(out.begin(), out.end(),
              [](const BufferSegment &a, const BufferSegment &b){ return a.start < b.start; });

    QList<BufferSegment> coalesced;
    for (const auto &s : std::as_const(out)) {
        if (!coalesced.isEmpty()) {
            auto &back = coalesced.last();
            const bool sameTag    = (back.label == s.label) && (back.note == s.note);
            const bool contiguous = (back.start + back.length == s.start);
            if (sameTag && contiguous) { back.length += s.length; continue; }
        }
        coalesced.append(s);
    }

    bufferSegments = std::move(coalesced);
    updateLegendTable();
}
