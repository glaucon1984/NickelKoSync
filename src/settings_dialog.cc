#include "settings_dialog.h"

#include "eink.h"
#include "text_entry.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
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
    "QPushButton { color: black; background: white; border: 2px solid black; border-radius: 10px;"
    "  padding: 10px 20px; text-align: left; }"
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

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent)
    : QDialog(parent), url_(config.url), username_(config.username), hasPassword_(!config.userKey.isEmpty()) {
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setStyleSheet(dialogStyle);
    QFont base = font();
    base.setPixelSize(scale(26));
    setFont(base);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(scale(36), scale(36), scale(36), scale(36));
    outer->setSpacing(scale(16));

    QLabel *title = new QLabel("KOSync", this);
    QFont titleFont = base;
    titleFont.setPixelSize(scale(40));
    titleFont.setBold(true);
    title->setFont(titleFont);
    outer->addWidget(title);

    QHBoxLayout *actions = new QHBoxLayout;
    actions->setSpacing(scale(24));
    QPushButton *syncNow = new QPushButton("Sync now", this);
    syncNow->setObjectName("action");
    syncNow->setMinimumHeight(scale(90));
    connect(syncNow, &QPushButton::clicked, this, [this] {
        if (onSync)
            onSync(false);
    });
    QPushButton *syncAll = new QPushButton("Check whole library", this);
    syncAll->setObjectName("action");
    syncAll->setMinimumHeight(scale(90));
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
    small.setPixelSize(scale(22));
    status_->setFont(small);
    outer->addWidget(status_);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    QWidget *inner = new QWidget(scroll);
    QVBoxLayout *rows = new QVBoxLayout(inner);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(scale(12));
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);

    addAccountButton("url", "Server");
    addAccountButton("username", "Username");
    addAccountButton("password", "Password");
    for (int i = 0; i < account_.size(); ++i)
        rows->addWidget(account_[i]);

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
    close->setMinimumHeight(scale(90));
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    outer->addWidget(close);

    if (const QScreen *screen = QApplication::primaryScreen())
        setGeometry(screen->geometry());
    eink::markControls(this);
}

void SettingsDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    // The first paint clears the panel; later paints update their own area.
    eink::markFlash(this);
    QTimer::singleShot(800, this, SLOT(settle()));
}

void SettingsDialog::settle() {
    eink::markControl(this);
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
    option.button->setMinimumHeight(scale(96));
    option.button->setProperty("option", options_.size());
    option.button->setText(text(option));
    connect(option.button, SIGNAL(clicked()), this, SLOT(cycle()));
    options_.append(option);
}

void SettingsDialog::addAccountButton(const char *key, const char *label) {
    QPushButton *button = new QPushButton(this);
    button->setMinimumHeight(scale(96));
    button->setProperty("key", QString::fromUtf8(key));
    button->setProperty("label", QString::fromUtf8(label));
    button->setText(QString::fromUtf8(label) + "\n" + accountText(QString::fromUtf8(key)));
    connect(button, SIGNAL(clicked()), this, SLOT(editAccount()));
    account_.append(button);
}

QString SettingsDialog::accountText(const QString &key) const {
    if (key == "url")
        return url_.isEmpty() ? QString("not set") : url_;
    if (key == "username")
        return username_.isEmpty() ? QString("not set") : username_;
    return hasPassword_ ? QString::fromUtf8("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2")
                        : QString("not set");
}

void SettingsDialog::editAccount() {
    QObject *button = sender();
    if (!button)
        return;
    const QString key = button->property("key").toString();
    const QString label = button->property("label").toString();
    const bool password = key == "password";
    const QString initial = key == "url" ? url_ : key == "username" ? username_ : QString();
    TextEntryDialog *entry = new TextEntryDialog(label, initial, password, this);
    entry->setAttribute(Qt::WA_DeleteOnClose);
    connect(entry, &QDialog::accepted, this, [this, entry, key, password] {
        const QString value = entry->value().trimmed();
        if (password && value.isEmpty())
            return;
        if (onSet) {
            onSet("server", key.toUtf8().constData(), value);
            if (password)
                onSet("server", "password_md5", QString());  // the plain password now rules
        }
        if (key == "url")
            url_ = value;
        else if (key == "username")
            username_ = value;
        else
            hasPassword_ = true;
        for (int i = 0; i < account_.size(); ++i) {
            const QString k = account_[i]->property("key").toString();
            account_[i]->setText(account_[i]->property("label").toString() + "\n" + accountText(k));
        }
    });
    entry->show();
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
    url_ = config.url;
    username_ = config.username;
    hasPassword_ = !config.userKey.isEmpty();
    for (int i = 0; i < account_.size(); ++i) {
        const QString k = account_[i]->property("key").toString();
        account_[i]->setText(account_[i]->property("label").toString() + "\n" + accountText(k));
    }
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
