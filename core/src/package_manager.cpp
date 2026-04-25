#include "package_manager.hpp"
#include <QTimer>

namespace AviQtl::Core {

PackageManager &PackageManager::instance() {
    static PackageManager instance;
    return instance;
}

PackageManager::PackageManager(QObject *parent) : QObject(parent) { m_statusText = tr("待機中"); }

void PackageManager::setBusy(bool busy) {
    if (m_isBusy == busy)
        return;
    m_isBusy = busy;
    emit isBusyChanged();
}

void PackageManager::setStatus(const QString &status) {
    if (m_statusText == status)
        return;
    m_statusText = status;
    emit statusTextChanged();
}

void PackageManager::setProgress(double p) {
    if (m_progress == p)
        return;
    m_progress = p;
    emit progressChanged();
}

void PackageManager::refreshRepositories() {
    if (m_isBusy)
        return;
    setBusy(true);
    setStatus(tr("リポジトリを同期中..."));
    setProgress(0.0);

    // TODO: QNetworkAccessManager による libresoft, freeware, shareware.json の取得
    // モック実装として少し待って完了とする
    QTimer::singleShot(1000, this, [this]() {
        setProgress(1.0);
        setStatus(tr("同期完了"));
        setBusy(false);
        emit repositoryRefreshed();
    });
}

void PackageManager::installPackage(const QString &packageId) {
    if (m_isBusy)
        return;
    setBusy(true);
    setStatus(tr("パッケージのインストール中: %1").arg(packageId));
    setProgress(0.0);

    // TODO: ダウンロードと依存解決、SHA256検証、ZIP展開
    QTimer::singleShot(1500, this, [this, packageId]() {
        setProgress(1.0);
        setStatus(tr("インストール完了: %1").arg(packageId));
        setBusy(false);
        emit packageInstalled(packageId);
    });
}

void PackageManager::removePackage(const QString &packageId) {
    // TODO: ローカルファイル削除
}

QVariantList PackageManager::searchPackages(const QString &query) const {
    // TODO: インメモリのDBから検索
    return {};
}

QVariantList PackageManager::getInstalledPackages() const {
    // TODO: local.json からインストール済み一覧を返す
    return {};
}

} // namespace AviQtl::Core
