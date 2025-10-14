#pragma once

#include <QTableView>
#include <QUrl>

class SegmentTableView : public QTableView {
    Q_OBJECT
public:
    explicit SegmentTableView(QWidget *parent = nullptr);

signals:
    void externalFilesDropped(int row, const QList<QUrl> &urls);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    static constexpr const char *kSegmentMime = "application/x-fireminipro-segment-row";
    bool isInternalDrag(const QMimeData *mime) const;
};
