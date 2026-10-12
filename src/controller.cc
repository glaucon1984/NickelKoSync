#include "controller.h"

#include "digest.h"
#include "epub.h"
#include "kobalt_state.h"
#include "log.h"
#include "mapping.h"
#include "nickel.h"
#include "nickeldb.h"
#include "notice.h"
#include "settings_dialog.h"
#include "touch_input.h"
#include "xpath.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QApplication>
#include <QFileSystemWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>

#ifndef NH_VERSION
#define NH_VERSION "dev"
#endif

namespace kosync {
namespace {

const int tickMs = 2000;
const int networkMs = 5000;
const int closeSettleMs = 2500;
const int retryDelaySeconds = 90;
const int maxFailures = 5;
const char *const stateFile = KS_CONFIG_DIR "/state.json";
const char *const configFile = KS_CONFIG_DIR "/config.ini";
const char *const syncNowFile = KS_CONFIG_DIR "/sync-now";
const char *const settingsFile = KS_CONFIG_DIR "/settings";
const int maxToastLines = 3;
const char *const kobaltMarkerFile = "/mnt/onboard/.adds/kobalt/active-session.json";

qint64 nowSeconds() {
    return QDateTime::currentMSecsSinceEpoch() / 1000;
}

QString percentText(double fraction) {
    return QString::number(qRound(fraction * 100)) + "%";
}

QString label(const BookRow &row) {
    if (!row.title.isEmpty())
        return row.title;
    const QString file = QFileInfo(row.filePath()).fileName();
    return file.isEmpty() ? row.contentId : file;
}

}

Controller::Controller(QObject *parent) : QObject(parent), client_(this) {
}

// ---------------------------------------------------------------------------
// start-up

void Controller::start() {
    log(QString("starting (version %1)").arg(NH_VERSION));
    QDir().mkpath(configDir());
    loadState();
    loadConfig();

    watcher_ = new QFileSystemWatcher(this);
    watcher_->addPath(configDir());
    if (kobalt::installed())
        watcher_->addPath(kobalt::directory());
    connect(watcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        if (path == kobalt::directory())
            onKobaltDirectoryChanged();
        else
            onConfigDirectoryChanged();
    });
    configModified_ = QFileInfo(configFile).lastModified();
    kobaltMarker_ = QFile::exists(kobaltMarkerFile);
    kobaltBook_ = kobaltMarker_ ? kobalt::activeBook() : QString();
    readingView_ = nickeldb::readingViewActive();
    wasAsleep_ = !awake();

    installTouchInput(this);
    tickTimer_ = new QTimer(this);
    connect(tickTimer_, &QTimer::timeout, this, &Controller::tick);
    tickTimer_->start(tickMs);
    networkTimer_ = new QTimer(this);
    connect(networkTimer_, &QTimer::timeout, this, &Controller::networkTick);
    networkTimer_->start(networkMs);

    const nickel::Api &api = nickel::api();
    if (api.syncManager) {
        if (QObject *sync = api.syncManager())
            connect(sync, SIGNAL(finished()), this, SLOT(onNickelSyncFinished()));
    }
    if (!api.volumeApi())
        log("Nickel's volume API was not found: positions from other devices will reach Kobalt only");

    // Books read while the mod was not running (first start, or a book open at
    // power-off) have changed without a close event.
    foreach (const BookRow &row, nickeldb::recentBooks(config_.recentBooks)) {
        if (currentSeen(row) != bookState(row.contentId).value("seen").toObject()) {
            QJsonObject book = bookState(row.contentId);
            const kobalt::State state = kobalt::readState(row.contentId);
            book["changed_at"] = double(qMax(row.modifiedMs(), state.modifiedMs) / 1000);
            book["seen"] = currentSeen(row);
            setBookState(row.contentId, book);
        }
    }
    // The recently read books at start; the whole library only on request
    // from the KOSync screen, as it takes a few seconds per book.
    enqueueRecent("start");
}

void Controller::loadConfig() {
    QString problem;
    const bool present = readConfig(configFile, &config_, &problem);
    configOk_ = present && config_.complete();
    setVerboseLogging(config_.verbose);
    authFailed_ = false;
    authChecked_ = false;
    if (!configOk_) {
        log("not configured: " + problem + " (edit " + QString(configFile) + ")");
        return;
    }
    writeNickelMenu();
    if (dialog_)
        dialog_->refresh(config_);
    client_.configure(config_.url, config_.username, config_.userKey, config_.insecure, config_.caFile);
    log(QString("configured: server %1, user %2, document id by %3, pull %4, Wi-Fi %5")
            .arg(config_.url, config_.username, config_.documentId == Config::DocumentFilename ? "filename" : "content",
                 config_.pull == Config::PullOff ? "off" : config_.pull == Config::PullForward ? "forward only" : "newest wins",
                 config_.connectWifi ? "connect when needed" : "leave alone"));
}

// NickelMenu reads every file in .adds/nm; this one is ours.
void Controller::writeNickelMenu() {
    const QString directory = "/mnt/onboard/.adds/nm";
    const QString path = directory + "/kosync";
    if (!QFileInfo(directory).isDir())
        return;
    QString content;
    if (config_.menu) {
        content += "# Written by NickelKoSync from config.ini ([options] menu, menu_log); edits are overwritten.\n";
        content += "menu_item :main :KOSync :cmd_spawn :quiet :touch " KS_CONFIG_DIR "/settings\n";
        if (config_.menuLog)
            content += "menu_item :main :KOSync log :cmd_output :5000 :tail -n 12 " KS_CONFIG_DIR "/log.txt\n";
    }
    QFile existing(path);
    if (content.isEmpty()) {
        if (existing.exists() && existing.remove())
            log("removed the NickelMenu entries");
        return;
    }
    if (existing.open(QIODevice::ReadOnly) && QString::fromUtf8(existing.readAll()) == content)
        return;
    existing.close();
    QSaveFile file(path);
    const QByteArray bytes = content.toUtf8();
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit())
        log("wrote the NickelMenu entries to " + path);
    else
        log("cannot write " + path);
}

void Controller::loadState() {
    state_ = QJsonObject();
    QFile file(stateFile);
    if (file.open(QIODevice::ReadOnly) && file.size() < 1024 * 1024) {
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
        if (document.isObject())
            state_ = document.object();
    }
    deviceId_ = state_.value("device_id").toString();
    if (deviceId_.size() != 32) {
        deviceId_ = QUuid::createUuid().toString().remove('{').remove('}').remove('-').toUpper();
        state_["device_id"] = deviceId_;
        saveState();
    }
    queue_.clear();
    foreach (const QJsonValue &value, state_.value("queue").toArray())
        if (!value.toString().isEmpty())
            queue_.append(value.toString());
}

void Controller::saveState() {
    QJsonArray queue;
    foreach (const QString &id, queue_)
        queue.append(id);
    state_["queue"] = queue;
    QSaveFile file(stateFile);
    const QByteArray bytes = QJsonDocument(state_).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        log("cannot write state.json");
}

QJsonObject Controller::bookState(const QString &contentId) const {
    return state_.value("books").toObject().value(contentId).toObject();
}

void Controller::setBookState(const QString &contentId, const QJsonObject &state) {
    QJsonObject books = state_.value("books").toObject();
    books[contentId] = state;
    // Keep the file small: forget books that have not been seen for a while.
    if (books.size() > 50) {
        QString oldest;
        double oldestTime = 0;
        for (QJsonObject::const_iterator it = books.constBegin(); it != books.constEnd(); ++it) {
            const double time = it.value().toObject().value("changed_at").toDouble();
            if (oldest.isEmpty() || time < oldestTime) {
                oldest = it.key();
                oldestTime = time;
            }
        }
        books.remove(oldest);
    }
    state_["books"] = books;
    saveState();
}

// ---------------------------------------------------------------------------
// watching the readers

void Controller::onKobaltDirectoryChanged() {
    const bool marker = QFile::exists(kobaltMarkerFile);
    if (marker && !kobaltMarker_) {
        kobaltBook_ = kobalt::activeBook();
        debug("Kobalt opened " + kobaltBook_);
        onBookOpened(kobaltBook_, "Kobalt");
    } else if (!marker && kobaltMarker_) {
        const QString book = kobaltBook_;
        kobaltBook_.clear();
        if (!book.isEmpty()) {
            debug("Kobalt closed " + book);
            pendingCloses_[book] = QDateTime::currentMSecsSinceEpoch();
        }
    } else if (marker) {
        const QString book = kobalt::activeBook();
        if (!book.isEmpty())
            kobaltBook_ = book;
    }
    kobaltMarker_ = marker;
}

void Controller::onConfigDirectoryChanged() {
    if (QFile::exists(syncNowFile)) {
        QFile::remove(syncNowFile);
        log("sync requested");
        lastWifiRequest_ = 0;
        enqueueRecent("sync-now");
        if (!online())
            requestWifi();
    }
    const QDateTime modified = QFileInfo(configFile).lastModified();
    if (modified != configModified_) {
        if (configModified_.isValid())
            log("config.ini changed, reloading");
        configModified_ = modified;
        loadConfig();
    }
}

void Controller::tick() {
    // A USB session unmounts the storage, which silently drops the folder
    // watches. Re-add them and check the files they covered.
    if (watcher_) {
        const QStringList watched = watcher_->directories();
        if (!watched.contains(configDir()) && QFileInfo(configDir()).isDir()) {
            watcher_->addPath(configDir());
            onConfigDirectoryChanged();
        }
        if (!watched.contains(kobalt::directory()) && kobalt::installed())
            watcher_->addPath(kobalt::directory());
    }
    if (QFile::exists(syncNowFile) || QFileInfo(configFile).lastModified() != configModified_)
        onConfigDirectoryChanged();
    if (QFile::exists(settingsFile)) {
        QFile::remove(settingsFile);
        showSettings();
    }
    const bool asleep = !awake();
    if (!wasAsleep_ && asleep) {
        // Kobalt saves its position as the device goes to sleep but writes
        // Nickel's library only on close; send it now, while Wi-Fi may be up.
        const QString book = openBook();
        if (!book.isEmpty()) {
            debug("going to sleep with " + book + " open");
            noteChange(book);
            enqueue(book, "sleep");
            if (online())
                continueQueue();
            else
                requestWifi(true);  // best effort: Nickel may or may not connect before it suspends
        }
    }
    if (wasAsleep_ && !asleep) {
        const QString book = openBook();
        if (!book.isEmpty()) {
            debug("woke up with " + book + " open");
            onBookOpened(book, "wake");
        }
    }
    wasAsleep_ = asleep;
    const bool reading = nickeldb::readingViewActive();
    if (reading && !readingView_) {
        const QList<BookRow> recent = nickeldb::recentBooks(1);
        if (!recent.isEmpty())
            onBookOpened(recent[0].contentId, "Nickel's reader");
    }
    if (readingView_ && !reading) {
        const QList<BookRow> recent = nickeldb::recentBooks(1);
        if (!recent.isEmpty()) {
            debug("Nickel's reader closed " + recent[0].contentId);
            pendingCloses_[recent[0].contentId] = QDateTime::currentMSecsSinceEpoch();
        }
    }
    readingView_ = reading;
    if (QFile::exists(kobaltMarkerFile) != kobaltMarker_)
        onKobaltDirectoryChanged();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QMap<QString, qint64>::iterator it = pendingCloses_.begin();
    while (it != pendingCloses_.end()) {
        if (now - it.value() < closeSettleMs) {
            ++it;
            continue;
        }
        const QString book = it.key();
        it = pendingCloses_.erase(it);
        noteChange(book);
        enqueue(book, "book closed");
        if (!online())
            requestWifi();
    }
}

void Controller::onNickelSyncFinished() {
    debug("Nickel finished its own sync");
    enqueueRecent("Nickel sync");
}

// ---------------------------------------------------------------------------
// local positions

QJsonObject Controller::currentSeen(const BookRow &row) const {
    const kobalt::State state = kobalt::readState(row.contentId);
    QJsonObject seen;
    // Opening and closing a book changes Nickel's DateLastRead without moving
    // the position; only the position counts as a change.
    seen["chapter"] = row.chapter;
    seen["percent"] = row.percent;
    seen["kobalt"] = state.position;
    return seen;
}

void Controller::noteChange(const QString &contentId) {
    const BookRow row = nickeldb::row(contentId);
    QJsonObject book = bookState(contentId);
    const QJsonObject seen = currentSeen(row);
    if (seen == book.value("seen").toObject() && book.contains("changed_at"))
        return;
    book["changed_at"] = double(nowSeconds());
    book["seen"] = seen;
    setBookState(contentId, book);
}

QString Controller::documentId(const QString &filePath) const {
    return config_.documentId == Config::DocumentFilename ? filenameMd5(filePath) : partialMd5(filePath);
}

LocalPosition Controller::localPosition(const BookRow &row) const {
    LocalPosition position;
    const QString path = row.filePath();
    position.title = row.title;
    position.author = row.author;
    position.fileName = QFileInfo(path).fileName();
    position.finished = row.status == 2;
    const kobalt::State state = kobalt::readState(row.contentId);
    const qint64 rowMs = row.modifiedMs();
    const QJsonObject book = bookState(row.contentId);
    position.changedAt = qint64(book.value("changed_at").toDouble());
    if (position.changedAt <= 0)
        position.changedAt = qMax(rowMs, state.modifiedMs) / 1000;

    // Kobalt's checkpoint is current when it corresponds to Nickel's row:
    // Kobalt writes the row and then records it as its baseline. Nickel's
    // reader changes the row without touching the checkpoint.
    const bool kobaltCurrent = state.exists && !state.position.isEmpty() &&
                               (rowMs == 0 || qAbs(state.nativeModifiedMs - rowMs) <= 1500);
    if (kobaltCurrent) {
        position.source = "kobalt";
        position.progress = normalizeXPointer(state.position);
        position.percentage = row.percent / 100.0;
        // A checkpoint newer than Nickel's row (sleep, or a failed write-back)
        // carries a position the row's percentage does not describe.
        const bool newerThanRow = state.modifiedMs > rowMs + 2000;
        if (newerThanRow || state.raw.value("native_writeback").toString() != "verified" || row.percent <= 0) {
            Epub epub;
            QString error;
            if (epub.open(path, &error))
                position.percentage = fractionOfXPointer(epub, state.position);
        }
        position.valid = true;
    } else if (!row.chapter.isEmpty()) {
        Epub epub;
        QString error;
        if (!epub.open(path, &error)) {
            log("cannot read " + label(row) + ": " + error);
            return position;
        }
        QString xpointer;
        if (!xpointerFromNickel(epub, row, &xpointer, &error)) {
            log("cannot map Nickel's position in " + label(row) + ": " + error);
            return position;
        }
        position.source = "nickel";
        position.progress = xpointer;
        position.percentage = row.percent / 100.0;
        position.valid = true;
    }
    if (position.finished)
        position.percentage = 1.0;
    position.percentage = qBound(0.0, position.percentage, 1.0);
    return position;
}

// ---------------------------------------------------------------------------
// the queue

void Controller::enqueue(const QString &contentId, const char *reason) {
    if (contentId.isEmpty() || queue_.contains(contentId))
        return;
    queue_.append(contentId);
    deferred_.remove(contentId);
    debug(QString("queued %1 (%2)").arg(contentId, reason));
    saveState();
}

void Controller::enqueueRecent(const char *reason) {
    foreach (const BookRow &row, nickeldb::recentBooks(config_.recentBooks))
        if (row.epub())
            enqueue(row.contentId, reason);
}

void Controller::enqueueAll(const char *reason) {
    int count = 0;
    foreach (const BookRow &row, nickeldb::allBooks(config_.maxBooks)) {
        if (!row.epub())
            continue;
        enqueue(row.contentId, reason);
        ++count;
    }
    debug(QString("checking %1 books (%2)").arg(count).arg(reason));
}

void Controller::networkTick() {
    if (!configOk_)
        return;
    const qint64 now = nowSeconds();
    if (now - lastPeriodic_ >= qint64(config_.pullIntervalMinutes) * 60 && awake() && online()) {
        lastPeriodic_ = now;
        enqueueRecent("periodic check");
    }
    if (queue_.isEmpty() || inFlight_ || now < retryAfter_)
        return;
    if (!online()) {
        if (now - lastQueueLog_ > 600) {
            debug(QString("%1 book(s) waiting for a network connection").arg(queue_.size()));
            lastQueueLog_ = now;
        }
        requestWifi();
        return;
    }
    if (authFailed_)
        return;
    if (!authChecked_) {
        authChecked_ = true;
        inFlight_ = true;
        client_.authorize([this](const Reply &reply) {
            inFlight_ = false;
            if (reply.ok()) {
                log("signed in to " + config_.url + " as " + config_.username);
            } else {
                log("sign-in failed: " + reply.error);
                toast("KOSync", "Sign-in failed: " + reply.error);
                if (reply.unauthorized())
                    authFailed_ = true;
                else
                    retryAfter_ = nowSeconds() + retryDelaySeconds;
            }
        });
        return;
    }
    processQueue();
}

void Controller::processQueue() {
    if (inFlight_ || queue_.isEmpty())
        return;
    syncBook(queue_.first());
}

void Controller::finishBook(const QString &contentId, bool success) {
    if (manualSync_)
        ++checkedSinceManual_;
    if (success) {
        queue_.removeAll(contentId);
        failures_.remove(contentId);
    } else {
        queue_.removeAll(contentId);
        if (++failures_[contentId] < maxFailures)
            queue_.append(contentId);
        else
            log("giving up on " + contentId + " after repeated failures");
        retryAfter_ = nowSeconds() + retryDelaySeconds;
    }
    saveState();
    if (queue_.isEmpty())
        flushEvents();
    else
        QTimer::singleShot(200, this, SLOT(continueQueue()));  // next book without waiting for the timer
    if (dialog_)
        dialog_->setStatus(statusText());
}

void Controller::continueQueue() {
    if (!configOk_ || authFailed_ || inFlight_ || queue_.isEmpty() || nowSeconds() < retryAfter_ || !online())
        return;
    processQueue();
}

void Controller::syncBook(const QString &contentId) {
    const BookRow row = nickeldb::row(contentId);
    const QString path = row.filePath();
    if (!row.valid || path.isEmpty() || !QFile::exists(path) || !row.epub()) {
        debug("skipping " + contentId + ": not a sideloaded EPUB on the device");
        finishBook(contentId, true);
        return;
    }
    const QString document = documentId(path);
    if (document.isEmpty()) {
        log("cannot compute the document id of " + label(row));
        finishBook(contentId, true);
        return;
    }
    const LocalPosition local = localPosition(row);
    debug(QString("checking %1: local %2 at %3 (%4), document %5")
              .arg(label(row), local.valid ? local.progress : "none", percentText(local.percentage), local.source, document));
    inFlight_ = true;
    client_.getProgress(document, [this, contentId, row, document, local](const Reply &reply, const RemoteProgress &remote) {
        inFlight_ = false;
        if (!reply.ok() && reply.status != 404) {
            if (reply.unauthorized()) {
                authFailed_ = true;
                log("the server rejected the credentials: " + reply.error);
            } else {
                log("fetching progress for " + label(row) + " failed: " + reply.error);
            }
            finishBook(contentId, false);
            return;
        }
        QJsonObject book = bookState(contentId);
        const bool remoteOther = remote.found && remote.deviceId != deviceId_ && !remote.progress.isEmpty();
        if (remote.found)
            debug(QString("remote %1 at %2 from %3 (%4)")
                      .arg(remote.progress, percentText(remote.percentage), remote.device,
                           QDateTime::fromMSecsSinceEpoch(remote.timestamp * 1000).toString(Qt::ISODate)));
        bool applied = false, remoteNewer = false;
        const bool samePlace = local.valid && paragraphKey(remote.progress) == paragraphKey(local.progress);
        // A book that has only been opened here (cover, 0%) has not been read
        // here: whatever another device stored wins, however old. Otherwise the
        // later change wins.
        const bool unread = !local.valid || local.percentage < 0.005;
        // The server holds a later position from this very device than the
        // one it has now (Kobalt reinstalled, or Nickel's reader opened a row
        // Kobalt had not written yet): the local copy regressed. Restore it
        // rather than sending the older position back.
        const bool ownRemote = remote.found && remote.deviceId == deviceId_ && !remote.progress.isEmpty();
        if (ownRemote && !samePlace && local.valid && config_.pull != Config::PullOff && remote.timestamp > 0 &&
            remote.timestamp >= local.changedAt && remote.percentage > local.percentage + 0.0001) {
            QString error;
            if (apply(row, remote, local, &error)) {
                applied = true;
                log(QString("%1: restored this device's own later position (%2, local copy had %3)")
                        .arg(label(row), percentText(remote.percentage), percentText(local.percentage)));
                noteEvent(QString("%1 \u2192 %2 (restored)").arg(label(row), percentText(remote.percentage)), true);
                book["applied"] = remote.progress;
                book["applied_timestamp"] = double(remote.timestamp);
                book["applied_location"] = appliedLocation_;
                book["pushed"] = remote.progress;
                book["pushed_percentage"] = remote.percentage;
                book["seen"] = currentSeen(nickeldb::row(contentId));
                setBookState(contentId, book);
            } else if (error == "deferred") {
                debug(label(row) + ": Nickel's reader is open, restoring later");
                deferred_.insert(contentId);
                finishBook(contentId, true);
                return;
            } else {
                log(label(row) + ": could not restore its later position: " + error);
            }
        }
        if (!applied && remoteOther && !samePlace) {
            remoteNewer = unread ? remote.percentage > local.percentage + 0.0001
                                 : (remote.timestamp > 0 ? remote.timestamp > local.changedAt
                                                         : remote.percentage > local.percentage + 0.0001);
            const bool forward = config_.pull == Config::PullNewer || remote.percentage > local.percentage + 0.0001;
            const bool alreadyApplied = book.value("applied").toString() == remote.progress &&
                                        qint64(book.value("applied_timestamp").toDouble()) == remote.timestamp;
            if (config_.pull != Config::PullOff && remoteNewer && forward && !alreadyApplied) {
                QString error;
                if (apply(row, remote, local, &error)) {
                    applied = true;
                    log(QString("%1: moved to %2 from %3").arg(label(row), percentText(remote.percentage), remote.device));
                    noteEvent(QString("%1 \u2192 %2 (from %3)").arg(label(row), percentText(remote.percentage), remote.device), true);
                    book["applied"] = remote.progress;
                    book["applied_timestamp"] = double(remote.timestamp);
                    book["applied_location"] = appliedLocation_;
                    book["pushed"] = remote.progress;
                    book["pushed_percentage"] = remote.percentage;
                    book["seen"] = currentSeen(nickeldb::row(contentId));
                    setBookState(contentId, book);
                } else if (error == "deferred") {
                    debug(label(row) + ": Nickel's reader is open, trying again later");
                    deferred_.insert(contentId);
                    toast("KOSync", QString("%1: %2 is at %3. Close and reopen the book to move there.")
                                        .arg(label(row), remote.device, percentText(remote.percentage)));
                    finishBook(contentId, true);
                    return;
                } else {
                    log(label(row) + ": could not apply the position from " + remote.device + ": " + error);
                }
            }
        }
        if (applied || !config_.push || !local.valid || local.progress.isEmpty()) {
            finishBook(contentId, true);
            return;
        }
        // A reader that opens an applied position reads it back from Nickel's
        // library as its own XPointer to the same paragraph. That is not a new
        // position; sending it would make the other device move again.
        const QString key = paragraphKey(local.progress);
        const bool samePushed = paragraphKey(book.value("pushed").toString()) == key;
        const bool sameRemote = remote.found && paragraphKey(remote.progress) == key;
        const bool sameApplied = !book.value("applied").toString().isEmpty() &&
                                 paragraphKey(book.value("applied").toString()) == key;
        if ((samePushed || sameRemote || sameApplied) && local.progress != book.value("pushed").toString())
            debug(label(row) + ": same paragraph as the last exchange; not sending it again");
        if (samePushed || sameRemote || sameApplied) {
            if (!samePushed) {
                book["pushed"] = local.progress;
                book["pushed_percentage"] = local.percentage;
                setBookState(contentId, book);
            }
            finishBook(contentId, true);
            return;
        }
        if (remoteOther && remoteNewer) {
            debug(label(row) + ": the server has a newer position that was not applied; not pushing over it");
            finishBook(contentId, true);
            return;
        }
        QJsonObject metadata;
        if (config_.sendMetadata) {
            metadata["filename"] = local.fileName;
            metadata["title"] = local.title;
            metadata["authors"] = local.author;
        }
        inFlight_ = true;
        client_.updateProgress(document, local.progress, local.percentage, config_.deviceName, deviceId_, metadata,
                               [this, contentId, row, local](const Reply &reply) {
                                   inFlight_ = false;
                                   if (reply.ok()) {
                                       QJsonObject book = bookState(contentId);
                                       book["pushed"] = local.progress;
                                       book["pushed_percentage"] = local.percentage;
                                       book["pushed_at"] = double(nowSeconds());
                                       setBookState(contentId, book);
                                       log(QString("%1: pushed %2 (%3)").arg(label(row), percentText(local.percentage), local.source));
                                       noteEvent(QString("%1: sent %2").arg(label(row), percentText(local.percentage)));
                                       finishBook(contentId, true);
                                   } else {
                                       if (reply.unauthorized())
                                           authFailed_ = true;
                                       log(label(row) + ": push failed: " + reply.error);
                                       finishBook(contentId, false);
                                   }
                               });
    });
}

// ---------------------------------------------------------------------------
// applying a remote position

bool Controller::apply(const BookRow &row, const RemoteProgress &remote, const LocalPosition &local, QString *error) {
    Q_UNUSED(local);
    if (nickeldb::readingViewActive()) {
        *error = "deferred";
        return false;
    }
    const bool kobaltOpen = QFile::exists(kobaltMarkerFile) && kobalt::activeBook() == row.contentId;
    Epub epub;
    if (!epub.open(row.filePath(), error))
        return false;
    QString location;
    if (!nickelFromXPointer(epub, row, remote.progress, &location, error))
        return false;
    const bool finished = remote.percentage >= 0.999;
    bool stored = false;
    QString nickelError;
    appliedLocation_ = nickeldb::sharedLocation(location);
    if (nickeldb::writePosition(row.contentId, location, remote.percentage * 100.0, finished, &nickelError)) {
        stored = true;
        debug("stored " + location + " in Nickel's library");
    } else {
        debug("Nickel's library was not updated: " + nickelError);
    }
    if (kobaltOpen) {
        if (!stored) {
            *error = "Kobalt has the book open and Nickel's library could not be updated (" + nickelError + ")";
            return false;
        }
        log(label(row) + ": Kobalt has the book open; it will offer the new position");
        return true;
    }
    const kobalt::State state = kobalt::readState(row.contentId);
    if (state.exists) {
        const NativeSnapshot snapshot = nickeldb::snapshot(nickeldb::row(row.contentId));
        QString kobaltError;
        if (kobalt::writePosition(row.contentId, remote.progress, snapshot, &kobaltError)) {
            debug("stored " + remote.progress + " for Kobalt");
            return true;
        }
        if (!stored) {
            *error = kobaltError + "; " + nickelError;
            return false;
        }
        log(label(row) + ": Kobalt's saved state was not updated: " + kobaltError);
        return true;
    }
    if (!stored) {
        *error = nickelError;
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// the screen, notifications, open books

QString Controller::openBook() const {
    if (QFile::exists(kobaltMarkerFile)) {
        const QString book = kobalt::activeBook();
        if (!book.isEmpty())
            return book;
    }
    if (nickeldb::readingViewActive()) {
        const QList<BookRow> recent = nickeldb::recentBooks(1);
        if (!recent.isEmpty())
            return recent[0].contentId;
    }
    return QString();
}

void Controller::onBookOpened(const QString &contentId, const char *reader) {
    if (!config_.onOpen || contentId.isEmpty() || !configOk_)
        return;
    // A newer position elsewhere reaches Nickel's library; Kobalt then offers
    // it within a minute. Nickel's reader only benefits at its next opening.
    enqueue(contentId, reader);
    if (!online())
        requestWifi();
}

QString Controller::statusText() const {
    QString text;
    if (!configOk_)
        text = "Not configured: edit .adds/kosync/config.ini";
    else
        text = config_.username + " at " + config_.url;
    text += "\n";
    text += online() ? "Online. " : "Offline. ";
    if (!queue_.isEmpty())
        text += QString("%1 book(s) waiting. ").arg(queue_.size());
    if (!lastEvent_.isEmpty())
        text += "Last: " + lastEvent_;
    return text;
}

void Controller::showSettings() {
    if (dialog_) {
        dialog_->raise();
        return;
    }
    QWidget *parent = QApplication::activeWindow();
    SettingsDialog *dialog = new SettingsDialog(config_, parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->onSet = [this](const char *section, const char *key, const QString &value) {
        if (!setConfigValue(configFile, section, key, value))
            log(QString("cannot write %1 to config.ini").arg(key));
        else
            debug(QString("config.ini: %1 = %2").arg(key, value));
    };
    dialog->onSync = [this, dialog](bool wholeLibrary) {
        lastWifiRequest_ = 0;
        manualSync_ = true;
        checkedSinceManual_ = 0;
        if (wholeLibrary)
            enqueueAll("screen");
        else
            enqueueRecent("screen");
        if (queue_.isEmpty()) {
            manualSync_ = false;
            lastEvent_ = "No books to check";
            dialog->setStatus(statusText());
            toast("KOSync", "No books to check");
            return;
        }
        if (!online())
            requestWifi();
        dialog->setStatus(statusText());
        toast("KOSync", online() ? QString::fromUtf8("Checking %1 book(s)\u2026").arg(queue_.size())
                                 : QString::fromUtf8("Connecting to Wi-Fi\u2026"));
    };
    dialog->setStatus(statusText());
    dialog_ = dialog;
    dialog->show();
}

void Controller::toast(const QString &title, const QString &body) {
    if (!config_.notify)
        return;
    // The mod's own notice: it goes away when tapped, or after a few seconds.
    Notice::show(title, body, 7000);
}

void Controller::noteEvent(const QString &text, bool important) {
    events_.append(text);
    lastEvent_ = text;
    if (important)
        importantEvents_ = true;
}

void Controller::flushEvents() {
    const bool manual = manualSync_;
    manualSync_ = false;
    // While a book is open, a background run that only sent positions stays
    // quiet; a position moved by another device, or a sync from the screen,
    // is still shown.
    const bool important = importantEvents_;
    importantEvents_ = false;
    if (!manual && !important && !openBook().isEmpty()) {
        events_.clear();
        return;
    }
    if (events_.isEmpty()) {
        // Only a sync started from the screen reports that nothing changed.
        if (manual) {
            lastEvent_ = QString("Checked %1 book(s), nothing to sync").arg(checkedSinceManual_);
            toast("KOSync", lastEvent_);
        }
        return;
    }
    const int shown = manual ? maxToastLines - 1 : maxToastLines;
    QStringList lines = events_.mid(0, shown);
    if (events_.size() > shown)
        lines.append(QString("and %1 more").arg(events_.size() - shown));
    if (manual)
        lines.prepend(QString("Checked %1 book(s)").arg(checkedSinceManual_));
    toast("KOSync", lines.join("\n"));
    events_.clear();
}

// ---------------------------------------------------------------------------
// device state

bool Controller::online() const {
    QFile route("/proc/net/route");
    if (!route.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    route.readLine();  // header
    while (!route.atEnd()) {
        const QList<QByteArray> fields = route.readLine().simplified().split(' ');
        if (fields.size() < 4)
            continue;
        const bool gateway = fields[3].toInt(0, 16) & 2;  // RTF_GATEWAY
        if (fields[1] == "00000000" && gateway)
            return true;
    }
    return false;
}

bool Controller::awake() const {
    const nickel::Api &api = nickel::api();
    if (!api.powerManager || !api.sleeping)
        return true;
    const void *manager = api.powerManager();
    return !manager || !api.sleeping(manager);
}

void Controller::requestWifi(bool force) {
    if (!config_.connectWifi || (!force && !awake()))
        return;
    const qint64 now = nowSeconds();
    if (!force && now - lastWifiRequest_ < qint64(config_.connectCooldownMinutes) * 60)
        return;
    lastWifiRequest_ = now;
    const nickel::Api &api = nickel::api();
    if (!api.wirelessManager || !api.connectWirelessSilently) {
        debug("Nickel's wireless manager is not available; waiting for Wi-Fi");
        return;
    }
    void *manager = api.wirelessManager();
    if (!manager)
        return;
    log("asking Nickel to connect to Wi-Fi");
    api.connectWirelessSilently(manager);
}

}
