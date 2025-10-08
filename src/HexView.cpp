#include "HexView.h"
#include <QFont>
#include <QString>
#include <algorithm>

HexView::HexView(QObject *parent) : QAbstractTableModel(parent) {}

void HexView::setBufferRef(QByteArray *buffer) {
    beginResetModel();
    buf_ = buffer;
    endResetModel();
}

void HexView::clear() {
    beginResetModel();
    buf_ = nullptr;
    endResetModel();
}

void HexView::setBytesPerRow(int n) {
    if (n <= 0) return;
    if (bytesPerRow_ == n) return;
    beginResetModel();
    bytesPerRow_ = n;
    endResetModel();
}

void HexView::setSwapAscii16(bool on) {
    if (asciiSwap16_ == on) return;
    asciiSwap16_ = on;
    // Only ASCII column changes, but simplest is notify whole model
    if (rowCount() > 0) {
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));
    }
}

int HexView::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !buf_) return 0;
    const auto n = buf_->size();
    return (n + bytesPerRow_ - 1) / bytesPerRow_;
}

int HexView::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    // Address + N hex bytes + ASCII
    return 1 + bytesPerRow_ + 1;
}

QVariant HexView::headerData(int section, Qt::Orientation o, int role) const {
    if (role == Qt::FontRole) {
        QFont f; f.setFamily("Courier New"); f.setStyleHint(QFont::Monospace);
        return f;
    }
    if (o == Qt::Horizontal && role == Qt::DisplayRole) {
        if (section == 0) return QStringLiteral("Address");
        if (section == 1 + bytesPerRow_) return QStringLiteral("ASCII");
        return QString("%1").arg(section - 1, 2, 16, QLatin1Char('0')).toUpper();
    }
    if (o == Qt::Vertical && role == Qt::DisplayRole) {
        return QVariant(); // no row headers (or could show row offset / 0x)
    }
    return {};
}

static QString byteToHex(uint8_t b) {
    static const char* hex = "0123456789ABCDEF";
    QString s; s.resize(2);
    s[0] = QChar(hex[(b >> 4) & 0xF]);
    s[1] = QChar(hex[b & 0xF]);
    return s;
}

bool HexView::isPrintable(uint8_t b) {
    return b >= 0x20 && b < 0x7F;
}

QVariant HexView::data(const QModelIndex &idx, int role) const {
    if (!idx.isValid() || !buf_) return {};

    const int col = idx.column();
    const int row = idx.row();

    // monospace + centered hex cells
    if (role == Qt::FontRole) {
        QFont f; f.setFamily("Courier New"); f.setStyleHint(QFont::Monospace);
        return f;
    }
    if (role == Qt::TextAlignmentRole) {
        return (col == 0 || col == 1 + bytesPerRow_) ? QVariant(Qt::AlignLeft | Qt::AlignVCenter)
                                                     : QVariant(Qt::AlignCenter);
    }

    if (role != Qt::DisplayRole && role != Qt::EditRole) return {};

    const qint64 rowBase = static_cast<qint64>(row) * bytesPerRow_;

    // Address column
    if (col == 0) {
        return QString("%1").arg(rowBase, 8, 16, QLatin1Char('0')).toUpper();
    }

    // ASCII column
    if (col == 1 + bytesPerRow_) {
        QByteArray ascii;
        ascii.reserve(bytesPerRow_);
        // collect row bytes
        QByteArray slice;
        for (int i = 0; i < bytesPerRow_; ++i) {
            const qint64 off = rowBase + i;
            if (inRange(off)) slice.append((*buf_)[off]);
            else slice.append('\0');
        }
        if (asciiSwap16_) {
            // swap within each 16-bit word
            for (int i = 0; i + 1 < slice.size(); i += 2)
                std::swap(slice[i], slice[i+1]);
        }
        for (char c : slice) {
            uint8_t b = static_cast<uint8_t>(c);
            ascii.append(isPrintable(b) ? c : '.');
        }
        return QString::fromLatin1(ascii);
    }

    // Hex data columns
    const int byteIndexInRow = col - 1; // hex columns start at 1
    const qint64 off = rowBase + byteIndexInRow;
    if (!inRange(off)) return QString("  ");

    const uint8_t b = static_cast<uint8_t>((*buf_)[off]);
    // edit role should show the raw 2-digit hex
    if (role == Qt::EditRole)
        return QString("%1").arg(b, 2, 16, QLatin1Char('0')).toUpper();

    // display role
    return byteToHex(b);
}

Qt::ItemFlags HexView::flags(const QModelIndex &idx) const {
    if (!idx.isValid()) return Qt::NoItemFlags;
    auto f = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // only hex byte cells are editable (not address or ASCII)
    if (idx.column() > 0 && idx.column() < 1 + bytesPerRow_ && buf_) {
        f |= Qt::ItemIsEditable;
    }
    return f;
}

bool HexView::setData(const QModelIndex &idx, const QVariant &val, int role) {
    if (role != Qt::EditRole || !idx.isValid() || !buf_) return false;
    const int col = idx.column();
    if (col <= 0 || col >= 1 + bytesPerRow_) return false; // don't edit address/ascii

    bool ok = false;
    const QString s = val.toString().trimmed();
    if (s.size() == 0) return false;

    // accept "AA" or "0xAA"
    QString t = s.startsWith("0x", Qt::CaseInsensitive) ? s.mid(2) : s;
    if (t.size() > 2) return false;
    const uint v = t.toUInt(&ok, 16);
    if (!ok || v > 0xFF) return false;

    const int row = idx.row();
    const int byteIndexInRow = col - 1;
    const qint64 off = static_cast<qint64>(row) * bytesPerRow_ + byteIndexInRow;
    if (!inRange(off)) return false;

    (*buf_)[off] = static_cast<char>(v);

    // notify this cell + ASCII column for the row
    emit dataChanged(idx, idx);
    const int asciiCol = 1 + bytesPerRow_;
    emit dataChanged(index(row, asciiCol), index(row, asciiCol));
    return true;
}

void HexView::setBufferRef(QByteArray *buffer) {
    beginResetModel();
    buf_ = buffer;
    dirty_.resize(buf_ ? buf_->size() : 0);
    dirty_.fill(false);
    endResetModel();
}

void HexView::clearDirty() {
    if (!buf_ || dirty_.count(true) == 0) return;
    dirty_.fill(false);
    if (rowCount() > 0)
        emit dataChanged(index(0,0), index(rowCount()-1, columnCount()-1));
}

bool HexView::isDirty(qint64 off) const {
    return buf_ && off >= 0 && off < dirty_.size() && dirty_.testBit(int(off));
}

int HexView::dirtyCount() const { 
    return dirty_.count(true);
}
