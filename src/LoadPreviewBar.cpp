#include "LoadPreviewBar.h"

#include <QPainter>
#include <QPalette>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <utility>

LoadPreviewBar::LoadPreviewBar(QWidget *parent) : QWidget(parent) {
    setMinimumHeight(120);
}

void LoadPreviewBar::setParams(qulonglong bufSize, qulonglong off, qulonglong dataLen, qulonglong padLen) {
    bufSize_ = bufSize;
    off_ = off;
    dataLen_ = dataLen;
    padLen_ = padLen;
    update();
}

void LoadPreviewBar::setBufferSegments(QVector<QPair<qulonglong, qulonglong>> segments) {
    bufferSegments_ = std::move(segments);
    update();
}

QSize LoadPreviewBar::sizeHint() const {
    return QSize(420, 120);
}

void LoadPreviewBar::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QPalette pal = palette();
    const QColor windowColor = pal.color(QPalette::Window);
    const QColor baseColor = pal.color(QPalette::Base);
    const QColor alternateColor = pal.color(QPalette::AlternateBase);
    QColor frameColor = pal.color(QPalette::Mid);
    QColor textColor = pal.color(QPalette::WindowText);
    if (!frameColor.isValid()) frameColor = pal.color(QPalette::Dark);
    if (!frameColor.isValid()) frameColor = textColor;
    if (!textColor.isValid()) textColor = Qt::black;

    const bool darkTheme = windowColor.lightness() < textColor.lightness();

    auto adjustTone = [&](QColor color, int lighterFactor, int darkerFactor) {
        if (!color.isValid()) color = baseColor.isValid() ? baseColor : windowColor;
        color = darkTheme ? color.lighter(lighterFactor) : color.darker(darkerFactor);
        color.setAlpha(255);
        return color;
    };
    auto ensureContrast = [&](QColor color, const QColor &reference, int lighterFactor, int darkerFactor) {
        if (!color.isValid() || color == reference) {
            return adjustTone(reference, lighterFactor, darkerFactor);
        }
        return color;
    };

    const QColor emptyColor = adjustTone(baseColor.isValid() ? baseColor : windowColor, 108, 103);
    const QColor bufferColor = ensureContrast(alternateColor, emptyColor, 125, 115);
    const QColor dataColor = darkTheme ? QColor(90, 180, 130) : QColor(120, 200, 120);
    const QColor paddingColor = darkTheme ? QColor(190, 160, 80) : QColor(250, 220, 120);
    QColor overlapColor = darkTheme ? QColor(220, 110, 110) : QColor(220, 80, 80);
    overlapColor.setAlpha(255);
    frameColor = ensureContrast(frameColor, emptyColor, 140, 120);
    QColor tickColor = frameColor;

    const int W = width();
    const int barH = 16;
    const int topMargin = 28;
    const int y = topMargin; // bar top

    // Determine total span to visualize
    qulonglong total = bufSize_;
    qulonglong prePadLen = 0;
    if (off_ > bufSize_) prePadLen = off_ - bufSize_;
    const qulonglong newEnd = (off_ + dataLen_ + padLen_);
    if (newEnd > total) total = newEnd;
    if (total == 0) {
        p.fillRect(0, y, W, barH, emptyColor);
        p.setPen(frameColor);
        p.drawRect(0, y, W-1, barH);
        p.setPen(textColor);
        p.drawText(6, y+barH+16, tr("(empty)"));
        return;
    }

    auto xFor = [&](qulonglong v){ return int((double(v) / double(total)) * (W-2)) + 1; };

    // Background (gap/empty) light gray
    p.fillRect(0, y, W, barH, emptyColor);
    p.setPen(frameColor);
    p.drawRect(0, y, W-1, barH);

    // Existing buffer region [0, bufSize_)
    if (bufSize_ > 0) {
        int x0 = xFor(0), x1 = xFor(bufSize_);
        p.fillRect(x0, y, qMax(1, x1-x0), barH, bufferColor);
    }

    // Pre-padding from buffer end to offset (if any)
    if (prePadLen > 0) {
        int x0 = xFor(bufSize_);
        int x1 = xFor(off_);
        p.fillRect(x0, y, qMax(1, x1 - x0), barH, paddingColor);
    }

    // New data region [off_, off_+dataLen_)
    if (dataLen_ > 0) {
        int x0 = xFor(qMin(off_, total));
        int x1 = xFor(qMin(off_ + dataLen_, total));
        p.fillRect(x0, y, qMax(1, x1-x0), barH, dataColor);
    }

    bool hasOverlap = false;
    qulonglong ovStart = 0;
    qulonglong ovEnd = 0;

    // Overlap: portion of new data that overwrites existing buffer [0, bufSize_)
    if (dataLen_ > 0 && bufSize_ > 0) {
        const qulonglong dataStart = off_;
        const qulonglong dataEnd   = off_ + dataLen_;
        // True intersection of [dataStart, dataEnd) with [0, bufSize_)
        ovStart = std::max<qulonglong>(dataStart, 0);
        ovEnd   = std::min<qulonglong>(dataEnd,   bufSize_);
        if (ovEnd > ovStart) {
            int xr0 = xFor(ovStart);
            int xr1 = xFor(ovEnd);
            QColor red = overlapColor;
            red.setAlpha(180);
            p.fillRect(xr0, y, qMax(1, xr1 - xr0), barH, red);
            hasOverlap = true;
        }
    }

    // Padding region [off_+dataLen_, off_+dataLen_+padLen_)
    if (padLen_ > 0) {
        int x0 = xFor(qMin(off_ + dataLen_, total));
        int x1 = xFor(qMin(off_ + dataLen_ + padLen_, total));
        p.fillRect(x0, y, qMax(1, x1-x0), barH, paddingColor);
    }

    // Existing buffer segments markers (thin vertical lines at each start, except first)
    if (bufferSegments_.size() > 1) {
        const int lineTop = y + 1;
        const int lineBottom = y + barH - 2;
        QColor markerColor = frameColor;
        markerColor.setAlpha(180);
        QPen segmentPen(markerColor, 1);
        p.setPen(segmentPen);
        for (int i = 1; i < bufferSegments_.size(); ++i) {
            const auto &segment = bufferSegments_.at(i);
            qulonglong segStart = segment.first;
            if (segStart >= total) continue;
            int x = xFor(segStart);
            p.drawLine(x, lineTop, x, lineBottom);
        }
        p.setPen(frameColor);
    }

    // Address markers: numbers above the bar + legend below
    struct AddressMarker {
        qulonglong value = 0;
        int x = 0;
        QString text;
        bool top = true;
        int textX = 0;
    };
    const int edgeMargin = 2;
    const int tickLength = 7;
    QVector<AddressMarker> markers;
    const int leftEdge = 0;
    const int rightEdge = W - 1;

    auto addMarker = [&](qulonglong value) {
        if (total == 0) return;
        if (value > total) value = total;
        if (value > 0 && value == total) value = total - 1;
        if (qint64(value) < 0) value = 0;
        AddressMarker marker;
        marker.value = value;
        if (value == 0) {
            marker.x = leftEdge;
        } else if (value == total - 1) {
            marker.x = rightEdge;
        } else {
            marker.x = std::clamp(xFor(value), leftEdge, rightEdge);
        }
        marker.text = QStringLiteral("0x") + QString::number(value, 16).toUpper();
        markers.append(std::move(marker));
    };

    addMarker(0);
    if (bufSize_ > 0) addMarker(bufSize_ - 1);
    if (prePadLen > 0 && off_ > 0) addMarker(off_ - 1);
    if (dataLen_ > 0) {
        addMarker(off_);
        addMarker(off_ + dataLen_ - 1);
    }
    if (ovEnd > ovStart) {
        addMarker(ovStart);
        addMarker(ovEnd - 1);
    }
    if (padLen_ > 0) {
        addMarker(off_ + dataLen_);
        addMarker(off_ + dataLen_ + padLen_ - 1);
    }
    if (total > 0) addMarker(total - 1);

    std::sort(markers.begin(), markers.end(), [](const AddressMarker &a, const AddressMarker &b) {
        if (a.value == b.value) return a.x < b.x;
        return a.value < b.value;
    });
    QVector<AddressMarker> deduped;
    deduped.reserve(markers.size());
    int lastX = -1;
    qulonglong lastValue = std::numeric_limits<qulonglong>::max();
    for (const auto &marker : markers) {
        if (!deduped.isEmpty() && marker.value == lastValue) continue;
        if (!deduped.isEmpty() && std::abs(marker.x - lastX) <= 1) continue;
        deduped.append(marker);
        lastValue = marker.value;
        lastX = marker.x;
    }
    markers = deduped;

    QFont markerFont = p.font();
    markerFont.setBold(false);
    markerFont.setPointSizeF(markerFont.pointSizeF() - 1.5);
    p.setFont(markerFont);
    const QFontMetrics markerMetrics(markerFont);
    const int topBaseline = y - tickLength - 2;
    const int bottomTextTop = y + barH + tickLength + 2;
    const int bottomBaseline = bottomTextTop + markerMetrics.ascent();

    const int overlapGap = 2;
    int lastTopRight = edgeMargin - overlapGap - 3;
    int lastBottomRight = edgeMargin - overlapGap - 3;
    for (int i = 0; i < markers.size(); ++i) {
        const QString number = QString::number(i + 1);
        const int textWidth = markerMetrics.horizontalAdvance(number);
        const int anchor = markers[i].x;

        int topTextX = anchor - textWidth / 2;
        if (i == 0) {
            topTextX = std::max(edgeMargin, anchor - textWidth + 1);
        } else {
            if (topTextX < edgeMargin) topTextX = edgeMargin;
        }
        if (topTextX + textWidth > W - edgeMargin) topTextX = W - edgeMargin - textWidth;
        bool topOverlap = (topTextX <= lastTopRight + overlapGap);

        int bottomTextX = anchor - textWidth / 2;
        if (bottomTextX + textWidth > W - edgeMargin) bottomTextX = W - edgeMargin - textWidth;
        if (i == markers.size() - 1) {
            bottomTextX = std::min(W - edgeMargin - textWidth, anchor - 1);
        }
        if (bottomTextX < edgeMargin) bottomTextX = edgeMargin;
        bool bottomOverlap = (bottomTextX <= lastBottomRight + overlapGap);

        bool placeTop = true;
        if (topOverlap && !bottomOverlap) placeTop = false;
        else if (!topOverlap && bottomOverlap) placeTop = true;
        else if (topOverlap && bottomOverlap) {
            int spaceTop = topTextX - edgeMargin;
            int spaceBottom = (W - edgeMargin) - (bottomTextX + textWidth);
            placeTop = spaceTop >= spaceBottom;
        }

        markers[i].top = placeTop;
        if (placeTop) {
            if (topTextX <= lastTopRight + overlapGap) {
                topTextX = lastTopRight + overlapGap + 1;
                if (topTextX + textWidth > W - edgeMargin) {
                    topTextX = W - edgeMargin - textWidth;
                }
            }
            markers[i].textX = topTextX;
            lastTopRight = topTextX + textWidth;
        } else {
            if (bottomTextX <= lastBottomRight + overlapGap) {
                bottomTextX = lastBottomRight + overlapGap + 1;
                if (bottomTextX + textWidth > W - edgeMargin) {
                    bottomTextX = W - edgeMargin - textWidth;
                }
                if (bottomTextX < edgeMargin) bottomTextX = edgeMargin;
            }
            markers[i].textX = bottomTextX;
            lastBottomRight = bottomTextX + textWidth;
        }
    }

    p.setPen(tickColor);
    for (int i = 0; i < markers.size(); ++i) {
        const QString number = QString::number(i + 1);
        const int anchor = markers[i].x;
        if (markers[i].top) {
            p.drawLine(anchor, y, anchor, y - tickLength);
            p.setPen(textColor);
            p.drawText(markers[i].textX, topBaseline, number);
            p.setPen(tickColor);
        } else {
            p.drawLine(anchor, y + barH, anchor, y + barH + tickLength);
            p.setPen(textColor);
            p.drawText(markers[i].textX, bottomBaseline, number);
            p.setPen(tickColor);
        }
    }

    p.setFont(markerFont);
    const int markerBlockHeight = markerMetrics.height() + tickLength + 4;
    const int legendTop = y + barH + markerBlockHeight + 4;

    p.setBrush(Qt::NoBrush);
    QFont legendFont = p.font();
    legendFont.setBold(false);
    legendFont.setPointSizeF(legendFont.pointSizeF() - 1);
    p.setFont(legendFont);
    const QFontMetrics legendMetrics(legendFont);
    int addrLy = legendTop + legendMetrics.ascent();
    int addrLx = 4;
    const QString bufferLabel = tr("buffer");
    const QString dataLabel = tr("data");
    const QString paddingLabel = tr("padding");
    const QString overlapLabel = tr("overlap");
    const bool hasBufferSegment = bufSize_ > 0;
    bool hasDataSegment = false;
    if (dataLen_ > 0) {
        const qulonglong dataStart = off_;
        const qulonglong dataEnd = off_ + dataLen_;
        qulonglong overlapLen = 0;
        if (ovEnd > ovStart) {
            const qulonglong overlapStart = std::max(ovStart, dataStart);
            const qulonglong overlapEnd = std::min(ovEnd, dataEnd);
            if (overlapEnd > overlapStart) overlapLen = overlapEnd - overlapStart;
        }
        hasDataSegment = dataLen_ > overlapLen;
    }
    const bool hasPaddingSegment = (prePadLen > 0) || (padLen_ > 0);
    QStringList activeFields;
    if (hasBufferSegment) activeFields << bufferLabel;
    if (hasDataSegment) activeFields << dataLabel;
    if (hasPaddingSegment) activeFields << paddingLabel;
    if (hasOverlap) activeFields << overlapLabel;

    QFont legendBold = legendFont;
    legendBold.setBold(true);
    const QFontMetrics numberMetrics(legendBold);

    for (int i = 0; i < markers.size(); ++i) {
        const QString numberLabel = QString::number(i + 1);
        const QString addressText = markers[i].text;
        const QString suffix = QStringLiteral(": %1").arg(addressText);
        const int numberWidth = numberMetrics.horizontalAdvance(numberLabel);
        const int suffixWidth = legendMetrics.horizontalAdvance(suffix);

        p.setPen(textColor);
        p.setFont(legendBold);
        p.drawText(addrLx, addrLy, numberLabel);
        p.setFont(legendFont);
        p.drawText(addrLx + numberWidth, addrLy, suffix);

        addrLx += numberWidth + suffixWidth + 16;
    }

    int ly = addrLy + legendMetrics.height() + 8;
    auto legend = [&](QColor c, const QString &t, int &lx){
        p.fillRect(lx, ly-10, 10, 10, c);
        p.setPen(frameColor);
        p.drawRect(lx, ly-10, 10, 10);
        p.setPen(textColor);
        p.drawText(lx+14, ly, t);
        lx += 14 + p.fontMetrics().horizontalAdvance(t) + 12;
    };
    int lx = 4;
    for (const QString &label : activeFields) {
        if (label == bufferLabel) legend(bufferColor, label, lx);
        else if (label == dataLabel) legend(dataColor, label, lx);
        else if (label == paddingLabel) legend(paddingColor, label, lx);
        else if (label == overlapLabel) legend(overlapColor, label, lx);
    }
}
