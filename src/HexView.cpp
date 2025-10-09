#include "HexView.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QVariant>

HexView::HexView(QObject *parent) : QAbstractTableModel(parent) {}

void HexView::setBufferRef(QByteArray *buffer) {
    beginResetModel();
    buffer_ = buffer;
    dirty_.clear(); // reset dirty tracking when buffer changes
    endResetModel();
}

void HexView::clear() {
    beginResetModel();
    buffer_ = nullptr;
    dirty_.clear();
    endResetModel();
}

void HexView::setBytesPerRow(int n) {
    if (n < 1) n = 1;
    if (bytesPerRow_ == n) return;
    beginResetModel();
    bytesPerRow_ = n;
    endResetModel();
}

void HexView::setSwapAscii16(bool on) {
    if (swapAscii16_ == on) return;
    swapAscii16_ = on;
    if (rowCount() > 0 && columnCount() > 0) {
        emit headerDataChanged(Qt::Horizontal, 0, columnCount()-1);
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));
    }
}

int HexView::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !buffer_) return 0;
    const auto n = buffer_->size();
    return (n + bytesPerRow_ - 1) / bytesPerRow_;
}

int HexView::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    // 0 = address, 1..bytesPerRow_ = hex bytes, last = ASCII
    return 1 + bytesPerRow_ + 1;
}

QVariant HexView::headerData(int section, Qt::Orientation o, int role) const {
    if (o != Qt::Horizontal || role != Qt::DisplayRole) return {};
    if (section == 0) return QStringLiteral("Addr");
    if (section == 1 + bytesPerRow_) {
        return swapAscii16_ ? QStringLiteral("ASCII (swapped)") : QStringLiteral("ASCII");
    }
    if (section >= 1 && section <= bytesPerRow_) {
        return QString("%1").arg(section-1, 2, 16, QLatin1Char('0')).toUpper();
    }
    return {};
}

static inline bool bytePrintable(uint8_t b) {
    return b >= 32 && b <= 126;
}

QVariant HexView::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid() || !buffer_) return {};
    const int r = idx.row();
    const int c = idx.column();

    const qint64 rowBase = qint64(r) * bytesPerRow_;

    // Background tint for dirty bytes (hex columns) or for ascii row if any byte dirty
    if (role == Qt::BackgroundRole) {
        if (c >= 1 && c <= bytesPerRow_) {
            const qint64 off = rowBase + (c - 1);
            if (off < buffer_->size() && isDirty(off)) return QBrush(QColor(255,245,200));
        } else if (c == 1 + bytesPerRow_) {
            for (int i=0; i<bytesPerRow_; ++i) {
                const qint64 off = rowBase + i;
                if (off < buffer_->size() && isDirty(off)) return QBrush(QColor(255,245,200));
            }
        }
    }

    if (role == Qt::TextAlignmentRole) {
        if (c == 0) return int(Qt::AlignRight | Qt::AlignVCenter);
        if (c >= 1 && c <= bytesPerRow_) return int(Qt::AlignHCenter | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role == Qt::DisplayRole) {
        // address
        if (c == 0) {
            return QString("%1").arg(rowBase, 8, 16, QLatin1Char('0')).toUpper();
        }

        // hex bytes
        if (c >= 1 && c <= bytesPerRow_) {
            const qint64 off = rowBase + (c - 1);
            if (off >= buffer_->size()) return QString("  ");
            const uint8_t b = uint8_t(buffer_->at(int(off)));
            return QString("%1").arg(b, 2, 16, QLatin1Char('0')).toUpper();
        }

        // ascii column
        if (c == 1 + bytesPerRow_) {
            QString s; s.reserve(bytesPerRow_);
            for (int i=0; i<bytesPerRow_; ++i) {
                const qint64 off = rowBase + i;
                if (off >= buffer_->size()) { s.append(' '); continue; }
                const uint8_t b = uint8_t(buffer_->at(int(off)));
                uint8_t ch = b;
                if (swapAscii16_) {
                    // swap each pair within the row region
                    const int iPair = (i ^ 1);
                    const qint64 other = rowBase + iPair;
                    if (other < buffer_->size()) ch = uint8_t(buffer_->at(int(other)));
                }
                s.append(bytePrintable(ch) ? QChar(ch) : QChar('.'));
            }
            return s;
        }
    }

    return {};
}

Qt::ItemFlags HexView::flags(const QModelIndex &idx) const {
    if (!idx.isValid()) return Qt::NoItemFlags;
    const int c = idx.column();
    // editable hex bytes only (not address or ascii)
    if (c >= 1 && c <= bytesPerRow_) return Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

bool HexView::setData(const QModelIndex &idx, const QVariant &val, int role) {
    if (role != Qt::EditRole || !buffer_) return false;
    const int r = idx.row();
    const int c = idx.column();
    if (c < 1 || c > bytesPerRow_) return false;

    const qint64 off = qint64(r) * bytesPerRow_ + (c - 1);
    if (off < 0 || off >= buffer_->size()) return false;

    // parse two-hex-digit string
    bool ok = false;
    QString t = val.toString().trimmed();
    if (t.startsWith("0x", Qt::CaseInsensitive)) t = t.mid(2);
    const int b = t.toInt(&ok, 16);
    if (!ok || b < 0 || b > 255) return false;

    char &ref = (*buffer_)[int(off)];
    if (ref == char(b)) return false;
    ref = char(b);
    dirty_.insert(off);
    emit dataChanged(index(r, 0), index(r, columnCount()-1));
    return true;
}

void HexView::clearDirty() { dirty_.clear(); }
bool HexView::isDirty(qint64 off) const { return dirty_.contains(off); }
int  HexView::dirtyCount() const { return dirty_.size(); }
