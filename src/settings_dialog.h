// The KOSync screen: sync buttons and the settings that make sense to change
// on the device, as buttons that cycle through their values. Credentials stay
// in config.ini (they need a keyboard).
#ifndef KOSYNC_SETTINGS_DIALOG_H
#define KOSYNC_SETTINGS_DIALOG_H

#include "config.h"

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class QLabel;
class QPushButton;
class QShowEvent;

namespace kosync {

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(const Config &config, QWidget *parent = 0);

    // Called when a setting is cycled: section, key, new value for config.ini.
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
        const char *section, *key, *label;
        QStringList values, names;
        QPushButton *button = 0;
        int index = 0;
    };
    void addOption(const char *section, const char *key, const char *label, const QStringList &values,
                   const QStringList &names, int index);
    static QString text(const Option &option);

    void addAccountButton(const char *key, const char *label);
    QString accountText(const QString &key) const;

    QVector<Option> options_;
    QVector<QPushButton *> account_;
    QLabel *status_ = 0;
    QString url_, username_;
    bool hasPassword_ = false;
};

}

#endif
