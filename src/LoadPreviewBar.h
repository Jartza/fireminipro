#pragma once

#include <QWidget>

// Visualizes how a file will be merged into the current buffer.
class LoadPreviewBar : public QWidget {
public:
    explicit LoadPreviewBar(QWidget *parent = nullptr);

    void setParams(qulonglong bufSize, qulonglong off, qulonglong dataLen, qulonglong padLen);

protected:
    QSize sizeHint() const override;
    void paintEvent(QPaintEvent *event) override;

private:
    qulonglong bufSize_{};
    qulonglong off_{};
    qulonglong dataLen_{};
    qulonglong padLen_{};
};
