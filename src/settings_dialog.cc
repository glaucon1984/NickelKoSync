#include "settings_dialog.h"

#include "eink.h"
#include "text_entry.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
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
    "QLabel#section { background: #d8d8d8; color: #202020; padding: 6px 10px; }"
    "QLabel#value { color: #202020; font-style: italic; }"
    "QLabel#status { color: #404040; }"
    "kosync--SettingRow { border-bottom: 1px solid #a0a0a0; background: white; }"
    "kosync--ToggleSwitch { background: transparent; }"
    "QPushButton { color: black; background: white; border: 2px solid black; border-radius: 8px; padding: 8px 16px; }"
    "QPushButton:pressed { color: white; background: black; }"
    "QPushButton#action { background: black; color: white; }"
    "QPushButton#action:pressed { background: white; color: black; }"
    "QPushButton#plain { border: none; padding: 4px 12px; }";

QStringList list(const char *a, const char *b, const char *c = 0) {
    QStringList values;
    values << QString::fromUtf8(a) << QString::fromUtf8(b);
    if (c)
        values << QString::fromUtf8(c);
    return values;
}

}

// ---------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch(QWidget *parent) : QWidget(parent) {
    setFixedSize(sizeHint());
}

QSize ToggleSwitch::sizeHint() const {
    return QSize(scale(74), scale(38));
}

void ToggleSwitch::setOn(bool on) {
    if (on_ == on)
        return;
    on_ = on;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const int border = qMax(2, scale(3));
    const QRectF track = QRectF(rect()).adjusted(border, border, -border, -border);
    const qreal radius = track.height() / 2;
    painter.setPen(QPen(Qt::black, border));
    painter.setBrush(on_ ? Qt::black : Qt::white);
    painter.drawRoundedRect(track, radius, radius);
    const qreal knob = track.height() - 2 * border;
    const QRectF circle(on_ ? track.right() - knob - border : track.left() + border, track.top() + border, knob, knob);
    painter.setPen(QPen(on_ ? Qt::white : Qt::black, border));
    painter.setBrush(on_ ? Qt::white : Qt::black);
    painter.drawEllipse(circle);
}

SettingRow::SettingRow(const QString &label, bool toggle, QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground, true);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(scale(8), scale(18), scale(8), scale(18));
    layout->setSpacing(scale(16));
    label_ = new QLabel(label, this);
    label_->setWordWrap(true);
    value_ = new QLabel(this);
    value_->setObjectName("value");
    value_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    value_->setWordWrap(true);
    layout->addWidget(label_, 3);
    layout->addWidget(value_, 2);
    if (toggle) {
        switch_ = new ToggleSwitch(this);
        layout->addWidget(switch_, 0, Qt::AlignVCenter);
    }
    setMinimumHeight(scale(72));
}

void SettingRow::setValue(const QString &value) {
    value_->setText(value);
}

void SettingRow::setOn(bool on) {
    if (switch_)
        switch_->setOn(on);
}

void SettingRow::mousePressEvent(QMouseEvent *event) {
    pressed_ = true;
    event->accept();
}

void SettingRow::mouseReleaseEvent(QMouseEvent *event) {
    const bool inside = pressed_ && rect().contains(event->pos());
    pressed_ = false;
    event->accept();
    if (inside)
        emit clicked();
}

// ---------------------------------------------------------------------------

SettingsDialog::SettingsDialog(const Config &config, QWidget *parent)
    : QDialog(parent), url_(config.url), username_(config.username), hasPassword_(!config.userKey.isEmpty()) {
    setObjectName("kosyncSettings");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setStyleSheet(dialogStyle);
    QFont base = font();
    base.setPixelSize(scale(20));
    setFont(base);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(scale(40), scale(28), scale(40), scale(28));
    outer->setSpacing(scale(14));

    // Title bar: name on the left, close on the right.
    QHBoxLayout *bar = new QHBoxLayout;
    QLabel *title = new QLabel("KOSync", this);
    QFont titleFont = base;
    titleFont.setPixelSize(scale(30));
    title->setFont(titleFont);
    bar->addWidget(title, 1);
    QPushButton *close = new QPushButton("Close", this);
    close->setObjectName("plain");
    close->setMinimumHeight(scale(60));
    {
        // The UI font has no glyph for a cross; draw one.
        const int size = scale(26);
        QPixmap cross(size, size);
        cross.fill(Qt::transparent);
        QPainter painter(&cross);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(QPen(Qt::black, qMax(2, scale(3)), Qt::SolidLine, Qt::RoundCap));
        const int inset = size / 5;
        painter.drawLine(inset, inset, size - inset, size - inset);
        painter.drawLine(size - inset, inset, inset, size - inset);
        close->setIcon(QIcon(cross));
        close->setIconSize(QSize(size, size));
        close->setLayoutDirection(Qt::RightToLeft);  // icon after the text
    }
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    bar->addWidget(close);
    outer->addLayout(bar);

    QHBoxLayout *actions = new QHBoxLayout;
    actions->setSpacing(scale(20));
    QPushButton *syncNow = new QPushButton("Sync now", this);
    syncNow->setObjectName("action");
    syncNow->setMinimumHeight(scale(64));
    connect(syncNow, &QPushButton::clicked, this, [this] {
        if (onSync)
            onSync(false);
    });
    QPushButton *syncAll = new QPushButton("Check whole library", this);
    syncAll->setObjectName("action");
    syncAll->setMinimumHeight(scale(64));
    connect(syncAll, &QPushButton::clicked, this, [this] {
        if (onSync)
            onSync(true);
    });
    actions->addWidget(syncNow);
    actions->addWidget(syncAll);
    outer->addLayout(actions);

    status_ = new QLabel(this);
    status_->setObjectName("status");
    status_->setWordWrap(true);
    QFont small = base;
    small.setPixelSize(scale(17));
    status_->setFont(small);
    outer->addWidget(status_);

    QVBoxLayout *rows = new QVBoxLayout;
    rows->setContentsMargins(0, scale(6), 0, 0);
    rows->setSpacing(0);
    outer->addLayout(rows);

    rows->addWidget(section("ACCOUNT"));
    addAccount(rows, "url", "Server");
    addAccount(rows, "username", "Username");
    addAccount(rows, "password", "Password");

    rows->addWidget(section("SYNC"));
    addOption(rows, "sync", "pull", "Other devices' positions", list("newer", "forward", "off"),
              list("Newest wins", "Forward only", "Ignore"), int(config.pull));
    addOption(rows, "sync", "push", "Send my progress", list("true", "false"), list("On", "Off"),
              config.push ? 0 : 1);
    addOption(rows, "sync", "on_open", "Check on open and wake", list("true", "false"),
              list("On", "Off"), config.onOpen ? 0 : 1);
    addOption(rows, "wifi", "mode", "Wi-Fi", list("connect", "keep"), list("Connect if needed", "Leave alone"),
              config.connectWifi ? 0 : 1);
    addOption(rows, "sync", "document_id", "Match books by", list("binary", "filename"),
              list("File content", "File name"), int(config.documentId));

    rows->addWidget(section("OPTIONS"));
    addOption(rows, "options", "notify", "Notifications", list("true", "false"), list("On", "Off"),
              config.notify ? 0 : 1);
    addOption(rows, "options", "menu_log", "Log entry in the menu", list("true", "false"), list("On", "Off"),
              config.menuLog ? 0 : 1);
    addOption(rows, "options", "verbose", "Detailed log", list("true", "false"), list("On", "Off"),
              config.verbose ? 0 : 1);
    outer->addStretch(1);

    if (const QScreen *screen = QApplication::primaryScreen())
        setGeometry(screen->geometry());
    eink::markControls(this);
}

QWidget *SettingsDialog::section(const QString &title) {
    QLabel *label = new QLabel(title, this);
    label->setObjectName("section");
    QFont f = font();
    f.setPixelSize(scale(15));
    f.setBold(true);
    label->setFont(f);
    label->setMinimumHeight(scale(34));
    return label;
}

void SettingsDialog::addOption(QLayout *layout, const char *section, const char *key, const char *label,
                               const QStringList &values, const QStringList &names, int index) {
    Option option;
    option.section = section;
    option.key = key;
    option.values = values;
    option.names = names;
    option.index = qBound(0, index, values.size() - 1);
    option.toggle = values.size() == 2 && values[0] == "true" && values[1] == "false";
    option.row = new SettingRow(QString::fromUtf8(label), option.toggle, this);
    option.row->setProperty("option", options_.size());
    option.row->setValue(names.value(option.index));
    option.row->setOn(option.index == 0);
    connect(option.row, SIGNAL(clicked()), this, SLOT(cycle()));
    layout->addWidget(option.row);
    options_.append(option);
}

void SettingsDialog::addAccount(QLayout *layout, const char *key, const char *label) {
    SettingRow *row = new SettingRow(QString::fromUtf8(label), false, this);
    row->setProperty("key", QString::fromUtf8(key));
    row->setProperty("label", QString::fromUtf8(label));
    row->setValue(accountText(QString::fromUtf8(key)));
    connect(row, SIGNAL(clicked()), this, SLOT(editAccount()));
    layout->addWidget(row);
    account_.append(row);
}

QString SettingsDialog::accountText(const QString &key) const {
    if (key == "url")
        return url_.isEmpty() ? QString("Not set") : url_;
    if (key == "username")
        return username_.isEmpty() ? QString("Not set") : username_;
    return hasPassword_ ? QString::fromUtf8("\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2\xE2\x80\xA2")
                        : QString("Not set");
}

void SettingsDialog::refreshAccount() {
    for (int i = 0; i < account_.size(); ++i)
        account_[i]->setValue(accountText(account_[i]->property("key").toString()));
}

void SettingsDialog::cycle() {
    QObject *row = sender();
    if (!row)
        return;
    const int index = row->property("option").toInt();
    if (index < 0 || index >= options_.size())
        return;
    Option &option = options_[index];
    option.index = (option.index + 1) % option.values.size();
    option.row->setValue(option.names[option.index]);
    option.row->setOn(option.index == 0);
    if (onSet)
        onSet(option.section, option.key, option.values[option.index]);
}

void SettingsDialog::editAccount() {
    QObject *row = sender();
    if (!row)
        return;
    const QString key = row->property("key").toString();
    const QString label = row->property("label").toString();
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
        refreshAccount();
    });
    entry->show();
}

void SettingsDialog::refresh(const Config &config) {
    url_ = config.url;
    username_ = config.username;
    hasPassword_ = !config.userKey.isEmpty();
    refreshAccount();
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
        option.row->setValue(option.names[option.index]);
        option.row->setOn(option.index == 0);
    }
}

void SettingsDialog::setStatus(const QString &text) {
    status_->setText(text);
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

}
