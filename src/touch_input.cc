#include "touch_input.h"

#include <QApplication>
#include <QEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QTouchEvent>
#include <QWidget>

namespace kosync {
namespace {

bool ownWindow(const QWidget *widget) {
    for (const QWidget *window = widget ? widget->window() : 0; window;
         window = window->parentWidget() ? window->parentWidget()->window() : 0)
        if (window->objectName().startsWith("kosync"))
            return true;
    return false;
}

bool takesTouch(const QWidget *widget) {
    for (; widget; widget = widget->isWindow() ? 0 : widget->parentWidget())
        if (widget->testAttribute(Qt::WA_AcceptTouchEvents))
            return true;
    return false;
}

class TouchInput : public QObject {
public:
    explicit TouchInput(QObject *owner) : QObject(owner) {}

protected:
    bool eventFilter(QObject *receiver, QEvent *event) {
        const QEvent::Type type = event->type();
        if (type == QEvent::MouseButtonPress && event->spontaneous() && receiver->isWidgetType() &&
            ownWindow(static_cast<QWidget *>(receiver))) {
            // This Qt synthesizes mouse events itself; do not click twice.
            native = true;
            pressed.clear();
        }
        if (native || !receiver->isWindowType() ||
            (type != QEvent::TouchBegin && type != QEvent::TouchUpdate && type != QEvent::TouchEnd &&
             type != QEvent::TouchCancel))
            return false;
        const QList<QTouchEvent::TouchPoint> points = static_cast<QTouchEvent *>(event)->touchPoints();
        if (type == QEvent::TouchBegin) {
            pressed.clear();
            if (points.size() != 1)
                return false;
            const QPoint screen = points.first().screenPos().toPoint();
            QWidget *target = QApplication::widgetAt(screen);
            if (!target || !ownWindow(target) || takesTouch(target))
                return false;
            pressed = target;
            for (QWidget *widget = target; widget; widget = widget->isWindow() ? 0 : widget->parentWidget())
                if (widget->isEnabled() && (widget->focusPolicy() & Qt::ClickFocus)) {
                    widget->setFocus(Qt::MouseFocusReason);
                    break;
                }
            if (!pressed)
                return false;
            send(QEvent::MouseButtonPress, screen, Qt::LeftButton);
            return false;
        }
        if (!pressed)
            return false;
        if (type == QEvent::TouchCancel || points.size() != 1) {
            send(QEvent::MouseButtonRelease, pressed->mapToGlobal(QPoint(-1, -1)), Qt::NoButton);
            pressed.clear();
            return false;
        }
        const QPoint screen = points.first().screenPos().toPoint();
        if (type == QEvent::TouchUpdate) {
            send(QEvent::MouseMove, screen, Qt::LeftButton);
        } else {
            send(QEvent::MouseButtonRelease, screen, Qt::NoButton);
            pressed.clear();
        }
        return false;
    }

private:
    QPointer<QWidget> pressed;
    bool native = false;

    void send(QEvent::Type type, const QPoint &screen, Qt::MouseButtons held) {
        const QPointer<QWidget> target = pressed;
        if (!target)
            return;
        const QPointF local = target->mapFromGlobal(screen);
        const QPointF window = target->window()->mapFromGlobal(screen);
        QMouseEvent mouse(type, local, window, screen, type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held,
                          Qt::NoModifier);
        QApplication::sendEvent(target, &mouse);
    }
};

}

void installTouchInput(QObject *owner) {
    qApp->installEventFilter(new TouchInput(owner));
}

}
