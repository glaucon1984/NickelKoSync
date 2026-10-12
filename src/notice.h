// The mod's own notification: a box at the bottom of the screen that goes
// away when tapped, or by itself after a few seconds, like KOReader's.
#ifndef KOSYNC_NOTICE_H
#define KOSYNC_NOTICE_H

#include <QString>
#include <QWidget>

class QLabel;
class QMouseEvent;
class QTimer;

namespace kosync {

class Notice : public QWidget {
    Q_OBJECT
public:
    // Shows `text` (replacing any notice on screen) for `milliseconds`.
    static void show(const QString &title, const QString &text, int milliseconds);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);

private:
    Notice();
    void present(const QString &title, const QString &text, int milliseconds);
    QLabel *title_ = 0, *text_ = 0;
    QTimer *timer_ = 0;
};

}

#endif
