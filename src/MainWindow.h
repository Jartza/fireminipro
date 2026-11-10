#pragma once

#include <QMainWindow>
#include <QFileDialog>
#include <QByteArray>
#include <QStringList>
#include <QUrl>
#include "ProcessHandling.h"

class QComboBox;
class QPushButton;
class QTableView;
class QPlainTextEdit;
class QCheckBox;
class QLabel;
class QWidget;
class QGroupBox;
class QMenu;
class QAction;
class QTranslator;
class HexView;
class QProgressBar;
class SegmentView;
class QModelIndex;
class SegmentTableView;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;

private slots:
    void saveBufferToFile();
    void loadAtOffsetDialog(QString path = {}, bool deleteOnFinish = false);
    void loadFileAppendDialog();
    void onDevicesScanned(const QStringList &names);
    void onDevicesListed(const QStringList &names);
    void onSegmentRowReordered(int from, int to);
    void onLegendRowDoubleClicked(const QModelIndex &index);
    void onLegendFilesDropped(int row, const QList<QUrl> &urls);
    void onLegendContextMenuRequested(const QPoint &pos);

    QString pickFile(const QString &title, QFileDialog::AcceptMode mode,
                     const QString &filters = QString());

private:
    // Menus & i18n
    QMenu       *menuApp_{};
    QAction     *actAbout_{};
    QAction     *actPreferences_{};
    QAction     *actQuit_{};
    QMenu       *menuLanguage_{};
    QAction     *actLangAuto_{};
    QAction     *actLangEn_{};
    QAction     *actLangJa_{};
    QMenu       *menuTheme_{};
    QAction     *actThemeFamicom_{};
    QAction     *actThemeNes_{};
    QAction     *actThemeLight_{};
    QAction     *actThemeGray_{};
    QAction     *actThemeDark_{};
    QAction     *actOpenSettings_{};
    QAction     *actOpenSettings_{};
    QTranslator *qtTranslator_{};
    QTranslator *appTranslator_{};
    QString      currentLangCode_{QStringLiteral("auto")};
    QString      currentThemeCode_{QStringLiteral("famicom")};
    bool         processBusy_{false};
    ProcessHandling::ChipInfo lastChipInfo_{};

    // Group boxes & labels for retranslation
    QPushButton *btnSettings_{};
    QGroupBox   *groupTargets_{};
    QLabel      *lblProg_{};
    QLabel      *lblDev_{};
    QGroupBox   *groupChipInfo_{};
    QLabel      *lblChipKeyName_{};
    QLabel      *lblChipKeyPkg_{};
    QLabel      *lblChipKeyMem_{};
    QLabel      *lblChipKeyWidth_{};
    QLabel      *lblChipKeyProto_{};
    QGroupBox   *groupBuffer_{};
    QGroupBox   *groupDevOps_{};
    QGroupBox   *groupOpts_{};

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
    QPushButton *btnLoadBinary{};
    QPushButton *btnLoadAdvanced{};
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
    QFont logFontDefault_;
    QFont logFontFixed_;

    // Hex view model
    HexView *hexModel{};

    // Progress bar
    QProgressBar* progReadWrite{};

    // In-memory buffer
    QByteArray buffer_;
    QString    lastPath_;
    QString    pendingWriteTempPath_;

    // Buffer segment legend storage
    struct BufferSegment {
        qulonglong start{};
        qulonglong length{};
        QString    label;
        QString    note;
        qulonglong id{};
    };

    // Buffer segment legend
    QList<BufferSegment> bufferSegments{};
    SegmentTableView *legendTable{};
    SegmentView *segmentModel{};
    qulonglong nextSegmentId_ = 1;

    // Process handling helper
    ProcessHandling *proc{};

    // If selected device is a logic IC
    bool currentIsLogic_ = false;

    // Buffer legend manipulation
    void updateLegendTable();
    void addSegmentAndRefresh(qulonglong start, qulonglong length, const QString &label);
    void applyLogFontForDevice();
    void deleteSegmentAt(int row);
    void fillSegmentWithValue(int row, quint8 value);

    // Helpers
    QStringList optionFlags() const;
    void setUiEnabled(bool on);
    void disableBusyButtons();
    void updateActionEnabling();
    void updateChipInfo(const ProcessHandling::ChipInfo &ci);
    void clearChipInfo();
    void applyLanguage(const QString &langCode, bool saveSetting);
    void applyTheme(const QString &themeCode, bool saveSetting);
    void openSettingsDialog();
    void retranslateUi();
    QString exportBufferToTempFileLocal(const QString& baseName);

    // parsing / buffer helpers
    bool parseSizeLike(const QString &in, qulonglong &out);
    void ensureBufferSize(int newSize, char padByte);
    void patchBuffer(int offset, const QByteArray &data, char padByte);

protected:
      bool eventFilter(QObject *obj, QEvent *event) override;

};
