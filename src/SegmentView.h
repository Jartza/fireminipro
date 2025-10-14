#pragma once

#include <QAbstractTableModel>
#include <QVector>

class SegmentView : public QAbstractTableModel {
    Q_OBJECT
public:
    struct Segment {
        qulonglong start{};
        qulonglong length{};
        QString    label;
        QString    note;
        qulonglong id{};
    };

    explicit SegmentView(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    Qt::DropActions supportedDropActions() const override;
    Qt::DropActions supportedDragActions() const override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationRow) override;

    void setSegments(QVector<Segment> segments);
    void clear();
    QVector<Segment> segments() const;

signals:
    void rowReordered(int from, int to);

private:
    QVector<Segment> rows_;

    static QString formatStart(qulonglong value);
    static QString formatEnd(qulonglong start, qulonglong length);
    static QString formatSize(qulonglong length);
    static QString formatLabel(const Segment &segment);
};
