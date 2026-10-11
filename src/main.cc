// Entry point: NickelHook registration and start-up of the controller.
#include <NickelHook.h>

#include <pthread.h>
#include <unistd.h>

#include <QCoreApplication>
#include <QMetaObject>
#include <QTimer>

#include "config.h"
#include "controller.h"

namespace {

kosync::Controller *controller = 0;
const int startDelayMs = 20000;  // let Nickel finish its own start-up first

void scheduleStart() {
    // QTimer::singleShot must be called on a thread with an event loop; the
    // controller lives on Nickel's main thread, so post the call there.
    QMetaObject::invokeMethod(controller, "startLater", Qt::QueuedConnection);
}

// NickelHook runs the mod's init from the library constructor, which Qt's
// plugin loader triggers on Nickel's main thread. Nickel's QApplication
// normally exists by then; if it does not, wait for it on a helper thread.
void *waitForApplication(void *) {
    for (int i = 0; i < 600 && !QCoreApplication::instance(); ++i)
        usleep(100 * 1000);
    if (!QCoreApplication::instance()) {
        nh_log("kosync: no Qt application after 60 s, not starting");
        return 0;
    }
    scheduleStart();
    return 0;
}

int init() {
    controller = new kosync::Controller();
    if (QCoreApplication::instance()) {
        scheduleStart();
        return 0;
    }
    pthread_t thread;
    pthread_attr_t attributes;
    pthread_attr_init(&attributes);
    pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
    const int error = pthread_create(&thread, &attributes, waitForApplication, 0);
    pthread_attr_destroy(&attributes);
    if (error) {
        nh_log("kosync: cannot create start-up thread (%d)", error);
        return 1;
    }
    return 0;
}

// GCC 4.9 accepts designated initializers in C++ only when every field is
// listed in order, so these tables are positional: see NickelHook.h.
struct nh_info info = {
    "NickelKoSync",                                                  // name
    "Sync reading progress with KOReader's sync server",             // desc
    KS_CONFIG_DIR "/uninstall",                                      // uninstall_flag
    0,                                                               // uninstall_xflag
    5,                                                               // failsafe_delay
};

struct nh_hook hooks[] = {
    {0},
};

struct nh_dlsym symbols[] = {
    {0},
};

}

namespace kosync {
void Controller::startLater() {
    QTimer::singleShot(startDelayMs, this, SLOT(start()));
}
}

NickelHook(
    .init = init,
    .info = &info,
    .hook = hooks,
    .dlsym = symbols,
    .uninstall = 0,
)
