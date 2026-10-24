#pragma once

#include <QTableView>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QTimer>
#include <QKeyEvent>

class HexTableView : public QTableView {
    Q_OBJECT
  public:
    explicit HexTableView(QWidget *parent = nullptr) : QTableView(parent) {}

  protected:
    bool edit(const QModelIndex &index, EditTrigger trigger, QEvent *event) override {
        bool result = QTableView::edit(index, trigger, event);

        if (result) {
            // Use a single-shot timer to customize the editor after it's created
            QTimer::singleShot(0, this, [this, index]() {
                if (QWidget *editor = this->indexWidget(index)) {
                    if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(editor)) {
                        // CRITICAL: Use the exact font from the view, including size
                        QFont displayFont;

                        // First, check if the model provides a font
                        QVariant fontVariant = this->model()->data(index, Qt::FontRole);
                        if (fontVariant.isValid() && fontVariant.canConvert<QFont>()) {
                            displayFont = fontVariant.value<QFont>();
                        } else {
                            // Use the view's font as fallback
                            displayFont = this->font();
                        }

                        // Apply to editor
                        lineEdit->setFont(displayFont);
                        // Center alignment to match display
                        lineEdit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

                        // Hex values are max 2 characters
                        lineEdit->setMaxLength(2);

                        // Restrict to hex characters only
                        lineEdit->setValidator(new QRegularExpressionValidator(
                            QRegularExpression("^[0-9A-Fa-f]{0,2}$"), lineEdit));

                        // Remove any extra margins and padding
                        lineEdit->setContentsMargins(0, 0, 0, 0);
                        QMargins margins = lineEdit->textMargins();
                        margins.setLeft(0);
                        margins.setRight(0);
                        lineEdit->setTextMargins(margins);

                        // Set stylesheet to remove any default padding
                        lineEdit->setStyleSheet("QLineEdit { padding: 0px; margin: 0px; }");

                        // Place cursor at the end instead of selecting all
                        lineEdit->setCursorPosition(lineEdit->text().length());

                        // Install event filter to handle subsequent key presses
                        lineEdit->installEventFilter(this);
                    }
                }
            });
        }

        return result;
    }

    bool eventFilter(QObject *obj, QEvent *event) override {
        QLineEdit *editor = qobject_cast<QLineEdit*>(obj);
        if (editor && event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

            // If text is selected and user types a character
            if (editor->hasSelectedText() &&
                 !keyEvent->text().isEmpty() &&
                 keyEvent->text().at(0).isPrint()) {

                // Clear selection first, then let the character be inserted
                editor->deselect();

                // Move cursor to end
                editor->setCursorPosition(editor->text().length());
            }
        }
        return QTableView::eventFilter(obj, event);
    }
};
