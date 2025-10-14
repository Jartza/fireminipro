#include "SegmentTableView.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

SegmentTableView::SegmentTableView(QWidget *parent)
    : QTableView(parent) {
    setAcceptDrops(true);
}

void SegmentTableView::dragEnterEvent(QDragEnterEvent *event) {
    if (isInternalDrag(event->mimeData())) {
        QTableView::dragEnterEvent(event);
        return;
    }

    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QTableView::dragEnterEvent(event);
}

void SegmentTableView::dragMoveEvent(QDragMoveEvent *event) {
    if (isInternalDrag(event->mimeData())) {
        QTableView::dragMoveEvent(event);
        return;
    }

    if (event->mimeData() && event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QTableView::dragMoveEvent(event);
}

void SegmentTableView::dropEvent(QDropEvent *event) {
    if (isInternalDrag(event->mimeData())) {
        QTableView::dropEvent(event);
        return;
    }

    if (!event->mimeData() || !event->mimeData()->hasUrls()) {
        QTableView::dropEvent(event);
        return;
    }

    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty()) {
        event->ignore();
        return;
    }

    int targetRow = model() ? model()->rowCount() : 0;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QPoint pos = event->position().toPoint();
#else
    const QPoint pos = event->pos();
#endif
    const QModelIndex idx = indexAt(pos);
    if (idx.isValid()) {
        const QRect rect = visualRect(idx);
        if (pos.y() < rect.center().y())
            targetRow = idx.row();
        else
            targetRow = idx.row() + 1;
    }

    emit externalFilesDropped(targetRow, urls);
    event->acceptProposedAction();
}

bool SegmentTableView::isInternalDrag(const QMimeData *mime) const {
    return mime && mime->hasFormat(kSegmentMime);
}
