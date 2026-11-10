#include <QDir>
#include <QFileInfo>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QAbstractItemView>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>
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
#include "SegmentTableView.h"
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
#include <QPair>
#include <algorithm>
#include <utility>
#include <QSignalBlocker>
#include <QTextCursor>
#include <QEvent>
#include <QScopeGuard>
#include <algorithm>
#include <utility>

#include "ProcessHandling.h"
#include "MainWindow.h"
#include "HexView.h"
#include "LoadPreviewBar.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // Load preferred UI language before building translated widgets.
    {
        QSettings settings;
        const QString envLang = qEnvironmentVariable("FIREMINIPRO_LANG").trimmed();
        const QString initialLang = envLang.isEmpty()
            ? settings.value(QStringLiteral("ui/language"), QStringLiteral("auto")).toString()
            : envLang;
        applyLanguage(initialLang, false);
    }

    // Main window central widget
    auto *central = new QWidget(this);
    setCentralWidget(central);
    this->setMinimumSize(1060, 740);
    this->resize(1160, 840);

    // Menu bar
    auto *mb = menuBar();

    // App menu (macOS will merge this into the app menu automatically)
    menuApp_ = mb->addMenu(tr("&FireMinipro"));

    actAbout_ = new QAction(tr("&About FireMinipro…"), this);
    actAbout_->setMenuRole(QAction::AboutRole); // macOS-native placement
    connect(actAbout_, &QAction::triggered, this, [this]{
        const QString ver = QStringLiteral(FIREMINIPRO_VERSION);
        const QString qt  = QString::fromLatin1(qVersion());

        QString html = QStringLiteral("<b>FireMinipro</b><br>%1 %2<br>%3 %4<br><br>%5<br><small>© 2025 Jari Tulilahti / Firebay refurb.<br>%6</small>")
            .arg(tr("Version:"), ver, tr("Qt:"), qt,
                 tr("A fast, buffer-first GUI for minipro."),
                 tr("MIT License."));

        QMessageBox box(this);
        box.setWindowTitle(tr("About FireMinipro"));
        box.setTextFormat(Qt::RichText);
        box.setText(html);
        box.setIconPixmap(QPixmap(":/appicon.png").scaled(96,96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        box.exec();
    });
    menuApp_->addAction(actAbout_);

    actPreferences_ = new QAction(tr("&Settings…"), this);
    actPreferences_->setShortcuts(QKeySequence::Preferences);
    actPreferences_->setMenuRole(QAction::PreferencesRole);
    connect(actPreferences_, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    menuApp_->addAction(actPreferences_);
    menuApp_->addSeparator();

    actQuit_ = new QAction(tr("&Quit"), this);
    actQuit_->setShortcuts(QKeySequence::Quit);
    actQuit_->setMenuRole(QAction::QuitRole);
    connect(actQuit_, &QAction::triggered, qApp, &QCoreApplication::quit);
    menuApp_->addAction(actQuit_);

    // Language menu
    menuLanguage_ = mb->addMenu(tr("&Language"));
    auto *langGroup = new QActionGroup(this);
    langGroup->setExclusive(true);

    actLangAuto_ = new QAction(tr("System Default"), this);
    actLangAuto_->setCheckable(true);
    actLangEn_ = new QAction(QStringLiteral("English"), this);
    actLangEn_->setCheckable(true);
    actLangJa_ = new QAction(QStringLiteral("日本語 (Japanese)"), this);
    actLangJa_->setCheckable(true);

    for (QAction *a : {actLangAuto_, actLangEn_, actLangJa_}) {
        langGroup->addAction(a);
        menuLanguage_->addAction(a);
    }
    actLangAuto_->setChecked(currentLangCode_ == QLatin1String("auto"));
    actLangEn_->setChecked(currentLangCode_ == QLatin1String("en"));
    actLangJa_->setChecked(currentLangCode_ == QLatin1String("ja"));

    connect(actLangAuto_, &QAction::triggered, this, [this]{ applyLanguage(QStringLiteral("auto"), true); retranslateUi(); });
    connect(actLangEn_,   &QAction::triggered, this, [this]{ applyLanguage(QStringLiteral("en"),   true); retranslateUi(); });
    connect(actLangJa_,   &QAction::triggered, this, [this]{ applyLanguage(QStringLiteral("ja"),   true); retranslateUi(); });

    // Theme menu
    menuTheme_ = mb->addMenu(tr("&Theme"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    actThemeFamicom_ = new QAction(tr("Famicom (Red & Cream)"), this);
    actThemeFamicom_->setCheckable(true);
    actThemeNes_ = new QAction(tr("NES (Gray & Red)"), this);
    actThemeNes_->setCheckable(true);
    actThemeLight_ = new QAction(tr("Light"), this);
    actThemeLight_->setCheckable(true);
    actThemeGray_ = new QAction(tr("Gray"), this);
    actThemeGray_->setCheckable(true);
    actThemeDark_ = new QAction(tr("Dark (Original)"), this);
    actThemeDark_->setCheckable(true);

    for (QAction *a : {actThemeFamicom_, actThemeNes_, actThemeLight_, actThemeGray_, actThemeDark_}) {
        themeGroup->addAction(a);
        menuTheme_->addAction(a);
    }
    menuTheme_->addSeparator();
    actOpenSettings_ = new QAction(tr("Open Settings Window…"), this);
    connect(actOpenSettings_, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    menuTheme_->addAction(actOpenSettings_);

    connect(actThemeFamicom_, &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("famicom"), true); });
    connect(actThemeNes_,     &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("nes"),     true); });
    connect(actThemeLight_,   &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("light"),   true); });
    connect(actThemeGray_,    &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("gray"),    true); });
    connect(actThemeDark_,    &QAction::triggered, this, [this]{ applyTheme(QStringLiteral("dark"),    true); });

    // Left column inside a scroll area so controls never collapse vertically
    auto *leftScroll = new QScrollArea(central);
    leftScroll->setWidgetResizable(true);
    leftScroll->setFrameShape(QFrame::NoFrame);
    leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    leftScroll->setMinimumWidth(340);

    auto *leftBox = new QWidget(leftScroll);
    auto *leftLayout = new QVBoxLayout(leftBox);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->setSpacing(8);
    leftScroll->setWidget(leftBox);

    QFont small = this->font();
    small.setPointSizeF(this->font().pointSizeF() - 1);

    // Target group
    groupTargets_ = new QGroupBox(leftBox);
    auto *gridT = new QGridLayout(groupTargets_);
    lblProg_ = new QLabel(groupTargets_);
    lblDev_  = new QLabel(groupTargets_);
    comboProgrammer = new QComboBox(groupTargets_);
    comboDevice = new QComboBox(groupTargets_);
    btnRescan = new QPushButton(groupTargets_);
    lblProg_->setFont(small);
    lblDev_->setFont(small);

    gridT->addWidget(lblProg_,        0, 0);
    gridT->addWidget(comboProgrammer, 0, 1);
    gridT->addWidget(lblDev_,         1, 0);
    gridT->addWidget(comboDevice,     1, 1, 1, 2);
    gridT->addWidget(btnRescan,       0, 2, 1, 1, Qt::AlignRight | Qt::AlignTop);
    
    comboProgrammer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    comboDevice->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    btnRescan->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    gridT->setColumnStretch(0, 0);
    gridT->setColumnStretch(1, 1);
    gridT->setColumnStretch(2, 0);
    gridT->setHorizontalSpacing(4);

    groupTargets_->setLayout(gridT);
    leftLayout->addWidget(groupTargets_);

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
                clearChipInfo();
                return;
            }
            if (!proc) return;
            const QString p = comboProgrammer->currentText().trimmed();
            const QString d = comboDevice->itemText(idx).trimmed();
            if (p.isEmpty() || d.isEmpty()) return;
            clearChipInfo();
            proc->fetchChipInfo(p, d);
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
    groupChipInfo_ = new QGroupBox(leftBox);
    auto *gridC = new QGridLayout(groupChipInfo_);

    // left column (keys), right column (values)
    auto addRow = [&](int r, QLabel *&keyOut, QLabel *&valueOut) {
        keyOut = new QLabel(groupChipInfo_);
        keyOut->setFont(small);
        keyOut->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueOut = new QLabel("-", groupChipInfo_);
        valueOut->setTextInteractionFlags(Qt::TextSelectableByMouse);
        gridC->addWidget(keyOut, r, 0);
        gridC->addWidget(valueOut, r, 1);
    };

    // rows
    int r = 0;
    addRow(r++, lblChipKeyName_,  chipName);
    addRow(r++, lblChipKeyPkg_,   chipPackage);
    addRow(r++, lblChipKeyMem_,   chipMemory);
    addRow(r++, lblChipKeyWidth_, chipBusWidth);
    addRow(r++, lblChipKeyProto_, chipProtocol);

    gridC->setColumnStretch(0, 0);
    gridC->setColumnStretch(1, 1);
    groupChipInfo_->setLayout(gridC);
    leftLayout->addWidget(groupChipInfo_);

    // start clean
    clearChipInfo();

    // Buffer group
    groupBuffer_ = new QGroupBox(leftBox);
    auto *gridB  = new QGridLayout(groupBuffer_);
    btnClear     = new QPushButton(groupBuffer_);
    btnSave      = new QPushButton(groupBuffer_);
    btnRead      = new QPushButton(groupBuffer_);
    btnWrite     = new QPushButton(groupBuffer_);
    chkAsciiSwap = new QCheckBox(groupBuffer_);
    btnLoadBinary   = new QPushButton(groupBuffer_);
    btnLoadAdvanced = new QPushButton(groupBuffer_);
    progReadWrite = new QProgressBar(groupBuffer_);

    btnWrite->setObjectName(QStringLiteral("btnWritePrimary"));
    for (QPushButton *b : {btnLoadBinary, btnLoadAdvanced, btnClear, btnSave, btnRead, btnWrite}) {
        b->setMinimumHeight(26);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    progReadWrite->setRange(0, 100);
    progReadWrite->setValue(0);
    progReadWrite->setTextVisible(true);
    progReadWrite->setMinimumHeight(22);
    progReadWrite->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    progReadWrite->setStyle(QStyleFactory::create("Fusion"));

    if (!lblBufSize)  lblBufSize  = new QLabel(groupBuffer_);

    gridB->addWidget(btnLoadBinary,   0, 0, 1, 1);
    gridB->addWidget(btnLoadAdvanced, 0, 1, 1, 1);
    gridB->addWidget(btnClear,        1, 0, 1, 1);
    gridB->addWidget(btnSave,         1, 1, 1, 1);
    gridB->addWidget(btnRead,         2, 0, 1, 1);
    gridB->addWidget(btnWrite,        2, 1, 1, 1);
    gridB->addWidget(chkAsciiSwap,    3, 0, 1, 2);
    gridB->addWidget(lblBufSize,      4, 0, 1, 2);
    gridB->addWidget(progReadWrite,   5, 0, 1, 2);

    groupBuffer_->setLayout(gridB);
    leftLayout->addWidget(groupBuffer_);

    // Device operations group (sits between Buffer and Options)
    groupDevOps_ = new QGroupBox(leftBox);
    auto *gridDO = new QGridLayout(groupDevOps_);

    btnBlankCheck  = new QPushButton(groupDevOps_);
    btnEraseDevice = new QPushButton(groupDevOps_);
    btnTestLogic   = new QPushButton(groupDevOps_);
    for (QPushButton *b : {btnBlankCheck, btnEraseDevice, btnTestLogic}) {
        b->setMinimumHeight(26);
        b->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    // Layout: two columns, then one full-width
    gridDO->addWidget(btnBlankCheck,  0, 0);
    gridDO->addWidget(btnEraseDevice, 0, 1);
    gridDO->addWidget(btnTestLogic,   1, 0, 1, 2);

    groupDevOps_->setLayout(gridDO);
    leftLayout->addWidget(groupDevOps_);

    // Device Options
    groupOpts_  = new QGroupBox(leftBox);
    auto *gridO = new QGridLayout(groupOpts_);
    chkSkipVerify = new QCheckBox(groupOpts_);
    chkIgnoreId   = new QCheckBox(groupOpts_);
    chkSkipId     = new QCheckBox(groupOpts_);
    chkNoSizeErr  = new QCheckBox(groupOpts_);

    // layout options (2 columns)
    gridO->addWidget(chkSkipVerify,    0,0);
    gridO->addWidget(chkIgnoreId,      0,1);
    gridO->addWidget(chkSkipId,        1,0);
    gridO->addWidget(chkNoSizeErr,     1,1);

    groupOpts_->setLayout(gridO);
    leftLayout->addWidget(groupOpts_);

    btnSettings_ = new QPushButton(leftBox);
    btnSettings_->setMinimumHeight(26);
    btnSettings_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(btnSettings_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);
    leftLayout->addWidget(btnSettings_);
    leftLayout->addStretch();

    // right side (hex + legend + log)
    auto *rightSplitter = new QSplitter(Qt::Vertical, central);
    tableHex = new QTableView(rightSplitter);

    // Buffer legend table
    legendTable = new SegmentTableView(rightSplitter);
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
    legendTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(segmentModel, &SegmentView::rowReordered,
            this, &MainWindow::onSegmentRowReordered);
    connect(legendTable, &QTableView::doubleClicked,
            this, &MainWindow::onLegendRowDoubleClicked);
    connect(legendTable, &SegmentTableView::externalFilesDropped,
            this, &MainWindow::onLegendFilesDropped);
    connect(legendTable, &SegmentTableView::customContextMenuRequested,
            this, &MainWindow::onLegendContextMenuRequested);

    log = new QPlainTextEdit(rightSplitter);
    log->installEventFilter(this);
    log->setAttribute(Qt::WA_StaticContents, true);
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
    split->addWidget(leftScroll);
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
        if (lblBufSize)  lblBufSize->setText(tr("Size: 0 (0x0)"));
        updateActionEnabling();
    });
    connect(btnSave,  &QPushButton::clicked, this, &MainWindow::saveBufferToFile);
    connect(btnLoadBinary, &QPushButton::clicked, this, &MainWindow::loadFileAppendDialog);
    connect(btnLoadAdvanced, &QPushButton::clicked, this, [this]{
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
            for (const auto &n : names) log->appendPlainText(tr("[Scan] %1").arg(n));
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
        comboProgrammer->setPlaceholderText(tr("No programmer"));
        comboProgrammer->setEnabled(false);
        comboDevice->clear();
        comboDevice->setPlaceholderText(tr("No devices"));
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
            if (log) log->appendPlainText(tr("[Error] buffer is empty"));
            return;
        }
        // Export buffer to a temp file
        QString tempPath = exportBufferToTempFileLocal("fmp-write");
        if (tempPath.isEmpty()) {
            if (log) log->appendPlainText(tr("[Error] failed to create temp file for writing"));
            return;
        }
        pendingWriteTempPath_ = tempPath;
        // Write to target
        proc->writeChipImage(p, d, tempPath, optionFlags());
    });

    // Read from target is ready
    connect(proc, &ProcessHandling::readReady, this, [this](const QString& tempPath){
        // Use the same dialog, but with a preselected path, and remove temp afterwards
        loadAtOffsetDialog(tempPath, true);
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
        processBusy_ = true;
        disableBusyButtons();
        QApplication::setOverrideCursor(Qt::BusyCursor);
    });

    // When process finishes, ensure progress bar is at 100% and shows "Idle",
    // and that the UI is re-enabled after process completion
    connect(proc, &ProcessHandling::finished, this,
        [this](int /*exitCode*/, QProcess::ExitStatus /*status*/) {
            processBusy_ = false;
            // Always restore UI first
            updateActionEnabling();
            QApplication::restoreOverrideCursor();
            if (progReadWrite) {
                progReadWrite->setValue(100);
                progReadWrite->setFormat(tr("Idle"));
                progReadWrite->setTextVisible(true);
            }
            if (!pendingWriteTempPath_.isEmpty()) {
                QFile::remove(pendingWriteTempPath_);
                pendingWriteTempPath_.clear();
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
    {
        QSettings settings;
        const QString savedTheme = settings.value(QStringLiteral("ui/theme"), QStringLiteral("famicom")).toString();
        applyTheme(savedTheme, false);
    }
    retranslateUi();
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

    if (obj == log && e->type() == QEvent::Resize) {
        if (auto *bar = log->verticalScrollBar()) {
            bar->setValue(bar->maximum());
        }
    }

    return QMainWindow::eventFilter(obj, e);
}


void MainWindow::setUiEnabled(bool on) {
    // Enabled at boot
    for (auto *w : std::vector<QWidget*>{btnRescan, btnClear, btnSave, btnRead, btnWrite,
                                         btnLoadBinary, btnLoadAdvanced})
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
        btnRescan, comboProgrammer, comboDevice, btnLoadBinary, btnLoadAdvanced,
        btnSave, btnClear
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
    if (btnClear)        btnClear->setEnabled(true);
    if (btnLoadBinary)   btnLoadBinary->setEnabled(true);
    if (btnLoadAdvanced) btnLoadAdvanced->setEnabled(true);
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
    lastChipInfo_ = ci;
    // graceful fallbacks for partial info
    chipName     ->setText(ci.baseName.isEmpty()   ? "-" : ci.baseName);
    chipPackage  ->setText(ci.package.isEmpty()    ? "-" : ci.package);
    if (ci.isLogic) {
        currentIsLogic_ = true;
        chipMemory  ->setText("-");  // logic ICs don’t have a byte size
        chipBusWidth->setText(tr("Logic IC%1")
                              .arg(ci.vectorCount > 0 ? tr(" (%1 vectors)").arg(ci.vectorCount) : QString()));
    } else {
        currentIsLogic_ = false;
        chipMemory  ->setText(prettyBytes(ci.bytes));
        chipBusWidth->setText(ci.wordBits > 0 ? tr("%1-bit").arg(ci.wordBits) : "-");
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
    if (buffer_.isEmpty()) { log->appendPlainText(tr("[Info] Buffer is empty")); return; }
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
        log->appendPlainText(tr("[Error] save: %1").arg(f.errorString()));
        return;
    }
    // Error check the write
    if (f.write(buffer_) != buffer_.size()) {
        log->appendPlainText(tr("[Error] save: write failed"));
        f.close();
        return;
    }
    f.close();
    log->appendPlainText(tr("[Saved] %1 bytes to %2").arg(buffer_.size()).arg(path));
    lastPath_ = QFileInfo(path).absolutePath();
}

// Create temp file from buffer, for writing to target
QString MainWindow::exportBufferToTempFileLocal(const QString& baseName)
{
    if (buffer_.isEmpty()) {
        if (log) log->appendPlainText(tr("[Write] Buffer is empty."));
        return {};
    }

    const QString safeBase = baseName.isEmpty() ? QStringLiteral("image") : baseName;
    const QString ts = QDateTime::currentDateTime().toString("yyMMdd-HHmmss-zzz");
    const QString dir = QDir::tempPath();
    QString file = QString("%1-%2.bin").arg(safeBase, ts);
    QString path = QDir(dir).filePath(file);

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (log) log->appendPlainText(tr("[Write] open temp failed: %1").arg(f.errorString()));
        return {};
    }
    if (f.write(buffer_) != buffer_.size()) {
        if (log) log->appendPlainText(tr("[Write] write temp failed."));
        f.close();
        f.remove();
        return {};
    }
    f.close();

    if (log) log->appendPlainText(tr("[Write] Exported buffer to %1").arg(path));
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
        comboProgrammer->setPlaceholderText(tr("No programmer"));
        if (log) log->appendPlainText(tr("[Error] No programmer found."));
        // (Optional) disable actions that need a programmer here
        return;
    }

    comboProgrammer->addItems(names);
    comboProgrammer->setCurrentIndex(0);
    comboProgrammer->setEnabled(true);
    if (log) log->appendPlainText(tr("[Info] Found %1 programmer(s).").arg(names.size()));
}

void MainWindow::onDevicesListed(const QStringList &names)
{
    QSignalBlocker blocker(comboDevice);
    comboDevice->clear();

    if (names.isEmpty()) {
        comboDevice->setPlaceholderText(tr("No devices for this programmer"));
        comboDevice->setEnabled(false);
        if (log) log->appendPlainText(tr("[Error] No devices found for selected programmer."));
        return;
    }

    QStringList sorted = names;
    sorted.sort(Qt::CaseInsensitive);
    comboDevice->addItems(sorted);
    comboDevice->setCurrentIndex(-1);
    comboDevice->setEnabled(true);

    if (log) log->appendPlainText(tr("[Info] Loaded %1 devices.").arg(sorted.size()));
}

// Load file to buffer at user-specified offset, with optional padding
void MainWindow::loadFileAppendDialog() {
    const QString path = pickFile(tr("Load binary"), QFileDialog::AcceptOpen,
                                  tr("All files (*);;Binary (*.bin)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (log) log->appendPlainText(tr("[Error] open: %1").arg(f.errorString()));
        return;
    }

    const QByteArray data = f.readAll();
    if (data.isEmpty()) {
        if (log) log->appendPlainText(tr("[Warn] File is empty: %1").arg(QFileInfo(path).fileName()));
        return;
    }

    const qulonglong start = static_cast<qulonglong>(buffer_.size());
    buffer_.append(data);
    const qulonglong len = static_cast<qulonglong>(data.size());

    addSegmentAndRefresh(start, len, QFileInfo(path).fileName());

    if (hexModel) hexModel->setBufferRef(&buffer_);
    if (lblBufSize) {
        lblBufSize->setText(tr("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
    }

    if (log) {
        log->appendPlainText(tr("[Loaded] %1 bytes at 0x%2 from %3")
                             .arg(QLocale().toString(len))
                             .arg(QString::number(start, 16).toUpper())
                             .arg(QFileInfo(path).fileName()));
    }

    updateActionEnabling();
}

void MainWindow::loadAtOffsetDialog(QString path, bool deleteOnFinish) {
    // Sanitary cursor
    QApplication::restoreOverrideCursor();

    const auto cleanupTemp = qScopeGuard([&]{
        if (deleteOnFinish && !path.isEmpty()) {
            QFile::remove(path);
        }
    });

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
    if (!deleteOnFinish)
        lastPath_ = QFileInfo(path).absolutePath();
#endif

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        log->appendPlainText(tr("[Error] open: %1").arg(f.errorString()));
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
        L->setForegroundRole(QPalette::WindowText);
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
            lblOffInfo->setText(tr("<b>Offset:</b> 0x%1 (%2)")
                                .arg(QString::number(off, 16).toUpper())
                                .arg(QLocale().toString(off)));
        } else {
            lblOffInfo->setText(tr("<b>Offset:</b> <span style='color:#c00'>invalid</span>"));
        }
        // Length + File info (bold headings, multiline)
        if (haveLen) {
            lblLenInfo->setText(tr("<b>Length:</b> 0x%1 (%2)<br/><b>File size:</b> 0x%3 (%4)")
                                .arg(QString::number(lenReq, 16).toUpper())
                                .arg(QLocale().toString(lenReq))
                                .arg(QString::number(fileSize, 16).toUpper())
                                .arg(QLocale().toString(fileSize)));
        } else {
            lblLenInfo->setText(tr("<b>Length:</b> whole file<br/><b>File size:</b> 0x%1 (%2)")
                                .arg(QString::number(fileSize, 16).toUpper())
                                .arg(QLocale().toString(fileSize)));
        }
        // End address / new buffer size (bold headings, multiline)
        if (offOk && effLen > 0) {
            const qulonglong endAddr = off + effLen - 1;
            const qulonglong newSizeIfAppend = std::max<qulonglong>(buffer_.size(), off + effLen);
            lblEndInfo->setText(tr("<b>End address:</b> 0x%1 (%2)<br/><b>New buffer size:</b> 0x%3 (%4)")
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

        QVector<QPair<qulonglong, qulonglong>> previewSegments;
        if (!bufferSegments.isEmpty()) {
            previewSegments.reserve(bufferSegments.size());
            for (const auto &seg : std::as_const(bufferSegments)) {
                if (seg.length == 0) continue;
                previewSegments.append({seg.start, seg.length});
            }
            std::sort(previewSegments.begin(), previewSegments.end(),
                      [](const QPair<qulonglong, qulonglong> &a,
                         const QPair<qulonglong, qulonglong> &b) { return a.first < b.first; });
        }
        preview->setBufferSegments(std::move(previewSegments));
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
    if (!parseSizeLike(editOff->text(), off)) { log->appendPlainText(tr("[Error] invalid offset")); return; }
    if (!editLen->text().trimmed().isEmpty() && !parseSizeLike(editLen->text(), len)) {
        log->appendPlainText(tr("[Error] invalid length")); return;
    }
    if (!parseSizeLike(editPad->text(), pad) || pad > 0xFF) {
        log->appendPlainText(tr("[Error] invalid pad")); return;
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
        displayName += tr(" (padded 0x%1 %2)").arg(padByteText, parts.join(QLatin1Char(' ')));
    }
    addSegmentAndRefresh(off, effLen, displayName);
    if (!deleteOnFinish)
        lastPath_ = QFileInfo(path).absolutePath();

    log->appendPlainText(tr("[Loaded] %1 bytes at 0x%2 from %3")
                         .arg(QLocale().toString(effLen))
                         .arg(QString::number(off, 16).toUpper())
                         .arg(QFileInfo(path).fileName()));

    if (lblBufSize)
        lblBufSize->setText(tr("Size: %1 (0x%2)")
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

void MainWindow::onLegendRowDoubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    const int row = index.row();
    if (row < 0 || row >= bufferSegments.size()) return;
    if (!hexModel || !tableHex) return;
    if (buffer_.isEmpty()) return;

    const int bytesPerRow = std::max(1, hexModel->getBytesPerRow());
    const auto &segment = bufferSegments.at(row);
    qulonglong start = segment.start;
    if (start >= qulonglong(buffer_.size()))
        start = buffer_.isEmpty() ? 0 : qulonglong(buffer_.size() - 1);

    const int hexRowCount = hexModel->rowCount();
    const int hexColCount = hexModel->columnCount();
    if (hexRowCount <= 0 || hexColCount <= 0) return;

    int targetRow = static_cast<int>(start / bytesPerRow);
    targetRow = std::clamp(targetRow, 0, hexRowCount - 1);
    int targetColumn = static_cast<int>((start % bytesPerRow) + 1);
    if (targetColumn >= hexColCount) targetColumn = hexColCount - 1;
    if (targetColumn < 0) targetColumn = 0;

    const QModelIndex targetIndex = hexModel->index(targetRow, targetColumn);
    if (!targetIndex.isValid()) return;
    tableHex->scrollTo(targetIndex, QAbstractItemView::PositionAtCenter);
}

void MainWindow::onLegendFilesDropped(int row, const QList<QUrl> &urls) {
    if (urls.isEmpty()) return;

    int insertIndex = std::clamp(row, 0, static_cast<int>(bufferSegments.size()));
    qulonglong insertStart = (insertIndex < bufferSegments.size())
                           ? bufferSegments[insertIndex].start
                           : qulonglong(buffer_.size());

    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (log) log->appendPlainText(tr("[Error] Failed to open dropped file: %1").arg(path));
            continue;
        }
        QByteArray data = file.readAll();
        file.close();
        if (data.isEmpty()) {
            if (log) log->appendPlainText(tr("[Warn] Dropped file empty: %1").arg(path));
            continue;
        }

        const qint64 len = data.size();
        for (int i = insertIndex; i < bufferSegments.size(); ++i) {
            bufferSegments[i].start += qulonglong(len);
        }

        buffer_.insert(static_cast<int>(insertStart), data);

        BufferSegment seg;
        seg.start  = insertStart;
        seg.length = qulonglong(len);
        seg.label  = QFileInfo(path).fileName();
        seg.note   = {};
        seg.id     = nextSegmentId_++;
        bufferSegments.insert(insertIndex, seg);

        if (log) {
            log->appendPlainText(tr("[Dropped] %1 bytes at 0x%2 from %3")
                                 .arg(QLocale().toString(qulonglong(len)))
                                 .arg(QString::number(seg.start, 16).toUpper())
                                 .arg(seg.label));
        }

        insertIndex += 1;
        insertStart += qulonglong(len);
    }

    updateLegendTable();
    if (legendTable && insertIndex > 0) {
        const int selectRow = insertIndex - 1;
        legendTable->selectRow(selectRow);
        legendTable->scrollTo(segmentModel->index(selectRow, 0),
                              QAbstractItemView::PositionAtCenter);
    }
    if (hexModel) hexModel->setBufferRef(&buffer_);
    if (lblBufSize) {
        lblBufSize->setText(tr("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
    }
    updateActionEnabling();
}

void MainWindow::onLegendContextMenuRequested(const QPoint &pos) {
    if (!legendTable) return;
    const QModelIndex index = legendTable->indexAt(pos);
    if (!index.isValid()) return;
    const int row = index.row();
    if (row < 0 || row >= bufferSegments.size()) return;

    const auto &seg = bufferSegments.at(row);
    QMenu menu(legendTable);
    const QString displayLabel = seg.label.isEmpty() ? tr("Segment") : seg.label;
    QAction *deleteAction = menu.addAction(tr("Delete \"%1\"").arg(displayLabel));
    QAction *fillAction   = menu.addAction(tr("Fill \"%1\" with 0xFF").arg(displayLabel));

    const QPoint globalPos = legendTable->viewport()->mapToGlobal(pos);
    QAction *chosen = menu.exec(globalPos);
    if (!chosen) return;

    if (chosen == deleteAction) {
        deleteSegmentAt(row);
    } else if (chosen == fillAction) {
        fillSegmentWithValue(row, 0xFF);
    }
}

void MainWindow::deleteSegmentAt(int row) {
    if (row < 0 || row >= bufferSegments.size()) return;
    const BufferSegment seg = bufferSegments.at(row);
    const qulonglong bufferSize = static_cast<qulonglong>(buffer_.size());
    const QString displayName = seg.label.isEmpty() ? tr("segment") : seg.label;
    if (seg.length == 0) {
        bufferSegments.removeAt(row);
        if (log) log->appendPlainText(tr("[Segment] Removed empty %1").arg(displayName));
    } else if (seg.start >= bufferSize) {
        bufferSegments.removeAt(row);
        if (log) log->appendPlainText(tr("[Segment] Removed out-of-range %1").arg(displayName));
    } else {
        const qulonglong available = bufferSize - seg.start;
        const qulonglong effectiveLen = std::min(seg.length, available);
        const int start = static_cast<int>(seg.start);
        const int len = static_cast<int>(effectiveLen);
        if (len > 0) buffer_.remove(start, len);
        bufferSegments.removeAt(row);
        for (int i = row; i < bufferSegments.size(); ++i) {
            bufferSegments[i].start -= effectiveLen;
        }
        if (log) {
            log->appendPlainText(tr("[Segment] Deleted %1 bytes from 0x%2 (%3)")
                                 .arg(QLocale().toString(effectiveLen))
                                 .arg(QString::number(seg.start, 16).toUpper())
                                 .arg(displayName));
        }
    }

    updateLegendTable();
    if (legendTable) {
        if (!bufferSegments.isEmpty()) {
            const int last = static_cast<int>(bufferSegments.size() - 1);
            const int selectRow = std::clamp(row, 0, last);
            legendTable->selectRow(selectRow);
        } else {
            legendTable->clearSelection();
        }
    }
    if (hexModel) hexModel->setBufferRef(&buffer_);
    if (lblBufSize) {
        lblBufSize->setText(tr("Size: %1 (0x%2)")
                            .arg(QLocale().toString(buffer_.size()))
                            .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
    }
    updateActionEnabling();
}

void MainWindow::fillSegmentWithValue(int row, quint8 value) {
    if (row < 0 || row >= bufferSegments.size()) return;
    auto &segment = bufferSegments[row];
    const qulonglong bufferSize = static_cast<qulonglong>(buffer_.size());
    if (segment.start >= bufferSize || segment.length == 0) return;

    const qulonglong available = bufferSize - segment.start;
    const qulonglong effectiveLen = std::min(segment.length, available);
    if (effectiveLen == 0) return;

    const int start = static_cast<int>(segment.start);
    const int len = static_cast<int>(effectiveLen);
    const char fillChar = static_cast<char>(value);
    for (int i = 0; i < len; ++i) {
        buffer_[start + i] = fillChar;
    }

    segment.label = tr("Fill 0x%1").arg(QString::number(value, 16).toUpper().rightJustified(2, QLatin1Char('0')));
    segment.note.clear();

    updateLegendTable();
    if (legendTable && segmentModel) {
        legendTable->selectRow(row);
        legendTable->scrollTo(segmentModel->index(row, 0), QAbstractItemView::PositionAtCenter);
    }
    if (hexModel) hexModel->setBufferRef(&buffer_);
    if (log) {
        log->appendPlainText(tr("[Segment] Filled %1 bytes at 0x%2 with 0x%3")
                             .arg(QLocale().toString(effectiveLen))
                             .arg(QString::number(segment.start, 16).toUpper())
                             .arg(QString::number(value, 16).toUpper().rightJustified(2, QLatin1Char('0'))));
    }
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
            out.append(BufferSegment{ sBeg, nBeg - sBeg, seg.label, tr(" (partial)") , seg.id });

        // middle overlapped portion is dropped

        if (hasRightRemainder)
            out.append(BufferSegment{ nEnd, sEnd - nEnd, seg.label, tr(" (partial)") , seg.id });
    }

    const qulonglong thisInsertId = nextSegmentId_++;
    out.append(BufferSegment{ nBeg, length, label,
                              anyPartialOverlap ? tr(" (overlap)") : QString(),
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

void MainWindow::applyLanguage(const QString &langCode, bool saveSetting) {
    QString normalized = langCode.trimmed().toLower();
    if (normalized != QLatin1String("en") && normalized != QLatin1String("ja")) {
        normalized = QStringLiteral("auto");
    }
    currentLangCode_ = normalized;

    if (saveSetting) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/language"), currentLangCode_);
    }

    if (qtTranslator_) {
        qApp->removeTranslator(qtTranslator_);
        delete qtTranslator_;
        qtTranslator_ = nullptr;
    }
    if (appTranslator_) {
        qApp->removeTranslator(appTranslator_);
        delete appTranslator_;
        appTranslator_ = nullptr;
    }

    if (currentLangCode_ != QLatin1String("en")) {
        const QLocale targetLocale = (currentLangCode_ == QLatin1String("auto"))
            ? QLocale::system()
            : QLocale(currentLangCode_);

        auto *qtTr = new QTranslator(this);
        if (qtTr->load(targetLocale, QStringLiteral("qtbase"), QStringLiteral("_"),
                       QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            qApp->installTranslator(qtTr);
            qtTranslator_ = qtTr;
        } else {
            delete qtTr;
        }

        auto *appTr = new QTranslator(this);
        if (appTr->load(targetLocale, QStringLiteral("fireminipro"), QStringLiteral("_"),
                        QStringLiteral(":/i18n"))) {
            qApp->installTranslator(appTr);
            appTranslator_ = appTr;
        } else {
            delete appTr;
        }
    }

    if (actLangAuto_) actLangAuto_->setChecked(currentLangCode_ == QLatin1String("auto"));
    if (actLangEn_)   actLangEn_->setChecked(currentLangCode_ == QLatin1String("en"));
    if (actLangJa_)   actLangJa_->setChecked(currentLangCode_ == QLatin1String("ja"));
}

void MainWindow::applyTheme(const QString &themeCode, bool saveSetting) {
    QString normalized = themeCode.trimmed().toLower();
    if (normalized != QLatin1String("nes")
        && normalized != QLatin1String("light")
        && normalized != QLatin1String("gray")
        && normalized != QLatin1String("dark")) {
        normalized = QStringLiteral("famicom");
    }
    currentThemeCode_ = normalized;

    if (saveSetting) {
        QSettings settings;
        settings.setValue(QStringLiteral("ui/theme"), currentThemeCode_);
    }

    if (actThemeFamicom_) actThemeFamicom_->setChecked(currentThemeCode_ == QLatin1String("famicom"));
    if (actThemeNes_)     actThemeNes_->setChecked(currentThemeCode_ == QLatin1String("nes"));
    if (actThemeLight_)   actThemeLight_->setChecked(currentThemeCode_ == QLatin1String("light"));
    if (actThemeGray_)    actThemeGray_->setChecked(currentThemeCode_ == QLatin1String("gray"));
    if (actThemeDark_)    actThemeDark_->setChecked(currentThemeCode_ == QLatin1String("dark"));

    // Use Fusion style so palettes, checkmarks, comboboxes, and button heights render reliably on macOS/Windows/Linux
    static bool fusionApplied = false;
    if (!fusionApplied) {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
            qApp->setStyle(fusion);
        }
        fusionApplied = true;
    }

    QPalette pal;
    QString qss;
    if (currentThemeCode_ == QLatin1String("famicom")) {
        // Famicom / NES inspired: slightly warm yellowish-cream white + Famicom crimson red + gold trim
        const QColor bg(QStringLiteral("#F2EBD5"));
        const QColor card(QStringLiteral("#FAF5E4"));
        const QColor base(QStringLiteral("#FFFDF7"));
        const QColor alt(QStringLiteral("#F4EDE0"));
        const QColor text(QStringLiteral("#2B1B17"));
        const QColor btn(QStringLiteral("#EDE3C6"));
        const QColor hi(QStringLiteral("#A61C24"));
        const QColor hiText(QStringLiteral("#FFF9EB"));
        const QColor dis(QStringLiteral("#9C8E75"));
        const QColor mid(QStringLiteral("#C9BA95"));

        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::ToolTipBase, card);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Highlight, hi);
        pal.setColor(QPalette::HighlightedText, hiText);
        pal.setColor(QPalette::Mid, mid);
        pal.setColor(QPalette::Dark, QColor(QStringLiteral("#9C8E75")));
        pal.setColor(QPalette::Light, base);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Text, dis);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#E6DEC7")));

        qss = QStringLiteral(R"(
            QGroupBox {
                background-color: #FAF5E4;
                border: 1.5px solid #C9BA95;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 6px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 5px;
                color: #9E1B22;
                background-color: #F2EBD5;
            }
            QPushButton#btnWritePrimary:enabled {
                background-color: #A61C24;
                color: #FFF9EB;
                border: 1px solid #C59B38;
                font-weight: bold;
            }
            QPushButton#btnWritePrimary:hover:enabled {
                background-color: #88141B;
                color: #FFF9EB;
            }
            QHeaderView::section {
                background-color: #A61C24;
                color: #FFF8E7;
                padding: 4px 6px;
                border: none;
                border-right: 1px solid #C59B38;
                font-weight: bold;
            }
            QProgressBar {
                border: 1px solid #B8A67D;
                border-radius: 4px;
                text-align: center;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #A61C24;
                border-radius: 3px;
            }
        )");
    } else if (currentThemeCode_ == QLatin1String("nes")) {
        // Nintendo Entertainment System (NES-001): Light gray (#D0D0D4), Charcoal (#343434), Red A/B (#D92B2B)
        const QColor bg(QStringLiteral("#D0D0D4"));
        const QColor card(QStringLiteral("#E2E2E6"));
        const QColor base(QStringLiteral("#F4F4F6"));
        const QColor alt(QStringLiteral("#E8E8EC"));
        const QColor text(QStringLiteral("#1E1E1E"));
        const QColor btn(QStringLiteral("#C2C2C7"));
        const QColor hi(QStringLiteral("#D92B2B"));
        const QColor hiText(QStringLiteral("#FFFFFF"));
        const QColor dis(QStringLiteral("#787880"));
        const QColor mid(QStringLiteral("#9E9EA6"));

        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::ToolTipBase, card);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Highlight, hi);
        pal.setColor(QPalette::HighlightedText, hiText);
        pal.setColor(QPalette::Mid, mid);
        pal.setColor(QPalette::Dark, QColor(QStringLiteral("#343434")));
        pal.setColor(QPalette::Light, base);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Text, dis);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#C8C8CD")));

        qss = QStringLiteral(R"(
            QGroupBox {
                background-color: #E2E2E6;
                border: 1.5px solid #4A4A50;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 6px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 5px;
                color: #D92B2B;
                background-color: #D0D0D4;
            }
            QPushButton#btnWritePrimary:enabled {
                background-color: #D92B2B;
                color: #FFFFFF;
                border: 1px solid #343434;
                font-weight: bold;
            }
            QPushButton#btnWritePrimary:hover:enabled {
                background-color: #B81D1D;
            }
            QHeaderView::section {
                background-color: #343434;
                color: #FFFFFF;
                padding: 4px 6px;
                border: none;
                border-right: 1px solid #6E6E75;
                font-weight: bold;
            }
            QProgressBar {
                border: 1px solid #4A4A50;
                border-radius: 4px;
                text-align: center;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #D92B2B;
                border-radius: 3px;
            }
        )");
    } else if (currentThemeCode_ == QLatin1String("light")) {
        const QColor bg(QStringLiteral("#F3F5F8"));
        const QColor base(QStringLiteral("#FFFFFF"));
        const QColor alt(QStringLiteral("#F8FAFC"));
        const QColor text(QStringLiteral("#1E293B"));
        const QColor btn(QStringLiteral("#E7ECF2"));
        const QColor hi(QStringLiteral("#2563EB"));
        const QColor hiText(QStringLiteral("#FFFFFF"));
        const QColor dis(QStringLiteral("#94A3B8"));
        const QColor mid(QStringLiteral("#CBD5E1"));

        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::ToolTipBase, base);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Highlight, hi);
        pal.setColor(QPalette::HighlightedText, hiText);
        pal.setColor(QPalette::Mid, mid);
        pal.setColor(QPalette::Dark, QColor(QStringLiteral("#94A3B8")));
        pal.setColor(QPalette::Light, base);
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Text, dis);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#F1F5F9")));

        qss = QStringLiteral(R"(
            QGroupBox {
                background-color: #FFFFFF;
                border: 1px solid #CBD5E1;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 6px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 5px;
                color: #1D4ED8;
                background-color: #F3F5F8;
            }
            QPushButton#btnWritePrimary:enabled {
                background-color: #2563EB;
                color: #FFFFFF;
                border: 1px solid #1D4ED8;
                font-weight: bold;
            }
            QPushButton#btnWritePrimary:hover:enabled {
                background-color: #1D4ED8;
            }
            QHeaderView::section {
                background-color: #E2E8F0;
                color: #1E293B;
                padding: 4px 6px;
                border: none;
                border-right: 1px solid #CBD5E1;
                font-weight: bold;
            }
            QProgressBar {
                border: 1px solid #CBD5E1;
                border-radius: 4px;
                text-align: center;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #2563EB;
                border-radius: 3px;
            }
        )");
    } else if (currentThemeCode_ == QLatin1String("gray")) {
        // Gray theme
        const QColor bg(QStringLiteral("#2B2F36"));
        const QColor card(QStringLiteral("#323740"));
        const QColor base(QStringLiteral("#1E2126"));
        const QColor alt(QStringLiteral("#252930"));
        const QColor text(QStringLiteral("#E5E7EB"));
        const QColor btn(QStringLiteral("#3F4550"));
        const QColor hi(QStringLiteral("#3B82F6"));
        const QColor hiText(QStringLiteral("#FFFFFF"));
        const QColor dis(QStringLiteral("#6B7280"));
        const QColor mid(QStringLiteral("#4B5260"));

        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::ToolTipBase, card);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Highlight, hi);
        pal.setColor(QPalette::HighlightedText, hiText);
        pal.setColor(QPalette::Mid, mid);
        pal.setColor(QPalette::Dark, QColor(QStringLiteral("#1B1E23")));
        pal.setColor(QPalette::Light, QColor(QStringLiteral("#525866")));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Text, dis);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#2E3239")));

        qss = QStringLiteral(R"(
            QGroupBox {
                background-color: #323740;
                border: 1px solid #4B5260;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 6px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 5px;
                color: #93C5FD;
                background-color: #2B2F36;
            }
            QPushButton#btnWritePrimary:enabled {
                background-color: #3B82F6;
                color: #FFFFFF;
                border: 1px solid #2563EB;
                font-weight: bold;
            }
            QPushButton#btnWritePrimary:hover:enabled {
                background-color: #2563EB;
            }
            QHeaderView::section {
                background-color: #3B404A;
                color: #E5E7EB;
                padding: 4px 6px;
                border: none;
                border-right: 1px solid #4B5260;
                font-weight: bold;
            }
            QProgressBar {
                border: 1px solid #4B5260;
                border-radius: 4px;
                text-align: center;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #3B82F6;
                border-radius: 3px;
            }
        )");
    } else {
        // Dark (Original) theme matching original macOS dark screenshot
        const QColor bg(QStringLiteral("#262626"));
        const QColor card(QStringLiteral("#2C2C2C"));
        const QColor base(QStringLiteral("#141414"));
        const QColor alt(QStringLiteral("#1A1A1A"));
        const QColor text(QStringLiteral("#EAEAEA"));
        const QColor btn(QStringLiteral("#464646"));
        const QColor hi(QStringLiteral("#0A84FF"));
        const QColor hiText(QStringLiteral("#FFFFFF"));
        const QColor dis(QStringLiteral("#6E6E6E"));
        const QColor mid(QStringLiteral("#3C3C3C"));

        pal.setColor(QPalette::Window, bg);
        pal.setColor(QPalette::WindowText, text);
        pal.setColor(QPalette::Base, base);
        pal.setColor(QPalette::AlternateBase, alt);
        pal.setColor(QPalette::Text, text);
        pal.setColor(QPalette::ToolTipBase, card);
        pal.setColor(QPalette::ToolTipText, text);
        pal.setColor(QPalette::Button, btn);
        pal.setColor(QPalette::ButtonText, text);
        pal.setColor(QPalette::Highlight, hi);
        pal.setColor(QPalette::HighlightedText, hiText);
        pal.setColor(QPalette::Mid, mid);
        pal.setColor(QPalette::Dark, QColor(QStringLiteral("#121212")));
        pal.setColor(QPalette::Light, QColor(QStringLiteral("#555555")));
        pal.setColor(QPalette::Disabled, QPalette::WindowText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Text, dis);
        pal.setColor(QPalette::Disabled, QPalette::ButtonText, dis);
        pal.setColor(QPalette::Disabled, QPalette::Button, QColor(QStringLiteral("#323232")));

        qss = QStringLiteral(R"(
            QGroupBox {
                background-color: #2A2A2A;
                border: 1px solid #3E3E3E;
                border-radius: 6px;
                margin-top: 10px;
                padding-top: 6px;
                font-weight: bold;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                subcontrol-position: top left;
                left: 8px;
                padding: 0 5px;
                color: #D4D4D4;
                background-color: #262626;
            }
            QPushButton#btnWritePrimary:enabled {
                background-color: #0A84FF;
                color: #FFFFFF;
                border: 1px solid #0066CC;
                font-weight: bold;
            }
            QPushButton#btnWritePrimary:hover:enabled {
                background-color: #0070E0;
            }
            QHeaderView::section {
                background-color: #242424;
                color: #DCDCDC;
                padding: 4px 6px;
                border: none;
                border-right: 1px solid #3A3A3A;
                font-weight: bold;
            }
            QProgressBar {
                background-color: #1A3A6E;
                color: #FFFFFF;
                border: 1px solid #3E3E3E;
                border-radius: 4px;
                text-align: center;
                font-weight: bold;
            }
            QProgressBar::chunk {
                background-color: #0A84FF;
                border-radius: 3px;
            }
        )");
    }

    qApp->setPalette(pal);
    qApp->setStyleSheet(qss);
}

void MainWindow::openSettingsDialog() {
    QDialog dlg(this);
    dlg.setMinimumWidth(420);

    auto *vbox = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout();

    auto *comboLang = new QComboBox(&dlg);
    comboLang->addItem(tr("System Default"), QStringLiteral("auto"));
    comboLang->addItem(QStringLiteral("English"), QStringLiteral("en"));
    comboLang->addItem(QStringLiteral("日本語 (Japanese)"), QStringLiteral("ja"));
    if (const int i = comboLang->findData(currentLangCode_); i >= 0) {
        comboLang->setCurrentIndex(i);
    }

    auto *comboTheme = new QComboBox(&dlg);
    comboTheme->addItem(tr("Famicom (Red & Cream)"), QStringLiteral("famicom"));
    comboTheme->addItem(tr("NES (Gray & Red)"), QStringLiteral("nes"));
    comboTheme->addItem(tr("Light"), QStringLiteral("light"));
    comboTheme->addItem(tr("Gray"), QStringLiteral("gray"));
    comboTheme->addItem(tr("Dark (Original)"), QStringLiteral("dark"));
    if (const int i = comboTheme->findData(currentThemeCode_); i >= 0) {
        comboTheme->setCurrentIndex(i);
    }

    auto *lblLangRow = new QLabel(&dlg);
    auto *lblThemeRow = new QLabel(&dlg);
    auto *lblHint = new QLabel(&dlg);
    lblHint->setWordWrap(true);

    form->addRow(lblLangRow, comboLang);
    form->addRow(lblThemeRow, comboTheme);
    vbox->addLayout(form);
    vbox->addWidget(lblHint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::accept);
    vbox->addWidget(buttons);

    auto syncDlgText = [&]() {
        dlg.setWindowTitle(tr("Settings"));
        lblLangRow->setText(tr("Language:"));
        lblThemeRow->setText(tr("Theme:"));
        lblHint->setText(tr("Changes are applied immediately and saved automatically."));
        if (const int i = comboLang->findData(QStringLiteral("auto")); i >= 0) {
            comboLang->setItemText(i, tr("System Default"));
        }
        if (const int i = comboTheme->findData(QStringLiteral("famicom")); i >= 0) {
            comboTheme->setItemText(i, tr("Famicom (Red & Cream)"));
        }
        if (const int i = comboTheme->findData(QStringLiteral("nes")); i >= 0) {
            comboTheme->setItemText(i, tr("NES (Gray & Red)"));
        }
        if (const int i = comboTheme->findData(QStringLiteral("light")); i >= 0) {
            comboTheme->setItemText(i, tr("Light"));
        }
        if (const int i = comboTheme->findData(QStringLiteral("gray")); i >= 0) {
            comboTheme->setItemText(i, tr("Gray"));
        }
        if (const int i = comboTheme->findData(QStringLiteral("dark")); i >= 0) {
            comboTheme->setItemText(i, tr("Dark (Original)"));
        }
    };
    syncDlgText();

    connect(comboLang, &QComboBox::currentIndexChanged, &dlg, [&](int idx) {
        if (idx < 0) return;
        const QString code = comboLang->itemData(idx).toString();
        if (code == currentLangCode_) return;
        applyLanguage(code, true);
        retranslateUi();
        syncDlgText();
    });

    connect(comboTheme, &QComboBox::currentIndexChanged, &dlg, [&](int idx) {
        if (idx < 0) return;
        const QString code = comboTheme->itemData(idx).toString();
        if (code == currentThemeCode_) return;
        applyTheme(code, true);
    });

    dlg.exec();
}

void MainWindow::retranslateUi() {
    setWindowTitle(tr("FireMinipro - An Open Source GUI for minipro CLI tool %1")
                   .arg(QStringLiteral(FIREMINIPRO_VERSION)));

    if (menuApp_)        menuApp_->setTitle(tr("&FireMinipro"));
    if (actAbout_)       actAbout_->setText(tr("&About FireMinipro…"));
    if (actPreferences_) actPreferences_->setText(tr("&Settings…"));
    if (actQuit_)        actQuit_->setText(tr("&Quit"));
    if (menuLanguage_)   menuLanguage_->setTitle(tr("&Language"));
    if (actLangAuto_)    actLangAuto_->setText(tr("System Default"));
    if (menuTheme_)      menuTheme_->setTitle(tr("&Theme"));
    if (actThemeFamicom_) actThemeFamicom_->setText(tr("Famicom (Red & Cream)"));
    if (actThemeNes_)    actThemeNes_->setText(tr("NES (Gray & Red)"));
    if (actThemeLight_)  actThemeLight_->setText(tr("Light"));
    if (actThemeGray_)   actThemeGray_->setText(tr("Gray"));
    if (actThemeDark_)   actThemeDark_->setText(tr("Dark (Original)"));
    if (actOpenSettings_) actOpenSettings_->setText(tr("Open Settings Window…"));
    if (btnSettings_)    btnSettings_->setText(tr("Settings"));

    if (groupTargets_) groupTargets_->setTitle(tr("Target"));
    if (lblProg_)      lblProg_->setText(tr("Programmer:"));
    if (lblDev_)       lblDev_->setText(tr("Device:"));
    if (btnRescan)     btnRescan->setText(tr("Rescan"));
    if (comboProgrammer) comboProgrammer->setPlaceholderText(tr("No programmer"));
    if (comboDevice) {
        comboDevice->setPlaceholderText(
            (comboProgrammer && comboProgrammer->count() > 0 && comboDevice->count() == 0)
            ? tr("No devices for this programmer")
            : tr("No devices"));
    }

    if (groupChipInfo_)   groupChipInfo_->setTitle(tr("Chip information"));
    if (lblChipKeyName_)  lblChipKeyName_->setText(tr("Name:"));
    if (lblChipKeyPkg_)   lblChipKeyPkg_->setText(tr("Package:"));
    if (lblChipKeyMem_)   lblChipKeyMem_->setText(tr("Memory:"));
    if (lblChipKeyWidth_) lblChipKeyWidth_->setText(tr("Mem width:"));
    if (lblChipKeyProto_) lblChipKeyProto_->setText(tr("Protocol:"));
    updateChipInfo(lastChipInfo_);

    if (groupBuffer_)     groupBuffer_->setTitle(tr("Buffer"));
    if (btnLoadBinary)    btnLoadBinary->setText(tr("Load file to buffer"));
    if (btnLoadAdvanced)  btnLoadAdvanced->setText(tr("Advanced load…"));
    if (btnClear)         btnClear->setText(tr("Clear buffer"));
    if (btnSave)          btnSave->setText(tr("Save buffer"));
    if (btnRead)          btnRead->setText(tr("Read from target"));
    if (btnWrite)         btnWrite->setText(tr("Write to target"));
    if (chkAsciiSwap)     chkAsciiSwap->setText(tr("ASCII byteswap (16-bit)"));
    if (lblBufSize) {
        if (buffer_.isEmpty()) {
            lblBufSize->setText(tr("Size: 0 (0x0)"));
        } else {
            lblBufSize->setText(tr("Size: %1 (0x%2)")
                                .arg(QLocale().toString(buffer_.size()))
                                .arg(QString::number(qulonglong(buffer_.size()), 16).toUpper()));
        }
    }
    if (progReadWrite && !processBusy_) {
        progReadWrite->setFormat(tr("Idle"));
    }

    if (groupDevOps_)     groupDevOps_->setTitle(tr("Device operations"));
    if (btnBlankCheck)    btnBlankCheck->setText(tr("Blank check"));
    if (btnEraseDevice)   btnEraseDevice->setText(tr("Erase device"));
    if (btnTestLogic)     btnTestLogic->setText(tr("Test logic"));

    if (groupOpts_)       groupOpts_->setTitle(tr("Device Options"));
    if (chkSkipVerify)    chkSkipVerify->setText(tr("Skip verify"));
    if (chkIgnoreId)      chkIgnoreId->setText(tr("Ignore ID error"));
    if (chkSkipId)        chkSkipId->setText(tr("Skip ID check"));
    if (chkNoSizeErr)     chkNoSizeErr->setText(tr("Ignore size error"));

    if (hexModel && hexModel->columnCount() > 0) {
        emit hexModel->headerDataChanged(Qt::Horizontal, 0, hexModel->columnCount() - 1);
    }
    if (segmentModel && segmentModel->columnCount() > 0) {
        emit segmentModel->headerDataChanged(Qt::Horizontal, 0, segmentModel->columnCount() - 1);
    }
}
