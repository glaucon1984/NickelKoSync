// Nickel symbols, resolved with dlsym when first needed. Every one is
// optional: a missing symbol only disables the feature that needs it.
#ifndef KOSYNC_NICKEL_H
#define KOSYNC_NICKEL_H

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>

#include <dlfcn.h>

class QObject;
class QWidget;

namespace kosync {
namespace nickel {

template <class T> T symbol(const char *name) {
    return reinterpret_cast<T>(dlsym(RTLD_DEFAULT, name));
}

// Nickel's Volume handle: two words. VolumeManager::getById returns it by
// value; as the class is not trivially copyable, the ARM EABI passes a hidden
// result pointer first, which is what the leading parameter below receives.
// Destroy it with Volume::~Volume (see VolumeGuard in nickeldb.cc).
struct Volume {
    void *words[2];
};

struct Api {
    void *(*mainWindowController)() = symbol<decltype(mainWindowController)>("_ZN20MainWindowController14sharedInstanceEv");
    QWidget *(*currentView)(void *) = symbol<decltype(currentView)>("_ZNK20MainWindowController11currentViewEv");
    void *(*wirelessManager)() = symbol<decltype(wirelessManager)>("_ZN23WirelessWorkflowManager14sharedInstanceEv");
    void (*connectWirelessSilently)(void *) =
        symbol<decltype(connectWirelessSilently)>("_ZN23WirelessWorkflowManager23connectWirelessSilentlyEv");
    QObject *(*syncManager)() = symbol<decltype(syncManager)>("_ZN13N3SyncManager14sharedInstanceEv");
    const void *(*powerManager)() = symbol<decltype(powerManager)>("_ZN22N3PowerWorkflowManager14sharedInstanceEv");
    bool (*sleeping)(const void *) = symbol<decltype(sleeping)>("_ZNK22N3PowerWorkflowManager8sleepingEv");

    // Volume / database API, as Kobalt uses it to store a reading position.
    void (*getVolume)(Volume *, const QString &, const QString &) = symbol<decltype(getVolume)>("_ZN13VolumeManager7getByIdERK7QStringS2_");
    void (*destroyVolume)(void *) = symbol<decltype(destroyVolume)>("_ZN6VolumeD1Ev");
    QVariantMap (*values)(const void *) = symbol<decltype(values)>("_ZNK6Volume11getDbValuesEv");
    void *(*device)() = symbol<decltype(device)>("_ZN6Device16getCurrentDeviceEv");
    const QString &(*dbName)(const void *) = symbol<decltype(dbName)>("_ZNK6Device9getDbNameEv");
    QSqlDatabase (*connection)(QString, bool) = symbol<decltype(connection)>("_ZN15DatabaseAdapter12dbConnectionE7QStringb");
    void (*bookmark)(void *, const QString &, int, const QDateTime &) =
        symbol<decltype(bookmark)>("_ZN13VolumeManager14updateBookmarkER6VolumeRK7QStringiRK9QDateTime");
    void (*status)(void *, int) = symbol<decltype(status)>("_ZN7Content13setReadStatusE13ReadingStatus");
    void (*firstTime)(void *, const QVariant &) = symbol<decltype(firstTime)>("_ZN7Content19setFirstTimeReadingERK8QVariant");
    void (*percent)(void *, const QVariant &) = symbol<decltype(percent)>("_ZN7Content14setPercentReadERK8QVariant");
    bool (*update)(const QString &, const QVariantMap &, const QVariantMap &, const QString &) =
        symbol<decltype(update)>("_ZN13VolumeManager12updateFieldsERK7QStringRK4QMapIS0_8QVariantES7_S2_");
    void *(*refresh)(const QString &) = symbol<decltype(refresh)>("_ZN10Repository12refreshCacheI6VolumeEEP5CacheIT_ERK7QString");
    bool (*begin)(QString) = symbol<decltype(begin)>("_ZN15DatabaseAdapter16beginTransactionE7QString");
    bool (*end)(QString, bool) = symbol<decltype(end)>("_ZN15DatabaseAdapter14endTransactionE7QStringb");

    bool volumeApi() const {
        return getVolume && values && device && dbName && connection && bookmark && status && firstTime && percent &&
               update && refresh && begin && end && destroyVolume;
    }
};

const Api &api();

}
}

#endif
