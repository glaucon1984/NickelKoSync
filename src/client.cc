#include "client.h"

#include "log.h"

#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QTimer>
#include <QUrl>

#ifndef NH_VERSION
#define NH_VERSION "dev"
#endif

namespace kosync {
namespace {

const int timeoutMs = 40000;

}

Client::Client(QObject *parent) : QObject(parent) {
}

void Client::configure(const QString &baseUrl, const QString &username, const QString &userKey, bool insecure,
                       const QString &caFile) {
    baseUrl_ = baseUrl;
    username_ = username;
    userKey_ = userKey;
    insecure_ = insecure;
    caFile_ = caFile;
}

QNetworkRequest Client::request(const QString &path) const {
    QNetworkRequest req(QUrl(baseUrl_ + path));
    req.setRawHeader("accept", "application/vnd.koreader.v1+json");
    req.setRawHeader("x-auth-user", username_.toUtf8());
    req.setRawHeader("x-auth-key", userKey_.toUtf8());
    req.setRawHeader("User-Agent", "NickelKoSync/" NH_VERSION);
    if (!caFile_.isEmpty()) {
        QSslConfiguration ssl = req.sslConfiguration();
        QList<QSslCertificate> certificates = ssl.caCertificates();
        certificates.append(QSslCertificate::fromPath(caFile_, QSsl::Pem));
        ssl.setCaCertificates(certificates);
        req.setSslConfiguration(ssl);
    }
    return req;
}

void Client::send(QNetworkReply *reply, const Callback &done) {
    if (insecure_)
        reply->ignoreSslErrors();
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    timer->start(timeoutMs);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        Reply result;
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray bytes = reply->readAll();
        if (reply->error() != QNetworkReply::NoError && result.status == 0)
            result.error = reply->errorString();
        const QJsonDocument document = QJsonDocument::fromJson(bytes);
        if (document.isObject())
            result.body = document.object();
        if (!result.ok() && result.error.isEmpty()) {
            result.error = QString("HTTP %1").arg(result.status);
            const QString message = result.body.value("message").toString();
            if (!message.isEmpty())
                result.error += ": " + message;
        }
        reply->deleteLater();
        done(result);
    });
}

void Client::authorize(const Callback &done) {
    send(manager_.get(request("/users/auth")), done);
}

void Client::getProgress(const QString &document, const ProgressCallback &done) {
    send(manager_.get(request("/syncs/progress/" + document)), [done](const Reply &reply) {
        RemoteProgress progress;
        if (reply.ok()) {
            const QJsonObject &body = reply.body;
            progress.document = body.value("document").toString();
            progress.progress = body.value("progress").toString();
            progress.device = body.value("device").toString();
            progress.deviceId = body.value("device_id").toString();
            if (body.contains("percentage") && !body.value("percentage").isNull()) {
                progress.percentage = body.value("percentage").toDouble();
                progress.found = true;
            }
            const QJsonValue stamp = body.value("timestamp");
            progress.timestamp = stamp.isString() ? stamp.toString().toLongLong() : qint64(stamp.toDouble());
            // Some servers return milliseconds.
            if (progress.timestamp > 100000000000LL)
                progress.timestamp /= 1000;
        }
        done(reply, progress);
    });
}

void Client::updateProgress(const QString &document, const QString &progress, double percentage, const QString &device,
                            const QString &deviceId, const QJsonObject &metadata, const Callback &done) {
    QJsonObject body;
    body["document"] = document;
    body["progress"] = progress;
    body["percentage"] = percentage;
    body["device"] = device;
    body["device_id"] = deviceId;
    if (!metadata.isEmpty())
        body["metadata"] = metadata;
    QNetworkRequest req = request("/syncs/progress");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    send(manager_.put(req, QJsonDocument(body).toJson(QJsonDocument::Compact)), done);
}

}
