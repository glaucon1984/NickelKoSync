// Nickel's Qt delivers touches to windows as touch events only; its own
// widgets handle those directly. Ordinary Qt widgets expect mouse events, so
// this application-wide filter turns a single-finger touch inside one of the
// mod's windows (object names starting with "kosync") into a press, moves and
// a release for the widget under the finger. After Kobalt's touch_input.cpp.
#ifndef KOSYNC_TOUCH_INPUT_H
#define KOSYNC_TOUCH_INPUT_H

class QObject;

namespace kosync {

void installTouchInput(QObject *owner);

}

#endif
