// The KOSync screen, laid out like Kobo's own Settings pages: a title bar,
// section headers, and rows with the setting on the left and its value on the
// right. Tapping a row cycles the value, or opens the keyboard page for the
// account fields. Credentials and all other values go to config.ini.
#ifndef KOSYNC_SETTINGS_DIALOG_H
#define KOSYNC_SETTINGS_DIALOG_H

#include "config.h"

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QLayout;
class QShowEvent;
class QMouseEvent;

namespace kosync {

// One tappable row: label left, value right, separator below.
class SettingRow : public QWidget {
    Q_OBJECT
public:
    SettingRow(const QString &label, QWidget *parent = 0);
    void setValue(const QString &value);

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    QLabel *label_ = 0, *value_ = 0;
    bool pressed_ = false;
};

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const Config &config, QWidget *parent = 0);

    // Called when a setting changes: section, key, new value for config.ini.
    std::function<void(const char *, const char *, const QString &)> onSet;
    // Called by the sync buttons; true asks for the whole library.
    std::function<void(bool)> onSync;

    void setStatus(const QString &text);
    void refresh(const Config &config);

protected:
    void showEvent(QShowEvent *event);

private slots:
    void cycle();
    void editAccount();
    void settle();

private:
    struct Option {
        const char *section, *key;
        QStringList values, names;
        SettingRow *row = 0;
        int index = 0;
    };
    QWidget *section(const QString &title);
    void addOption(QLayout *layout, const char *section, const char *key, const char *label, const QStringList &values,
                   const QStringList &names, int index);
    void addAccount(QLayout *layout, const char *key, const char *label);
    QString accountText(const QString &key) const;
    void refreshAccount();

    QVector<Option> options_;
    QVector<SettingRow *> account_;
    QLabel *status_ = 0;
    QString url_, username_;
    bool hasPassword_ = false;
};

}

#endif
