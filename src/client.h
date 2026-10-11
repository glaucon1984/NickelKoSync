// HTTP client for the KOReader sync API (koreader-sync-server and compatible
// servers such as crosspoint-sync):
//   GET /users/auth                     check the credentials
//   GET /syncs/progress/:document       fetch a document's progress
//   PUT /syncs/progress                 store a document's progress
// Authentication is the x-auth-user / x-auth-key (MD5 of the password) pair.
#ifndef KOSYNC_CLIENT_H
#define KOSYNC_CLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

#include <functional>

namespace kosync {

struct RemoteProgress {
    QString document, progress, device, deviceId;
    double percentage = -1;   // 0..1, -1 when the server has no progress
    qint64 timestamp = 0;     // seconds since the epoch, 0 when not given
    bool found = false;
};

struct Reply {
    int status = 0;           // HTTP status, 0 for a transport failure
    QString error;            // empty on success
    QJsonObject body;
    bool ok() const { return status >= 200 && status < 300; }
    bool unauthorized() const { return status == 401 || status == 403; }
};

class Client : public QObject {
    Q_OBJECT
public:
    explicit Client(QObject *parent = 0);
    void configure(const QString &baseUrl, const QString &username, const QString &userKey, bool insecure,
                   const QString &caFile);

    typedef std::function<void(const Reply &)> Callback;
    typedef std::function<void(const Reply &, const RemoteProgress &)> ProgressCallback;

    void authorize(const Callback &done);
    void getProgress(const QString &document, const ProgressCallback &done);
    // metadata may be empty; it is sent as KOReader's optional metadata object.
    void updateProgress(const QString &document, const QString &progress, double percentage, const QString &device,
                        const QString &deviceId, const QJsonObject &metadata, const Callback &done);

private:
    QNetworkRequest request(const QString &path) const;
    void send(QNetworkReply *reply, const Callback &done);

    QNetworkAccessManager manager_;
    QString baseUrl_, username_, userKey_;
    bool insecure_ = false;
    QString caFile_;
};

}

#endif
