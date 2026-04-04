#pragma once
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

namespace Rina::Core {

class UpdateChecker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY updateAvailableChanged)

  public:
    explicit UpdateChecker(QObject *parent = nullptr);
    static UpdateChecker *instance();

    bool updateAvailable() const { return mUpdateAvailable; }
    bool checking() const { return mChecking; }
    QString latestVersion() const { return mLatestVersion; }
    QString releaseUrl() const { return mReleaseUrl; }
    QString releaseNotes() const { return mReleaseNotes; }

    // 起動時チェック（24時間制限あり）
    Q_INVOKABLE void checkOnStartup();
    // AboutWindow などから手動で強制チェック
    Q_INVOKABLE void forceCheck();
    // ポップアップ確認後に呼ぶ（lastKnownRelease を更新してポップアップを閉じる）
    Q_INVOKABLE void acknowledge();

  signals:
    void updateAvailableChanged();
    void checkingChanged();

  private slots:
    void onReplyFinished(QNetworkReply *reply);

  private:
    void doCheck();
    static bool isNewer(const QString &remote, const QString &stored);

    QNetworkAccessManager *mNam;
    bool mUpdateAvailable = false;
    bool mChecking = false;
    QString mLatestVersion;
    QString mReleaseUrl;
    QString mReleaseNotes;

    static UpdateChecker *sInstance;
};

} // namespace Rina::Core
