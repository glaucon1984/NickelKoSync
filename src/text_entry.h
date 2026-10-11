// A full-screen text entry page with its own on-screen keyboard, for the few
// settings that need typing (server, username, password). Independent of
// Nickel's keyboard classes, whose layout changes between firmware builds.
#ifndef KOSYNC_TEXT_ENTRY_H
#define KOSYNC_TEXT_ENTRY_H

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QPushButton;
class QShowEvent;

namespace kosync {

class TextEntryDialog : public QDialog {
    Q_OBJECT
public:
    TextEntryDialog(const QString &title, const QString &initial, bool password, QWidget *parent = 0);
    QString value() const;

protected:
    void showEvent(QShowEvent *event);

private slots:
    void settle();
    void key();
    void shift();
    void symbols();
    void backspace();
    void toggleEcho();

private:
    void relabel();

    QLineEdit *edit_ = 0;
    QPushButton *shift_ = 0, *symbols_ = 0, *echo_ = 0;
    QVector<QPushButton *> keys_;  // 4 rows of 10
    bool upper_ = false, symbolPage_ = false;
};

}

#endif
