#include "notice.h"

#include "eink.h"

#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace kosync {
namespace {

int scale(int px) {
    const QScreen *screen = QApplication::primaryScreen();
    const int width = screen ? qMin(screen->size().width(), screen->size().height()) : 1072;
    return px * width / 1072;
}

QPointer<Notice> current;

}

Notice::Notice() : QWidget(0) {
    setObjectName("kosyncNotice");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QString("QWidget#kosyncNotice { background: white; border: 3px solid black; border-radius: 14px; }"
                          "QLabel { color: black; background: transparent; font-size: %1px; }"
                          "QLabel#title { font-size: %2px; font-weight: bold; }")
                      .arg(scale(27))
                      .arg(scale(24)));
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(scale(28), scale(22), scale(28), scale(22));
    layout->setSpacing(scale(8));
    title_ = new QLabel(this);
    title_->setObjectName("title");
    text_ = new QLabel(this);
    text_->setWordWrap(true);
    layout->addWidget(title_);
    layout->addWidget(text_);
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, SIGNAL(timeout()), this, SLOT(close()));
}

void Notice::show(const QString &title, const QString &text, int milliseconds) {
    if (!current)
        current = new Notice();
    current->present(title, text, milliseconds);
}

void Notice::present(const QString &title, const QString &text, int milliseconds) {
    title_->setText(title);
    text_->setText(text);
    const QScreen *screen = QApplication::primaryScreen();
    const QRect area = screen ? screen->geometry() : QRect(0, 0, 1072, 1448);
    const int width = area.width() * 5 / 6;
    setFixedWidth(width);
    adjustSize();
    const int height = sizeHint().height();
    move(area.x() + (area.width() - width) / 2, area.y() + area.height() - height - scale(120));
    eink::markControls(this);
    QWidget::show();
    raise();
    timer_->start(milliseconds);
}

void Notice::mousePressEvent(QMouseEvent *event) {
    event->accept();
}

void Notice::mouseReleaseEvent(QMouseEvent *event) {
    event->accept();
    close();
}

}
