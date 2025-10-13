#include "LoadPreviewBar.h"

#include <QPainter>
#include <algorithm>

LoadPreviewBar::LoadPreviewBar(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(88);
}

void LoadPreviewBar::setParams(qulonglong bufSize, qulonglong off, qulonglong dataLen, qulonglong padLen) {
    bufSize_ = bufSize;
    off_ = off;
    dataLen_ = dataLen;
    padLen_ = padLen;
    update();
}

QSize LoadPreviewBar::sizeHint() const {
    return QSize(420, 92);
}

void LoadPreviewBar::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const int W = width();
    const int barH = 16;
    const int y = 8; // top margin

    // Determine total span to visualize
    qulonglong total = bufSize_;
    qulonglong prePadLen = 0;
    if (off_ > bufSize_) prePadLen = off_ - bufSize_;
    const qulonglong newEnd = (off_ + dataLen_ + padLen_);
    if (newEnd > total) total = newEnd;
    if (total == 0) {
        p.fillRect(0, y, W, barH, QColor(230,230,230));
        p.setPen(Qt::gray);
        p.drawRect(0, y, W-1, barH);
        p.drawText(6, y+barH+16, "(empty)");
        return;
    }

    auto xFor = [&](qulonglong v){ return int((double(v) / double(total)) * (W-2)) + 1; };

    // Background (gap/empty) light gray
    p.fillRect(0, y, W, barH, QColor(235,235,235));
    p.setPen(QColor(180,180,180));
    p.drawRect(0, y, W-1, barH);

    // Existing buffer region [0, bufSize_)
    if (bufSize_ > 0) {
        int x0 = xFor(0), x1 = xFor(bufSize_);
        p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(180,180,180)); // darker gray
    }

    // Pre-padding from buffer end to offset (if any)
    if (prePadLen > 0) {
        int x0 = xFor(bufSize_);
        int x1 = xFor(off_);
        p.fillRect(x0, y, qMax(1, x1 - x0), barH, QColor(250, 220, 120)); // yellow
    }

    // New data region [off_, off_+dataLen_)
    if (dataLen_ > 0) {
        int x0 = xFor(qMin(off_, total));
        int x1 = xFor(qMin(off_ + dataLen_, total));
        p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(120, 200, 120)); // green
    }

    bool hasOverlap = false;

    // Overlap: portion of new data that overwrites existing buffer [0, bufSize_)
    if (dataLen_ > 0 && bufSize_ > 0) {
        const qulonglong dataStart = off_;
        const qulonglong dataEnd   = off_ + dataLen_;
        // True intersection of [dataStart, dataEnd) with [0, bufSize_)
        const qulonglong ovStart = std::max<qulonglong>(dataStart, 0);
        const qulonglong ovEnd   = std::min<qulonglong>(dataEnd,   bufSize_);
        if (ovEnd > ovStart) {
            int xr0 = xFor(ovStart);
            int xr1 = xFor(ovEnd);
            QColor red(220, 80, 80, 180); // semi-transparent red overlay
            p.fillRect(xr0, y, qMax(1, xr1 - xr0), barH, red);
            hasOverlap = true;
        }
    }

    // Padding region [off_+dataLen_, off_+dataLen_+padLen_)
    if (padLen_ > 0) {
        int x0 = xFor(qMin(off_ + dataLen_, total));
        int x1 = xFor(qMin(off_ + dataLen_ + padLen_, total));
        p.fillRect(x0, y, qMax(1, x1-x0), barH, QColor(250, 220, 120)); // yellow
    }

    // Simple tick labels
    p.setPen(Qt::black);
    QFont f = p.font(); f.setPointSizeF(f.pointSizeF()-1); p.setFont(f);
    const QString startTxt = QString("0x") + QString::number(0,16).toUpper();
    const QString offTxt   = QString("0x") + QString::number(off_,16).toUpper();
    const QString endTxt   = QString("0x") + QString::number(total ? total-1 : 0,16).toUpper();
    p.drawText(4, y+barH+14, startTxt);
    // draw off only if meaningful and inside range
    if (off_ > 0 && off_ < total) {
        int xo = xFor(off_);
        p.setPen(QColor(80,80,80));
        p.drawLine(xo, y+barH, xo, y+barH+4);
        p.setPen(Qt::black);
        p.drawText(qMin(qMax(4, xo-40), W-60), y+barH+14, offTxt);
    }
    p.drawText(W-4 - p.fontMetrics().horizontalAdvance(endTxt), y+barH+14, endTxt);

    int ly = y + barH + 30;
    auto legend = [&](QColor c, const QString &t, int &lx){
        p.fillRect(lx, ly-10, 10, 10, c);
        p.setPen(Qt::black);
        p.drawRect(lx, ly-10, 10, 10);
        p.drawText(lx+14, ly, t);
        lx += 14 + p.fontMetrics().horizontalAdvance(t) + 12;
    };
    const bool hasBuffer  = (bufSize_ > 0);
    // dataLen_ is the file portion (green) that will be written
    const bool hasData    = (dataLen_ > 0);
    const bool hasPadding = (prePadLen > 0) || (padLen_ > 0);
    int lx = 4;
    if (hasBuffer)  legend(QColor(180,180,180), tr("buffer"),  lx);
    if (hasData)    legend(QColor(120,200,120), tr("data"),    lx);
    if (hasPadding) legend(QColor(250,220,120), tr("padding"), lx);
    if (hasOverlap) legend(QColor(220, 80, 80), tr("overlap"), lx);
}
