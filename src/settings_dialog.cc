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

QString dialogStyle() {
    return QString(
               "QDialog { background: white; }"
               "QLabel { color: black; background: transparent; font-size: %1px; }"
               "QLabel#title { font-size: %2px; }"
               "QLabel#section { background: #d8d8d8; color: #202020; padding: 4px 10px; font-size: %3px; font-weight: bold; }"
               "QLabel#value { color: #202020; font-style: italic; font-size: %1px; }"
               "QLabel#status { color: #404040; font-size: %4px; }"
               "kosync--SettingRow { border-bottom: 1px solid #a0a0a0; background: white; }"
               "kosync--ToggleSwitch { background: transparent; }"
               "QPushButton { color: black; background: white; border: 2px solid black; border-radius: 8px;"
               "  padding: 8px 16px; font-size: %1px; }"
               "QPushButton:pressed { color: white; background: black; }"
               "QPushButton#action { background: black; color: white; }"
               "QPushButton#action:pressed { background: white; color: black; }"
               "QPushButton#plain { border: none; padding: 4px 12px; }"
               "QPushButton#choice { border: none; border-bottom: 1px solid #a0a0a0; border-radius: 0;"
               "  text-align: left; padding: 18px 20px; }"
               "QPushButton#choice:pressed { background: black; color: white; }")
        .arg(scale(19))
        .arg(scale(27))
        .arg(scale(14))
        .arg(scale(16));
}

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
    setStyleSheet(dialogStyle());
    QFont base = font();
    base.setPixelSize(scale(19));
    setFont(base);

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(scale(40), scale(28), scale(40), scale(28));
    outer->setSpacing(scale(14));

    // Title bar: name on the left, close on the right.
    QHBoxLayout *bar = new QHBoxLayout;
    QLabel *title = new QLabel("KOSync", this);
    title->setObjectName("title");
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
    option.label = QString::fromUtf8(label);
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
    if (option.toggle) {
        option.index = (option.index + 1) % option.values.size();
        option.row->setValue(option.names[option.index]);
        option.row->setOn(option.index == 0);
        if (onSet)
            onSet(option.section, option.key, option.values[option.index]);
        return;
    }
    ChoiceDialog *chooser = new ChoiceDialog(option.label, option.names, option.index, this);
    chooser->setAttribute(Qt::WA_DeleteOnClose);
    connect(chooser, &QDialog::accepted, this, [this, chooser, index] {
        Option &option = options_[index];
        const int chosen = chooser->chosen();
        if (chosen < 0 || chosen >= option.values.size() || chosen == option.index)
            return;
        option.index = chosen;
        option.row->setValue(option.names[option.index]);
        if (onSet)
            onSet(option.section, option.key, option.values[option.index]);
    });
    chooser->show();
}

// ---------------------------------------------------------------------------

ChoiceDialog::ChoiceDialog(const QString &title, const QStringList &names, int current, QWidget *parent)
    : QDialog(parent) {
    setObjectName("kosyncChoice");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setStyleSheet(dialogStyle() + "QDialog { border: 3px solid black; }");
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(scale(24), scale(24), scale(24), scale(24));
    layout->setSpacing(scale(8));
    QLabel *heading = new QLabel(title, this);
    heading->setObjectName("title");
    heading->setWordWrap(true);
    layout->addWidget(heading);
    for (int i = 0; i < names.size(); ++i) {
        QPushButton *button = new QPushButton(names[i] + (i == current ? QString::fromUtf8("   (current)") : QString()), this);
        button->setObjectName("choice");
        button->setMinimumHeight(scale(76));
        button->setProperty("choice", i);
        connect(button, SIGNAL(clicked()), this, SLOT(pick()));
        layout->addWidget(button);
    }
    layout->addSpacing(scale(12));
    QPushButton *cancel = new QPushButton("Cancel", this);
    cancel->setMinimumHeight(scale(64));
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));
    layout->addWidget(cancel);
    const QScreen *screen = QApplication::primaryScreen();
    const QRect area = screen ? screen->geometry() : QRect(0, 0, 1072, 1448);
    const int width = area.width() * 4 / 5;
    adjustSize();
    const int height = qMax(sizeHint().height(), scale(200));
    setGeometry(area.x() + (area.width() - width) / 2, area.y() + (area.height() - height) / 2, width, height);
    eink::markControls(this);
}

void ChoiceDialog::pick() {
    QObject *button = sender();
    if (!button)
        return;
    chosen_ = button->property("choice").toInt();
    accept();
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
