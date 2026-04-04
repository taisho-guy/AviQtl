#include "update_checker.hpp"
#include "settings_manager.hpp"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <algorithm>
#include <array>

namespace Rina::Core {

static constexpr int CHECK_INTERVAL_HOURS = 24;

static const QString KEY_LAST_CHECK = QStringLiteral("update/lastCheckTime");
static const QString KEY_LAST_KNOWN = QStringLiteral("update/lastKnownRelease");
static const QString KEY_ETAG = QStringLiteral("update/etag");
static const QString API_URL = QStringLiteral("https://codeberg.org/api/v1/repos/taisho-guy/Rina/releases/latest");

UpdateChecker *UpdateChecker::sInstance = nullptr;

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent), mNam(new QNetworkAccessManager(this)) {
    sInstance = this;
    connect(mNam, &QNetworkAccessManager::finished, this, &UpdateChecker::onReplyFinished);
}

auto UpdateChecker::instance() -> UpdateChecker * { return sInstance; }

void UpdateChecker::checkOnStartup() {
    const QDateTime lastCheck = SettingsManager::instance().value(KEY_LAST_CHECK).toDateTime();

    // 24時間以内にチェック済みならスキップ（Codebergへの負荷軽減）
    if (lastCheck.isValid() && lastCheck.secsTo(QDateTime::currentDateTimeUtc()) < CHECK_INTERVAL_HOURS * 3600) {
        return;
    }
    doCheck();
}

void UpdateChecker::forceCheck() { doCheck(); }

void UpdateChecker::acknowledge() {
    // ユーザーが通知を確認 → 現バージョンを既知として保存し通知を消す
    SettingsManager::instance().setValue(KEY_LAST_KNOWN, mLatestVersion);
    mUpdateAvailable = false;
    emit updateAvailableChanged();
}

void UpdateChecker::doCheck() {
    if (mChecking)
        return;
    mChecking = true;
    emit checkingChanged();

    const QString savedEtag = SettingsManager::instance().value(KEY_ETAG).toString();

    // 最頻発解析 (vexing parse) を避けるためブレース初期化を使用する
    const QUrl url{API_URL};
    QNetworkRequest req{url};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Rina-UpdateChecker/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    // ETag キャッシュ：変化なければサーバーは 304 を返しボディ転送ゼロ
    if (!savedEtag.isEmpty())
        req.setRawHeader("If-None-Match", savedEtag.toUtf8());

    mNam->get(req);
}

void UpdateChecker::onReplyFinished(QNetworkReply *reply) {
    reply->deleteLater();
    mChecking = false;
    emit checkingChanged();

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // 304 Not Modified：変化なし → lastCheckTime だけ更新して終了
    if (status == 304) {
        SettingsManager::instance().setValue(KEY_LAST_CHECK, QDateTime::currentDateTimeUtc());
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        // ネットワークエラーは静かに無視（オフライン環境を考慮）
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    if (doc.isNull() || !doc.isObject())
        return;

    const QJsonObject obj = doc.object();
    const QString remoteTag = obj.value(QStringLiteral("tag_name")).toString().trimmed();
    if (remoteTag.isEmpty())
        return;

    // ETag と lastCheckTime を保存（次回の 304 活用のため）
    const QString newEtag = QString::fromUtf8(reply->rawHeader("ETag"));
    if (!newEtag.isEmpty())
        SettingsManager::instance().setValue(KEY_ETAG, newEtag);
    SettingsManager::instance().setValue(KEY_LAST_CHECK, QDateTime::currentDateTimeUtc());

    mLatestVersion = remoteTag;
    mReleaseUrl = obj.value(QStringLiteral("html_url")).toString();
    mReleaseNotes = obj.value(QStringLiteral("body")).toString();

    const QString lastKnown = SettingsManager::instance().value(KEY_LAST_KNOWN).toString();

    if (lastKnown.isEmpty()) {
        // 初回起動：保存のみ、通知なし（現バージョン = 最新と見なす）
        SettingsManager::instance().setValue(KEY_LAST_KNOWN, remoteTag);
        return;
    }

    if (isNewer(remoteTag, lastKnown)) {
        mUpdateAvailable = true;
        emit updateAvailableChanged();
        // acknowledge() が呼ばれるまで lastKnownRelease は更新しない
    }
}

// セマンティックバージョン比較（x.x.x 形式）：remote > stored なら true
auto UpdateChecker::isNewer(const QString &remote, const QString &stored) -> bool {
    const auto parse = [](const QString &v) -> std::array<int, 3> {
        const QStringList parts = v.split(QLatin1Char('.'));
        std::array<int, 3> r{0, 0, 0};
        // std::min の型不一致を避けるため明示的に size_t にキャストする
        const auto count = std::min(r.size(), static_cast<std::size_t>(parts.size()));
        for (std::size_t i = 0; i < count; ++i)
            r[i] = parts[static_cast<int>(i)].toInt();
        return r;
    };
    const auto ra = parse(remote);
    const auto sa = parse(stored);
    for (std::size_t i = 0; i < 3; ++i) {
        if (ra[i] > sa[i])
            return true;
        if (ra[i] < sa[i])
            return false;
    }
    return false;
}

} // namespace Rina::Core
