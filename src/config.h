// The mod's configuration, read from config.ini in the mod's folder.
#ifndef KOSYNC_CONFIG_H
#define KOSYNC_CONFIG_H

#include <QString>

#ifndef KS_CONFIG_DIR
#define KS_CONFIG_DIR "/mnt/onboard/.adds/kosync"
#endif

namespace kosync {

struct Config {
    // [server]
    QString url;        // base URL, no trailing slash
    QString username;
    QString userKey;    // MD5 of the password, lower-case hex
    QString deviceName;
    bool insecure = false;
    QString caFile;

    // [sync]
    enum DocumentId { DocumentBinary, DocumentFilename };
    DocumentId documentId = DocumentBinary;
    enum Pull { PullNewer, PullForward, PullOff };
    Pull pull = PullNewer;
    bool push = true;
    int recentBooks = 3;
    int pullIntervalMinutes = 15;
    bool sendMetadata = false;

    // [wifi]
    bool connectWifi = true;
    int connectCooldownMinutes = 10;

    // [options]
    bool verbose = false;
    bool menu = true;      // write a "KOSync now" NickelMenu entry
    bool menuLog = false;  // also a "KOSync log" entry
    int maxBooks = 300;    // library size cap for a full check

    bool complete() const {
        return !url.isEmpty() && !username.isEmpty() && !userKey.isEmpty();
    }
};

// Reads config.ini. Returns false when the file is missing or unreadable;
// `config` then holds the defaults.
bool readConfig(const QString &path, Config *config, QString *problem);

inline QString configDir() {
    return QString::fromLatin1(KS_CONFIG_DIR);
}

}

#endif
