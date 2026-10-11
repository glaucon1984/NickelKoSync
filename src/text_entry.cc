#include "text_entry.h"

#include "eink.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace kosync {
namespace {

int scale(int px) {
    const QScreen *screen = QApplication::primaryScreen();
    const int width = screen ? qMin(screen->size().width(), screen->size().height()) : 1072;
    return px * width / 1072;
}

const int columns = 10, rows = 4;
// Four rows of ten. Letters page; the symbol page below.
const char *const letterRows[rows] = {"1234567890", "qwertyuiop", "asdfghjkl'", "zxcvbnm,.-"};
const char *const symbolRows[rows] = {"@.-_/:~#%&", "+=?!;\"()[]", "{}<>*$^\\|`", "1234567890"};

const char *const dialogStyle =
    "QDialog { background: white; }"
    "QLabel { color: black; background: transparent; }"
    "QLineEdit { color: black; background: white; border: 3px solid black; border-radius: 8px; padding: 12px; }"
    "QPushButton { color: black; background: white; border: 3px solid black; border-radius: 10px; }"
    "QPushButton:pressed, QPushButton:checked { color: white; background: black; }"
    "QPushButton#action { background: black; color: white; }"
    "QPushButton#action:pressed { background: white; color: black; }";

}

TextEntryDialog::TextEntryDialog(const QString &title, const QString &initial, bool password, QWidget *parent)
    : QDialog(parent) {
    setObjectName("kosyncEntry");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setStyleSheet(dialogStyle);
    QFont base = font();
    base.setPixelSize(scale(28));
    setFont(base);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(scale(30), scale(40), scale(30), scale(30));
    outer->setSpacing(scale(20));

    QLabel *heading = new QLabel(title, this);
    QFont titleFont = base;
    titleFont.setPixelSize(scale(36));
    titleFont.setBold(true);
    heading->setFont(titleFont);
    outer->addWidget(heading);

    QHBoxLayout *field = new QHBoxLayout;
    field->setSpacing(scale(16));
    edit_ = new QLineEdit(initial, this);
    edit_->setMinimumHeight(scale(84));
    edit_->setFont(base);
    if (password)
        edit_->setEchoMode(QLineEdit::Password);
    field->addWidget(edit_, 1);
    if (password) {
        echo_ = new QPushButton("Show", this);
        echo_->setMinimumSize(scale(140), scale(84));
        connect(echo_, SIGNAL(clicked()), this, SLOT(toggleEcho()));
        field->addWidget(echo_);
    }
    outer->addLayout(field);
    outer->addStretch(1);

    QGridLayout *grid = new QGridLayout;
    grid->setSpacing(scale(12));
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            QPushButton *button = new QPushButton(this);
            button->setMinimumSize(scale(84), scale(90));
            button->setFocusPolicy(Qt::NoFocus);
            connect(button, SIGNAL(clicked()), this, SLOT(key()));
            grid->addWidget(button, row, column);
            keys_.append(button);
        }
    }
    outer->addLayout(grid);

    QHBoxLayout *controls = new QHBoxLayout;
    controls->setSpacing(scale(12));
    shift_ = new QPushButton("Shift", this);
    shift_->setCheckable(true);
    shift_->setMinimumSize(scale(160), scale(90));
    shift_->setFocusPolicy(Qt::NoFocus);
    connect(shift_, SIGNAL(clicked()), this, SLOT(shift()));
    symbols_ = new QPushButton("#+=", this);
    symbols_->setCheckable(true);
    symbols_->setMinimumSize(scale(160), scale(90));
    symbols_->setFocusPolicy(Qt::NoFocus);
    connect(symbols_, SIGNAL(clicked()), this, SLOT(symbols()));
    QPushButton *space = new QPushButton("Space", this);
    space->setMinimumHeight(scale(90));
    space->setFocusPolicy(Qt::NoFocus);
    space->setProperty("text_value", QString(" "));
    connect(space, SIGNAL(clicked()), this, SLOT(key()));
    QPushButton *back = new QPushButton(QString::fromUtf8("\xE2\x8C\xAB"), this);  // erase to the left
    back->setMinimumSize(scale(160), scale(90));
    back->setFocusPolicy(Qt::NoFocus);
    back->setAutoRepeat(true);
    connect(back, SIGNAL(clicked()), this, SLOT(backspace()));
    controls->addWidget(shift_);
    controls->addWidget(symbols_);
    controls->addWidget(space, 1);
    controls->addWidget(back);
    outer->addLayout(controls);

    QHBoxLayout *actions = new QHBoxLayout;
    actions->setSpacing(scale(24));
    QPushButton *cancel = new QPushButton("Cancel", this);
    cancel->setMinimumHeight(scale(90));
    cancel->setFocusPolicy(Qt::NoFocus);
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
    QPushButton *done = new QPushButton("Done", this);
    done->setObjectName("action");
    done->setMinimumHeight(scale(90));
    done->setFocusPolicy(Qt::NoFocus);
    connect(done, SIGNAL(clicked()), this, SLOT(accept()));
    actions->addWidget(cancel);
    actions->addWidget(done);
    outer->addLayout(actions);

    relabel();
    if (const QScreen *screen = QApplication::primaryScreen())
        setGeometry(screen->geometry());
    eink::markControls(this);
    edit_->setFocus();
    edit_->end(false);
}

void TextEntryDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    eink::markFlash(this);
    QTimer::singleShot(800, this, SLOT(settle()));
}

void TextEntryDialog::settle() {
    eink::markControl(this);
}

QString TextEntryDialog::value() const {
    return edit_->text();
}

void TextEntryDialog::relabel() {
    const char *const *page = symbolPage_ ? symbolRows : letterRows;
    for (int row = 0; row < rows; ++row) {
        const QString line = QString::fromUtf8(page[row]);
        for (int column = 0; column < columns; ++column) {
            QString text = column < line.size() ? QString(line[column]) : QString();
            if (upper_ && !symbolPage_)
                text = text.toUpper();
            QPushButton *button = keys_[row * columns + column];
            button->setProperty("text_value", text);
            // Qt reads a lone ampersand as a shortcut marker.
            button->setText(text == "&" ? "&&" : text);
            button->setEnabled(!text.isEmpty());
        }
    }
}

void TextEntryDialog::key() {
    QObject *button = sender();
    if (!button)
        return;
    const QString text = button->property("text_value").toString();
    if (text.isEmpty())
        return;
    edit_->insert(text);
    edit_->setFocus();
    if (upper_ && !symbolPage_ && text != " ") {
        // Shift applies to one letter, like a phone keyboard.
        upper_ = false;
        shift_->setChecked(false);
        relabel();
    }
}

void TextEntryDialog::shift() {
    upper_ = shift_->isChecked();
    relabel();
}

void TextEntryDialog::symbols() {
    symbolPage_ = symbols_->isChecked();
    relabel();
}

void TextEntryDialog::backspace() {
    edit_->backspace();
    edit_->setFocus();
}

void TextEntryDialog::toggleEcho() {
    const bool hidden = edit_->echoMode() == QLineEdit::Password;
    edit_->setEchoMode(hidden ? QLineEdit::Normal : QLineEdit::Password);
    echo_->setText(hidden ? "Hide" : "Show");
    edit_->setFocus();
}

}
