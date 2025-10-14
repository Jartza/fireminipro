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
#include "SegmentView.h"
#include <QVector>
#include <QTimer>
#include <QSortFilterProxyModel>
#include <QCompleter>
#include <QProgressBar>
#include <QStyleFactory>
#include <QFileInfo>
#include <QApplication>
#include <QMenuBar>
#include <QMessageBox>
#include <QFontDatabase>
#include <algorithm>
#include <utility>

#include "ProcessHandling.h"
#include "MainWindow.h"
#include "HexView.h"
#include "LoadPreviewBar.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // The constructor builds the entire UI programmatically.
    // This avoids the need for .ui files and Qt Designer.

    // Main window central widget
    auto *central = new QWidget(this);
    setCentralWidget(central);
    setWindowTitle("FireMinipro - An Open Source GUI for minipro CLI tool " FIREMINIPRO_VERSION);
    this->setMinimumSize(1030,768);

    // Menu bar
    auto *mb = menuBar();

    // App menu (macOS will merge this into the app menu automatically)
    auto *menuApp = mb->addMenu(tr("&FireMinipro"));

    auto *actAbout = new QAction(tr("&About FireMinipro…"), this);
    actAbout->setMenuRole(QAction::AboutRole); // macOS-native placement
    connect(actAbout, &QAction::triggered, this, [this]{
        const QString ver = QStringLiteral(FIREMINIPRO_VERSION);
        const QString qt  = QString::fromLatin1(qVersion());

        QString html =
            "<b>FireMinipro</b><br>"
            "Version: " + ver + "<br>"
            "Qt: " + qt + "<br><br>"
            "A fast, buffer-first GUI for minipro.<br>"
            "<small>© 2025 Jari Tulilahti / Firebay refurb.<br>"
            "MIT License.</small>";

        QMessageBox box(this);
        box.setWindowTitle(tr("About FireMinipro"));
        box.setTextFormat(Qt::RichText);
        box.setText(html);
        box.setIconPixmap(QPixmap(":/appicon.png").scaled(96,96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        box.exec();
    });
    menuApp->addAction(actAbout);
    menuApp->addSeparator();

    auto *actQuit = new QAction(tr("&Quit"), this);
    actQuit->setShortcuts(QKeySequence::Quit);
    actQuit->setMenuRole(QAction::QuitRole);
    connect(actQuit, &QAction::triggered, qApp, &QCoreApplication::quit);
    menuApp->addAction(actQuit);

    // Left column
    auto *leftBox = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(leftBox);

    // Target group
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
    // and editable to allow custom device names,
    // also enable clear button
    comboDevice->setEditable(true);
    comboDevice->setInsertPolicy(QComboBox::NoInsert);
    comboDevice->setFocusPolicy(Qt::StrongFocus);
    comboDevice->lineEdit()->setClearButtonEnabled(true);

    // Use a filter proxy model to filter the device list
    auto *filter_model = new QSortFilterProxyModel(comboDevice);
    filter_model->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filter_model->setSourceModel(comboDevice->model());

    // Set the filtered model as the view for the combobox
    // this allows typing to filter the dropdown list
    auto *completer = new QCompleter(filter_model, comboDevice);
    completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    comboDevice->setCompleter(completer);

    // When selection changes, update action enabling
    // Also, fetch chip info when user selects a device
    connect(comboDevice, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int idx){
            updateActionEnabling();
            if (idx < 0) {
                // Optional: if selection cleared, clear the chip info too
                clearChipInfo();
            }
        });

    // fetch only on user action. Avoid fetching on every text change
    // to avoid spamming processes when user is in the middle of typing to filter
    connect(completer, qOverload<const QString&>(&QCompleter::activated), this, [this](const QString &text){
        // Find exact item index (case-insensitive)
        int idx = -1;
        for (int i = 0; i < comboDevice->count(); ++i) {
            if (comboDevice->itemText(i).compare(text, Qt::CaseInsensitive) == 0) { idx = i; break; }
        }
        if (idx < 0 || !proc) return;

        comboDevice->setCurrentIndex(idx);   // reflect selection in the UI
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->itemText(idx).trimmed();
        if (!p.isEmpty() && !d.isEmpty()) {
            clearChipInfo();
            proc->fetchChipInfo(p, d);
        }
    });

    // When user types in the combobox, filter the list
    connect(comboDevice->lineEdit(),
      &QLineEdit::textEdited,
      this,
      [this, filter_model](const QString &text) {
        if (text.size() > 1 && filter_model) {
            filter_model->setFilterFixedString(text);
        }
      }
    );

    // When user clicks the combobox, clear the text on first click
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

    // Event filter to clear text on first click
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
    progReadWrite->setFormat(QStringLiteral("Idle"));
    progReadWrite->setStyle(QStyleFactory::create("Fusion"));

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

    // Device operations group (sits between Buffer and Options)
    auto *groupDevOps = new QGroupBox("Device operations", leftBox);
    auto *gridDO      = new QGridLayout(groupDevOps);

    btnBlankCheck  = new QPushButton("Blank check",  groupDevOps);
    btnEraseDevice = new QPushButton("Erase device", groupDevOps);
    btnTestLogic   = new QPushButton("Test logic",   groupDevOps);

    // Layout: two columns, then one full-width
    gridDO->addWidget(btnBlankCheck,  0, 0);
    gridDO->addWidget(btnEraseDevice, 0, 1);
    gridDO->addWidget(btnTestLogic,   1, 0, 1, 2);

    groupDevOps->setLayout(gridDO);
    leftLayout->addWidget(groupDevOps);

    // Device Options
    auto *groupOpts  = new QGroupBox("Device Options", leftBox);
    auto *gridO      = new QGridLayout(groupOpts);
    chkSkipVerify    = new QCheckBox("Skip verify", groupOpts);
    chkIgnoreId      = new QCheckBox("Ignore ID error", groupOpts);
    chkSkipId        = new QCheckBox("Skip ID check", groupOpts);
    chkNoSizeErr     = new QCheckBox("Ignore size error", groupOpts);

    // layout options (2 columns)
    gridO->addWidget(chkSkipVerify,    0,0);
    gridO->addWidget(chkIgnoreId,      0,1);
    gridO->addWidget(chkSkipId,        1,0);
    gridO->addWidget(chkNoSizeErr,     1,1);

    groupOpts->setLayout(gridO);
    leftLayout->addWidget(groupOpts);
    leftLayout->addStretch();

    // right side (hex + legend + log)
    auto *rightSplitter = new QSplitter(Qt::Vertical, central);
    tableHex = new QTableView(rightSplitter);

    // Buffer legend table
    legendTable = new QTableView(rightSplitter);
    legendTable->setObjectName("bufferLegendTable");
    segmentModel = new SegmentView(legendTable);
    legendTable->setModel(segmentModel);
    legendTable->horizontalHeader()->setStretchLastSection(true);
    legendTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    legendTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    legendTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    legendTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    legendTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    legendTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    legendTable->setSelectionMode(QAbstractItemView::SingleSelection);
    legendTable->setDragDropMode(QAbstractItemView::InternalMove);
    legendTable->setDragDropOverwriteMode(false);
    legendTable->setDefaultDropAction(Qt::MoveAction);
    legendTable->setAcceptDrops(true);
    legendTable->viewport()->setAcceptDrops(true);
    legendTable->setFocusPolicy(Qt::StrongFocus);
    legendTable->verticalHeader()->setVisible(false);
    legendTable->setAlternatingRowColors(true);
    legendTable->setMinimumHeight(152);
    connect(segmentModel, &SegmentView::rowReordered,
            this, &MainWindow::onSegmentRowReordered);

    log = new QPlainTextEdit(rightSplitter);
    log->setReadOnly(true);
    logFontDefault_ = log->font();
    logFontFixed_ = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    logFontFixed_.setPointSizeF(this->font().pointSizeF() - 1);

    applyLogFontForDevice();

    rightSplitter->addWidget(tableHex);
    rightSplitter->addWidget(legendTable);
    rightSplitter->addWidget(log);
    rightSplitter->setStretchFactor(0, 5);
    rightSplitter->setStretchFactor(1, 1);
    rightSplitter->setStretchFactor(2, 3);

    // Hex view/model
    hexModel = new HexView(this);
    hexModel->setBufferRef(&buffer_);
    tableHex->setModel(hexModel);
    QFont mono;
    mono.setFamily("Courier New");
    mono.setStyleHint(QFont::TypeWriter);
    mono.setPointSizeF(this->font().pointSizeF() - 1);
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
    for (int c = 1; c <= hexModel->getBytesPerRow(); ++c) tableHex->setColumnWidth(c, 28);
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
    // Display error lines in red
    connect(proc, &ProcessHandling::errorLine, this, [this](const QString &line){
        if (!log) return;
        log->appendHtml("<font color='red'>" + line + "</font>");
    });
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
    connect(comboProgrammer, &QComboBox::currentIndexChanged,
            this, [this](int index){
                if (index == -1) return;
                const QString p = comboProgrammer->itemText(index).trimmed();
                comboDevice->clear();
                comboDevice->setPlaceholderText("");
                comboDevice->setEnabled(false);
                proc->fetchSupportedDevices(p);
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

    // Write to target
    connect(btnWrite, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->currentText().trimmed();
        if (p.isEmpty() || d.isEmpty()) return;
        if (buffer_.isEmpty()) {
            if (log) log->appendPlainText("[Error] buffer is empty");
            return;
        }
        // Export buffer to a temp file
        QString tempPath = exportBufferToTempFileLocal("fmp-write");
        if (tempPath.isEmpty()) {
            if (log) log->appendPlainText("[Error] failed to create temp file for writing");
            return;
        }
        // Write to target
        proc->writeChipImage(p, d, tempPath, optionFlags());
    });

    // Read from target is ready
    connect(proc, &ProcessHandling::readReady, this, [this](const QString& tempPath){
        // Use the same dialog, but with a preselected path
        loadAtOffsetDialog(tempPath);
    });

    // Update the bar as progress arrives
    connect(proc, &ProcessHandling::progress, this, 
        [this](int pct, const QString &label) {
        if (progReadWrite) {
            if (!progReadWrite->isVisible()) progReadWrite->show();
            if (pct > 0) progReadWrite->setValue(pct);
            if (!label.isEmpty()) progReadWrite->setFormat(label + " %p%");
        }
    });

    // Disable UI buttons when minipro is running
    connect(proc, &ProcessHandling::started, this, [this]{
        disableBusyButtons();
        QApplication::setOverrideCursor(Qt::BusyCursor);
    });

    // When process finishes, ensure progress bar is at 100% and shows "Idle",
    // and that the UI is re-enabled after process completion
    connect(proc, &ProcessHandling::finished, this,
        [this](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
            // Always restore UI first
            updateActionEnabling();
            QApplication::restoreOverrideCursor();
            if (progReadWrite) {
                progReadWrite->setValue(100);
                progReadWrite->setFormat(QStringLiteral("Idle"));
                progReadWrite->setTextVisible(true);
            }
        });

    // Blank check button
    connect(btnBlankCheck, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->currentText().trimmed();
        if (p.isEmpty() || d.isEmpty()) return;
       proc->checkIfBlank(p, d, optionFlags());
    });

    // Erase device button
    connect(btnEraseDevice, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->currentText().trimmed();
        if (p.isEmpty() || d.isEmpty()) return;
        proc->eraseChip(p, d, optionFlags());
    });

    // Logic test button
    connect(btnTestLogic, &QPushButton::clicked, this, [this]{
        if (!proc) return;
        const QString p = comboProgrammer->currentText().trimmed();
        const QString d = comboDevice->currentText().trimmed();
        if (p.isEmpty() || d.isEmpty()) return;
        proc->testLogicChip(p, d, optionFlags());
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

// Disable buttons that should not be used while a process is running
void MainWindow::disableBusyButtons()
{
    for (QWidget *w : std::vector<QWidget*>{
        btnRead, btnWrite, btnEraseDevice, btnBlankCheck, btnTestLogic,
        btnRescan, comboProgrammer, comboDevice, btnLoad, btnSave, btnClear
    }) {
        if (w) w->setEnabled(false);
    }
}

void MainWindow::updateActionEnabling() {
    // Always enable programmer/device selection and rescan
    if (comboProgrammer) comboProgrammer->setEnabled(true);
    if (comboDevice)     comboDevice->setEnabled(true);
    if (btnRescan)      btnRescan->setEnabled(true);

    const bool deviceSelected = (comboDevice->currentIndex() >= 0);
    const bool hasBuffer      = !buffer_.isEmpty();

    // Common to all devices
    if (btnClear)     btnClear->setEnabled(true);
    if (btnLoad)      btnLoad->setEnabled(true);
    if (chkAsciiSwap) chkAsciiSwap->setEnabled(hasBuffer);
    if (btnSave)      btnSave->setEnabled(hasBuffer);

    // Separate logic ICs from memory devices
    if (currentIsLogic_) {
        // Logic IC: only “Test logic” makes sense
        if (btnEraseDevice) btnEraseDevice->setEnabled(false);
        if (btnBlankCheck)  btnBlankCheck->setEnabled(false);
        if (btnRead)        btnRead->setEnabled(false);
        if (btnWrite)       btnWrite->setEnabled(false);
        if (btnTestLogic)   btnTestLogic->setEnabled(deviceSelected);
    } else {
        // Memory device: buffer ops + blank/erase; no logic test
        if (btnEraseDevice) btnEraseDevice->setEnabled(deviceSelected);
        if (btnBlankCheck)  btnBlankCheck->setEnabled(deviceSelected);
        if (btnTestLogic)   btnTestLogic->setEnabled(false);
        if (btnRead)        btnRead->setEnabled(deviceSelected);
        if (btnWrite)       btnWrite->setEnabled(deviceSelected && hasBuffer);
    }
}

void MainWindow::applyLogFontForDevice() {
    if (!log) return;
    if (currentIsLogic_) {
        QFont mono = logFontFixed_;
        if (mono.family().isEmpty()) {
            mono = logFontDefault_;
        } else {
            mono.setPointSizeF(logFontDefault_.pointSizeF());
        }
        log->setFont(mono);
    } else {
        log->setFont(logFontDefault_);
    }
}

QStringList MainWindow::optionFlags() const {
    QStringList f;
    if (chkSkipVerify->isChecked())    f << "-v";
    if (chkIgnoreId->isChecked())      f << "-y";
    if (chkSkipId->isChecked())        f << "-x";
    if (chkNoSizeErr->isChecked())     f << "-s";
    return f;
}

static inline QString prettyBytes(qulonglong b) {
    if (!b) return "-";
    return QString("%1 (0x%2)")
            .arg(QLocale().toString(b))
            .arg(QString::number(b, 16).toUpper());
}

QString MainWindow::pickFile(const QString &title, QFileDialog::AcceptMode mode,
                             const QString &filters)
{
    QFileDialog dialog(this, title, lastPath_);
    dialog.setAcceptMode(mode);
    dialog.setFileMode(mode == QFileDialog::AcceptSave ? QFileDialog::AnyFile
                                                       : QFileDialog::ExistingFile);
    if (!filters.isEmpty()) {
        dialog.setNameFilters(filters.split(QStringLiteral(";;")));
    }

    dialog.setWindowFlag(Qt::Sheet);
    dialog.setWindowModality(Qt::WindowModal);

    QString selected;
    if (dialog.exec() == QDialog::Accepted) {
        const QStringList files = dialog.selectedFiles();
        if (!files.isEmpty()) {
            selected = files.first();
            lastPath_ = QFileInfo(selected).absolutePath();
        }
    }
    return selected;
}

void MainWindow::updateChipInfo(const ProcessHandling::ChipInfo &ci)
{
    // graceful fallbacks for partial info
    chipName     ->setText(ci.baseName.isEmpty()   ? "-" : ci.baseName);
    chipPackage  ->setText(ci.package.isEmpty()    ? "-" : ci.package);
    if (ci.isLogic) {
        currentIsLogic_ = true;
        chipMemory  ->setText("-");  // logic ICs don’t have a byte size
        chipBusWidth->setText(QString("Logic IC%1")
                              .arg(ci.vectorCount > 0 ? QString(" (%1 vectors)").arg(ci.vectorCount) : QString()));
    } else {
        currentIsLogic_ = false;
        chipMemory  ->setText(prettyBytes(ci.bytes));
        chipBusWidth->setText(ci.wordBits > 0 ? QString("%1-bit").arg(ci.wordBits) : "-");
    }
    chipProtocol ->setText(ci.protocol.isEmpty()   ? "-" : ci.protocol);

    applyLogFontForDevice();
    updateActionEnabling();
}

void MainWindow::clearChipInfo()
{
    currentIsLogic_ = false;
    ProcessHandling::ChipInfo blank;
    updateChipInfo(blank);
}

void MainWindow::saveBufferToFile() {
    if (buffer_.isEmpty()) { log->appendPlainText("[Info] Buffer is empty"); return; }
#if defined(Q_OS_MACOS)
    const QString path = pickFile(tr("Save image"),
                                  QFileDialog::AcceptSave,
                                  tr("Binary (*.bin);;All files (*)"));
#else
    const QString path = QFileDialog::getSaveFileName(this,
        tr("Save image"), lastPath_,
        tr("Binary (*.bin);;All files (*)"));
#endif
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        log->appendPlainText(QString("[Error] save: %1").arg(f.errorString()));
        return;
    }
    // Error check the write
    if (f.write(buffer_) != buffer_.size()) {
        log->appendPlainText("[Error] save: write failed");
        f.close();
        return;
    }
    f.close();
    log->appendPlainText(QString("[Saved] %1 bytes to %2").arg(buffer_.size()).arg(path));
    lastPath_ = QFileInfo(path).absolutePath();
}

// Create temp file from buffer, for writing to target
QString MainWindow::exportBufferToTempFileLocal(const QString& baseName)
{
    if (buffer_.isEmpty()) {
        if (log) log->appendPlainText("[Write] Buffer is empty.");
        return {};
    }

    const QString safeBase = baseName.isEmpty() ? QStringLiteral("image") : baseName;
    const QString ts = QDateTime::currentDateTime().toString("yyMMdd-HHmmss-zzz");
    const QString dir = QDir::tempPath();
    QString file = QString("%1-%2.bin").arg(safeBase, ts);
    QString path = QDir(dir).filePath(file);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (log) log->appendPlainText(QString("[Write] open temp failed: %1").arg(f.errorString()));
        return {};
    }
    if (f.write(buffer_) != buffer_.size()) {
        if (log) log->appendPlainText("[Write] write temp failed.");
        return {};
    }
    f.close();

    if (log) log->appendPlainText(QString("[Write] Exported buffer to %1").arg(path));
    return path;
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

// Load file to buffer at user-specified offset, with optional padding
void MainWindow::loadAtOffsetDialog(QString path) {
    // Sanitary cursor
    QApplication::restoreOverrideCursor();

    // If preset path is given, skip the file picker dialog
    if (path.isEmpty()) {
#if defined(Q_OS_MACOS)
        path = pickFile(tr("Pick image"),
                        QFileDialog::AcceptOpen,
                        tr("All files (*);;Binary (*.bin)"));
#else
        path = QFileDialog::getOpenFileName(this,
                tr("Pick image"), lastPath_,
                tr("All files (*);;Binary (*.bin)"));
#endif
        if (path.isEmpty()) return;
    }
#if !defined(Q_OS_MACOS)
    lastPath_ = QFileInfo(path).absolutePath();
#endif

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
    dlg.raise();
    dlg.activateWindow();
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

    const qulonglong bufferSizeBefore = static_cast<qulonglong>(buffer_.size());

    // Build a data block of effLen: file bytes then padding (if requested > file)
    QByteArray data;
    data.reserve(static_cast<int>(effLen));
    const qulonglong take = std::min<qulonglong>(effLen, fileSize);
    data.append(file.left(static_cast<int>(take)));
    if (effLen > take) data.append(QByteArray(static_cast<int>(effLen - take), char(pad & 0xFF)));

    const qulonglong prePadLen  = (off > bufferSizeBefore) ? (off - bufferSizeBefore) : 0;
    const qulonglong postPadLen = (effLen > take) ? (effLen - take) : 0;

    // Patch the buffer with PAD used also for growth between current size and offset
    patchBuffer(static_cast<int>(off), data, char(pad & 0xFF));
    QString displayName = QFileInfo(path).fileName();
    if (prePadLen > 0 || postPadLen > 0) {
        QStringList parts;
        if (prePadLen > 0)  parts << tr("pre:%1").arg(QLocale().toString(prePadLen));
        if (postPadLen > 0) parts << tr("post:%1").arg(QLocale().toString(postPadLen));
        const QString padByteText = QString::number(pad & 0xFF, 16).toUpper().rightJustified(2, QLatin1Char('0'));
        displayName += QString(" (padded 0x%1 %2)").arg(padByteText, parts.join(QLatin1Char(' ')));
    }
    addSegmentAndRefresh(off, effLen, displayName);
    lastPath_ = QFileInfo(path).absolutePath();

    log->appendPlainText(QString("[Loaded] %1 bytes at 0x%2 from %3")
                         .arg(QLocale().toString(effLen))
                         .arg(QString::number(off, 16).toUpper())
                         .arg(QFileInfo(path).fileName()));

    if (lblBufSize)
        lblBufSize->setText(QString("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));

    updateActionEnabling();
}

void MainWindow::updateLegendTable() {
    if (!segmentModel) return;

    QVector<SegmentView::Segment> rows;
    rows.reserve(bufferSegments.size());
    for (const auto &s : std::as_const(bufferSegments)) {
        SegmentView::Segment seg;
        seg.start  = s.start;
        seg.length = s.length;
        seg.label  = s.label;
        seg.note   = s.note;
        seg.id     = s.id;
        rows.append(seg);
    }

    segmentModel->setSegments(std::move(rows));
    if (legendTable) legendTable->resizeRowsToContents();
}

void MainWindow::onSegmentRowReordered(int from, int to) {
    if (from == to) return;
    if (bufferSegments.isEmpty()) return;
    if (from < 0 || from >= bufferSegments.size()) return;

    BufferSegment moving = bufferSegments.at(from);
    const qulonglong maxLen = (moving.start < qulonglong(buffer_.size()))
                            ? std::min<qulonglong>(moving.length,
                                                   qulonglong(buffer_.size()) - moving.start)
                            : 0;
    if (maxLen == 0) return;

    const int segStart = static_cast<int>(moving.start);
    const int segLen   = static_cast<int>(maxLen);
    QByteArray segmentData = buffer_.mid(segStart, segLen);
    if (segmentData.size() != segLen) return;

    buffer_.remove(segStart, segLen);
    bufferSegments.removeAt(from);
    const qulonglong removedLen = maxLen;
    for (int i = from; i < bufferSegments.size(); ++i) {
        bufferSegments[i].start -= removedLen;
    }

    int insertIndex = std::clamp(to, 0, static_cast<int>(bufferSegments.size()));

    qulonglong insertStart = (insertIndex < bufferSegments.size())
                           ? bufferSegments[insertIndex].start
                           : qulonglong(buffer_.size());

    for (int i = insertIndex; i < bufferSegments.size(); ++i) {
        bufferSegments[i].start += removedLen;
    }

    moving.start  = insertStart;
    moving.length = removedLen;
    bufferSegments.insert(insertIndex, moving);
    buffer_.insert(static_cast<int>(insertStart), segmentData);

    updateLegendTable();
    if (legendTable) legendTable->selectRow(insertIndex);
    if (legendTable) legendTable->scrollTo(legendTable->model()->index(insertIndex, 0));
    if (hexModel) hexModel->setBufferRef(&buffer_);
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
            out.append(BufferSegment{ sBeg, nBeg - sBeg, seg.label, QStringLiteral(" (partial)") , seg.id });

        // middle overlapped portion is dropped

        if (hasRightRemainder)
            out.append(BufferSegment{ nEnd, sEnd - nEnd, seg.label, QStringLiteral(" (partial)") , seg.id });
    }

    const qulonglong thisInsertId = nextSegmentId_++;
    out.append(BufferSegment{ nBeg, length, label,
                              anyPartialOverlap ? QStringLiteral(" (overlap)") : QString(),
                              thisInsertId });

    std::sort(out.begin(), out.end(),
              [](const BufferSegment &a, const BufferSegment &b){ return a.start < b.start; });

    QList<BufferSegment> coalesced;
    for (const auto &s : std::as_const(out)) {
        if (!coalesced.isEmpty()) {
            auto &back = coalesced.last();
            const bool sameTag    = (back.label == s.label)
                                   && (back.note == s.note)
                                   && (back.id == s.id);
            const bool contiguous = (back.start + back.length == s.start);
            if (sameTag && contiguous) { back.length += s.length; continue; }
        }
        coalesced.append(s);
    }

    bufferSegments = std::move(coalesced);
    updateLegendTable();
}
