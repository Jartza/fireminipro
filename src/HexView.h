#pragma once

#include <QAbstractTableModel>
#include <QByteArray>
#include <QSet>

class HexView : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit HexView(QObject *parent = nullptr);

    void setBufferRef(QByteArray *buffer);
    void clear();

    void setBytesPerRow(int n);
    int  getBytesPerRow() const { return bytesPerRow_; }

    void setSwapAscii16(bool on);

    // QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant headerData(int section, Qt::Orientation o, int role) const override;
    QVariant data(const QModelIndex &idx, int role) const override;
    Qt::ItemFlags flags(const QModelIndex &idx) const override;
    bool setData(const QModelIndex &idx, const QVariant &val, int role) override;

    // dirty tracking
    void clearDirty();
    bool isDirty(qint64 off) const;
    int  dirtyCount() const;

private:
    static bool isPrintable(uint8_t b);

    QByteArray *buffer_{};    // not owned
    int         bytesPerRow_{16};
    bool        swapAscii16_{false};
    QSet<qint64> dirty_;
};
