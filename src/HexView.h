#pragma once
#include <QFont>
#include <QString>
#include <QAbstractTableModel>
#include <QByteArray>
#include <QBitArray>
#include <QBrush>
#include <QColor>
#include <algorithm>

class HexView : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit HexView(QObject *parent = nullptr);

    // external buffer management
    void setBufferRef(QByteArray *buffer);        // points to an external QByteArray
    void clear();                                 // clears view (does not clear external buffer)
    void setBytesPerRow(int n);                   // default 16
    void setSwapAscii16(bool on);                 // display only
    void clearDirty();
    bool isDirty(qint64 offset) const;
    int  dirtyCount() const;

    // QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    // convenience
    static bool isPrintable(uint8_t b);

private:
    QByteArray *buf_ = nullptr;   // not owned
    QBitArray dirty_; // one flag bit per byte in buf_, for tracking edits
    int bytesPerRow_ = 16;
    bool asciiSwap16_ = false;

    inline bool inRange(qint64 offset) const {
        return buf_ && offset >= 0 && offset < buf_->size();
    }
};
