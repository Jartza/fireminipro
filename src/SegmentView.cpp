#include "SegmentView.h"

#include <QDataStream>
#include <QMimeData>
#include <QIODevice>

#include <utility>

namespace {
constexpr auto kMimeType = "application/x-fireminipro-segment-row";
}

SegmentView::SegmentView(QObject *parent)
    : QAbstractTableModel(parent) {}

int SegmentView::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return rows_.size();
}

int SegmentView::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return 4;
}

QVariant SegmentView::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    if (index.row() < 0 || index.row() >= rows_.size()) return {};

    const auto &segment = rows_.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case 0: return formatStart(segment.start);
        case 1: return formatEnd(segment.start, segment.length);
        case 2: return formatSize(segment.length);
        case 3: return formatLabel(segment);
        default: return {};
        }
    case Qt::TextAlignmentRole:
        if (index.column() < 3) return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    default:
        return {};
    }
}

QVariant SegmentView::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return tr("Start");
        case 1: return tr("End");
        case 2: return tr("Size");
        case 3: return tr("File");
        default: break;
        }
    }
    return QAbstractTableModel::headerData(section, orientation, role);
}

Qt::ItemFlags SegmentView::flags(const QModelIndex &index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.isValid())
        f |= Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    else
        f |= Qt::ItemIsDropEnabled;
    return f;
}

QStringList SegmentView::mimeTypes() const {
    return {QString::fromLatin1(kMimeType)};
}

QMimeData *SegmentView::mimeData(const QModelIndexList &indexes) const {
    if (indexes.isEmpty()) return nullptr;
    const int row = indexes.first().row();
    if (row < 0 || row >= rows_.size()) return nullptr;

    auto *mime = new QMimeData;
    QByteArray encoded;
    QDataStream out(&encoded, QIODevice::WriteOnly);
    out << row;
    mime->setData(kMimeType, encoded);
    return mime;
}

bool SegmentView::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                               const QModelIndex &parent) {
    Q_UNUSED(column);
    if (action == Qt::IgnoreAction) return true;
    if (!data || !data->hasFormat(kMimeType)) return false;

    QByteArray encoded = data->data(kMimeType);
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    int sourceRow = -1;
    stream >> sourceRow;
    if (sourceRow < 0 || sourceRow >= rows_.size()) return false;

    int destinationRow = row;
    if (destinationRow == -1) {
        destinationRow = parent.isValid() ? parent.row() : rows_.size();
    }

    if (destinationRow > sourceRow) destinationRow -= 1;
    if (destinationRow == sourceRow) return false;

    return moveRows(QModelIndex(), sourceRow, 1, QModelIndex(), destinationRow);
}

Qt::DropActions SegmentView::supportedDropActions() const {
    return Qt::MoveAction;
}

Qt::DropActions SegmentView::supportedDragActions() const {
    return Qt::MoveAction;
}

bool SegmentView::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                           const QModelIndex &destinationParent, int destinationRow) {
    if (sourceParent.isValid() || destinationParent.isValid()) return false;
    if (count <= 0 || count > 1) return false;
    if (sourceRow < 0 || sourceRow + count > rows_.size()) return false;
    if (destinationRow < 0 || destinationRow > rows_.size()) return false;
    if (destinationRow >= sourceRow && destinationRow <= sourceRow + count) return false;

    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow + count - 1,
                       QModelIndex(), destinationRow)) {
        return false;
    }

    QVector<Segment> moved;
    moved.reserve(count);
    for (int i = 0; i < count; ++i) moved.append(rows_.at(sourceRow + i));
    for (int i = 0; i < count; ++i) rows_.removeAt(sourceRow);

    int insertRow = destinationRow;
    if (destinationRow > sourceRow) insertRow -= count;
    for (int i = 0; i < count; ++i) rows_.insert(insertRow + i, moved.at(i));

    endMoveRows();
    emit rowReordered(sourceRow, insertRow);
    return true;
}

void SegmentView::setSegments(QVector<Segment> segments) {
    beginResetModel();
    rows_ = std::move(segments);
    endResetModel();
}

void SegmentView::clear() {
    setSegments({});
}

QVector<SegmentView::Segment> SegmentView::segments() const {
    return rows_;
}

QString SegmentView::formatStart(qulonglong value) {
    return QStringLiteral("0x%1").arg(QString::number(value, 16).toUpper());
}

QString SegmentView::formatEnd(qulonglong start, qulonglong length) {
    const qulonglong end = length ? (start + length - 1) : start;
    return QStringLiteral("0x%1").arg(QString::number(end, 16).toUpper());
}

QString SegmentView::formatSize(qulonglong length) {
    return QStringLiteral("%1 (0x%2)")
        .arg(QString::number(length),
             QString::number(length, 16).toUpper());
}

QString SegmentView::formatLabel(const Segment &segment) {
    QString label = segment.label;
    if (!segment.note.isEmpty()) label += segment.note;
    return label;
}
