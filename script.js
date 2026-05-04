// DOMContentLoadedを待たずに即時実行、ただし非同期
const initReleases = async () => {
    // DOMアクセスを最小限にするため、一度だけ取得
    const downloadItems = document.querySelectorAll('.download-item[data-platform]');
    const REPO = 'taisho-guy/AviQtl';
    const PATTERNS = { linux: ['linux'], windows: ['win'], apple: ['mac', 'apple', 'darwin'] };
    const info = document.getElementById('download-info-row');

    try {
        const res = await fetch(`https://codeberg.org/api/v1/repos/${REPO}/releases?limit=2`);
        const [latest, prev] = await res.json();
        if (!latest) throw 0;

        downloadItems.forEach(el => {
            const p = el.dataset.platform;
            const icon = el.querySelector('.download-icon');
            
            if (p === 'source') return el.href = `https://codeberg.org/${REPO}/archive/${latest.tag_name}.zip`;

            const asset = latest.assets.find(a => PATTERNS[p]?.some(s => a.name.toLowerCase().includes(s)));
            if (asset) {
                el.href = `https://codeberg.org/${REPO}/releases/download/${latest.tag_name}/${asset.name}`;
                icon?.classList.remove('is-missing');
            } else {
                el.removeAttribute('href');
                el.classList.add('is-disabled');
                icon?.classList.add('is-missing');
            }
        });

        if (info) {
            const date = new Date(latest.created_at).toLocaleDateString('ja-JP');
            const comp = prev ? `compare/${prev.tag_name}...${latest.tag_name}` : `releases/tag/${latest.tag_name}`;
            info.innerHTML = `${latest.tag_name} (${date}) / <a href="https://codeberg.org/${REPO}/${comp}" target="_blank">更新内容</a> / <a href="https://codeberg.org/${REPO}/releases" target="_blank">過去のバージョン</a>`;
        }
    } catch {
        if (info) info.textContent = 'APIエラー';
    } finally {
        document.querySelectorAll('.download-icon').forEach(i => i.classList.remove('is-loading'));
    }
};

// requestIdleCallbackでブラウザの暇な時間に初期化
if ('requestIdleCallback' in window) { requestIdleCallback(initReleases); }
else { setTimeout(initReleases, 1); }

document.addEventListener('click', ({target}) => {
    if (target.tagName === 'CODE') {
        navigator.clipboard.writeText(target.textContent).then(() => {
            const old = target.style.cssText;
            target.style.cssText = 'color:var(--bg);background:var(--accent)';
            setTimeout(() => target.style.cssText = old, 200);
        });
    }
});