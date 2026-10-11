#include "settings_dialog.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>

namespace kosync {
namespace {

int scale(int px) {
    // Sized for the Clara's 1072 px width; scaled for other screens.
    const QScreen *screen = QApplication::primaryScreen();
    const int width = screen ? qMin(screen->size().width(), screen->size().height()) : 1072;
    return px * width / 1072;
}

const char *const dialogStyle =
    "QDialog { background: white; }"
    "QLabel { color: black; background: transparent; }"
    "QPushButton { color: black; background: white; border: 3px solid black; border-radius: 12px;"
    "  padding: 18px 24px; text-align: left; }"
    "QPushButton:pressed { color: white; background: black; }"
    "QPushButton#action { background: black; color: white; text-align: center; }"
    "QPushButton#action:pressed { background: white; color: black; }"
    "QScrollArea { border: none; background: white; }";

QStringList list(const char *a, const char *b, const char *c = 0) {
    QStringList values;
    values << QString::fromUtf8(a) << QString::fromUtf8(b);
    if (c)
        values << QString::fromUtf8(c);
    return values;
}

}

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent) : QDialog(parent) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setStyleSheet(dialogStyle);
    QFont base = font();
    base.setPixelSize(scale(34));
    setFont(base);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(scale(40), scale(40), scale(40), scale(40));
    outer->setSpacing(scale(24));

    QLabel *title = new QLabel("KOSync", this);
    QFont titleFont = base;
    titleFont.setPixelSize(scale(52));
    titleFont.setBold(true);
    title->setFont(titleFont);
    outer->addWidget(title);

    QHBoxLayout *actions = new QHBoxLayout;
    actions->setSpacing(scale(24));
    QPushButton *syncNow = new QPushButton("Sync now", this);
    syncNow->setObjectName("action");
    syncNow->setMinimumHeight(scale(110));
    connect(syncNow, &QPushButton::clicked, this, [this] {
        if (onSync)
            onSync(false);
    });
    QPushButton *syncAll = new QPushButton("Check whole library", this);
    syncAll->setObjectName("action");
    syncAll->setMinimumHeight(scale(110));
    connect(syncAll, &QPushButton::clicked, this, [this] {
        if (onSync)
            onSync(true);
    });
    actions->addWidget(syncNow);
    actions->addWidget(syncAll);
    outer->addLayout(actions);

    status_ = new QLabel(this);
    status_->setWordWrap(true);
    QFont small = base;
    small.setPixelSize(scale(28));
    status_->setFont(small);
    outer->addWidget(status_);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *inner = new QWidget(scroll);
    QVBoxLayout *rows = new QVBoxLayout(inner);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(scale(18));
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);

    addOption("sync", "pull", "Positions from other devices", list("newer", "forward", "off"),
              list("newest wins", "only when further ahead", "never move"), int(config.pull));
    addOption("sync", "push", "Send this device's progress", list("true", "false"), list("yes", "no"),
              config.push ? 0 : 1);
    addOption("sync", "on_open", "Check when a book is opened or the device wakes", list("true", "false"),
              list("yes", "no"), config.onOpen ? 0 : 1);
    addOption("wifi", "mode", "Wi-Fi", list("connect", "keep"), list("connect when needed", "leave alone"),
              config.connectWifi ? 0 : 1);
    addOption("sync", "document_id", "Match books by", list("binary", "filename"), list("file content", "file name"),
              int(config.documentId));
    addOption("options", "notify", "Notifications", list("true", "false"), list("on", "off"), config.notify ? 0 : 1);
    addOption("options", "menu_log", "Log entry in the menu", list("true", "false"), list("yes", "no"),
              config.menuLog ? 0 : 1);
    addOption("options", "verbose", "Detailed log", list("true", "false"), list("on", "off"), config.verbose ? 0 : 1);
    for (int i = 0; i < options_.size(); ++i)
        rows->addWidget(options_[i].button);
    rows->addStretch(1);

    QPushButton *close = new QPushButton("Close", this);
    close->setObjectName("action");
    close->setMinimumHeight(scale(110));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    outer->addWidget(close);

    if (const QScreen *screen = QApplication::primaryScreen())
        setGeometry(screen->geometry());
}

void SettingsDialog::addOption(const char *section, const char *key, const char *label, const QStringList &values,
                               const QStringList &names, int index) {
    Option option;
    option.section = section;
    option.key = key;
    option.label = label;
    option.values = values;
    option.names = names;
    option.index = qBound(0, index, values.size() - 1);
    option.button = new QPushButton(this);
    option.button->setMinimumHeight(scale(120));
    option.button->setProperty("option", options_.size());
    option.button->setText(text(option));
    connect(option.button, SIGNAL(clicked()), this, SLOT(cycle()));
    options_.append(option);
}

QString SettingsDialog::text(const Option &option) {
    return QString::fromUtf8(option.label) + "\n" + option.names.value(option.index);
}

void SettingsDialog::cycle() {
    QObject *button = sender();
    if (!button)
        return;
    const int index = button->property("option").toInt();
    if (index < 0 || index >= options_.size())
        return;
    Option &option = options_[index];
    option.index = (option.index + 1) % option.values.size();
    option.button->setText(text(option));
    if (onSet)
        onSet(option.section, option.key, option.values[option.index]);
}

void SettingsDialog::refresh(const Config &config) {
    for (int i = 0; i < options_.size(); ++i) {
        Option &option = options_[i];
        const QString key = QString::fromUtf8(option.key);
        int index = option.index;
        if (key == "pull")
            index = int(config.pull);
        else if (key == "push")
            index = config.push ? 0 : 1;
        else if (key == "on_open")
            index = config.onOpen ? 0 : 1;
        else if (key == "mode")
            index = config.connectWifi ? 0 : 1;
        else if (key == "document_id")
            index = int(config.documentId);
        else if (key == "notify")
            index = config.notify ? 0 : 1;
        else if (key == "menu_log")
            index = config.menuLog ? 0 : 1;
        else if (key == "verbose")
            index = config.verbose ? 0 : 1;
        option.index = qBound(0, index, option.values.size() - 1);
        option.button->setText(text(option));
    }
}

void SettingsDialog::setStatus(const QString &text) {
    status_->setText(text);
}

}
