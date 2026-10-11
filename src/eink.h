// Kobo's Qt refreshes the e-ink panel according to widget attributes of its
// own (numbered from 128 in Qt::WidgetAttribute): without them a widget may
// be painted into the framebuffer but never shown. Firmware 4 numbering, as
// documented by Kobalt's refresh.cpp: 129 heuristic update mode, 130 partial,
// 131 full, 132 automatic waveform, 134 GC16 (the clean, flashing waveform).
#ifndef KOSYNC_EINK_H
#define KOSYNC_EINK_H

#include <QList>
#include <QPushButton>
#include <QWidget>

namespace kosync {
namespace eink {

const int updateHeuristic = 129, updatePartial = 130, updateFull = 131, waveformAutomatic = 132, waveformGc16 = 134;

inline void clearPolicy(QWidget *widget) {
    for (int attribute = 129; attribute <= 141; ++attribute)
        widget->setAttribute(static_cast<Qt::WidgetAttribute>(attribute), false);
}

// Ordinary controls: the driver picks the waveform, updates stay local.
inline void markControl(QWidget *widget) {
    clearPolicy(widget);
    widget->setAttribute(static_cast<Qt::WidgetAttribute>(waveformAutomatic), true);
    widget->setAttribute(static_cast<Qt::WidgetAttribute>(updateHeuristic), true);
    if (qobject_cast<QPushButton *>(widget))
        widget->setFocusPolicy(Qt::NoFocus);
}

// One clean full-panel flash, for a window that has just appeared.
inline void markFlash(QWidget *widget) {
    clearPolicy(widget);
    widget->setAttribute(static_cast<Qt::WidgetAttribute>(waveformGc16), true);
    widget->setAttribute(static_cast<Qt::WidgetAttribute>(updateFull), true);
}

inline void markControls(QWidget *root) {
    markControl(root);
    foreach (QWidget *child, root->findChildren<QWidget *>())
        markControl(child);
}

}
}

#endif
