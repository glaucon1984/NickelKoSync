// The mod's brain: notices when a book is closed in Kobalt or in Nickel's
// reader, keeps a queue of books to synchronise, talks to the sync server when
// the device is online, and applies newer remote positions to both readers.
#ifndef KOSYNC_CONTROLLER_H
#define KOSYNC_CONTROLLER_H

#include "client.h"
#include "config.h"

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QPointer>
#include <QStringList>

class QFileSystemWatcher;
class QTimer;

namespace kosync {

struct BookRow;
class SettingsDialog;
struct LocalPosition {
    bool valid = false;
    QString source;        // "kobalt" or "nickel"
    QString progress;      // XPointer
    double percentage = 0; // 0..1
    qint64 changedAt = 0;  // seconds since the epoch
    bool finished = false;
    QString title, author, fileName;
};

class Controller : public QObject {
    Q_OBJECT
public:
    explicit Controller(QObject *parent = 0);

public slots:
    void start();
    void startLater();

private slots:
    void onKobaltDirectoryChanged();
    void onConfigDirectoryChanged();
    void tick();
    void networkTick();
    void onNickelSyncFinished();

private:
    // configuration and persistent state
    void loadConfig();
    void loadState();
    void saveState();
    QJsonObject bookState(const QString &contentId) const;
    void setBookState(const QString &contentId, const QJsonObject &state);

    // local positions
    LocalPosition localPosition(const BookRow &row) const;
    QString documentId(const QString &filePath) const;
    void noteChange(const QString &contentId);
    QJsonObject currentSeen(const BookRow &row) const;

    // the queue
    void enqueue(const QString &contentId, const char *reason);
    void enqueueRecent(const char *reason);
    void enqueueAll(const char *reason);
    void writeNickelMenu();
    void showSettings();
    void toast(const QString &title, const QString &body);
    void noteEvent(const QString &text);
    void flushEvents();
    void onBookOpened(const QString &contentId, const char *reader);
    QString openBook() const;
    QString statusText() const;
    void processQueue();
    void syncBook(const QString &contentId);
    void finishBook(const QString &contentId, bool success);

    // applying remote progress
    bool apply(const BookRow &row, const RemoteProgress &remote, const LocalPosition &local, QString *error);

    // device state
    bool online() const;
    bool awake() const;
    void requestWifi();

    Config config_;
    bool configOk_ = false;
    bool authFailed_ = false;
    bool authChecked_ = false;
    Client client_;
    QJsonObject state_;
    QString deviceId_;
    QFileSystemWatcher *watcher_ = 0;
    QTimer *tickTimer_ = 0, *networkTimer_ = 0;

    QStringList queue_;
    QSet<QString> deferred_;
    QHash<QString, int> failures_;
    QMap<QString, qint64> pendingCloses_;  // content id -> when the reader closed it (ms)
    bool inFlight_ = false;
    QString kobaltBook_;     // the book Kobalt has open, as last observed
    bool readingView_ = false;
    bool kobaltMarker_ = false;
    qint64 lastWifiRequest_ = 0, lastPeriodic_ = 0, lastQueueLog_ = 0, retryAfter_ = 0;
    QDateTime configModified_;
    QStringList events_;
    QString lastEvent_;
    bool manualSync_ = false;   // started from the screen: always report the outcome
    int checkedSinceManual_ = 0;
    bool wasAsleep_ = false;
    QPointer<SettingsDialog> dialog_;
};

}

#endif
