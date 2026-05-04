async function fetchReleases() {
    const repo = 'taisho-guy/AviQtl';
    const releasesUrl = `https://codeberg.org/api/v1/repos/${repo}/releases?limit=2`;
    
    const infoEl = document.getElementById('download-info-row');
    const sourceEl = document.getElementById('link-source');

    const platformMap = [
        { id: 'link-linux', patterns: ['linux'] },
        { id: 'link-windows', patterns: ['win'] },
        { id: 'link-apple', patterns: ['mac', 'apple', 'darwin'] }
    ];

    try {
        const res = await fetch(releasesUrl);
        if (!res.ok) throw new Error('API request failed');
        
        const releases = await res.json();
        if (!releases || releases.length === 0) throw new Error('No releases found');

        const latest = releases[0];
        const prev = releases[1];
        const tag = latest.tag_name;
        const assets = latest.assets;

        const findAsset = (patterns) => assets.find(a => patterns.some(p => a.name.toLowerCase().includes(p)));
        
        const updateItem = (el, asset) => {
            if (!el) return;
            const icon = el.querySelector('.download-icon');
            if (asset) {
                el.href = `https://codeberg.org/${repo}/releases/download/${tag}/${asset.name}`;
                if (icon) icon.classList.remove('is-missing');
            } else {
                el.href = "#";
                el.style.pointerEvents = 'none';
                el.classList.add('is-disabled');
                if (icon) icon.classList.add('is-missing');
            }
        };

        platformMap.forEach(platform => {
            updateItem(document.getElementById(platform.id), findAsset(platform.patterns));
        });

        if (sourceEl) {
            sourceEl.href = `https://codeberg.org/${repo}/archive/${tag}.zip`;
        }

        // 情報バーの更新
        if (infoEl) {
            const date = new Date(latest.created_at).toLocaleDateString('ja-JP');
            const compareUrl = prev 
                ? `https://codeberg.org/${repo}/compare/${prev.tag_name}...${tag}`
                : `https://codeberg.org/${repo}/releases/tag/${tag}`;
                
            const pastUrl = `https://codeberg.org/${repo}/releases`;

            infoEl.innerHTML = `${tag} (${date}) / <a href="${compareUrl}" target="_blank">更新内容</a> / <a href="${pastUrl}" target="_blank">過去のリリース</a>`;
        }

    } catch (error) {
        console.error(error);
        if (infoEl) infoEl.textContent = 'リリースの取得に失敗しました。';
    } finally {
        document.querySelectorAll('.download-icon').forEach(el => el.classList.remove('is-loading'));
    }
}
fetchReleases();

(function() {
    document.addEventListener('click', function(e) {
        if (e.target.tagName === 'CODE') {
            const text = e.target.textContent;
            navigator.clipboard.writeText(text).then(() => {
                e.target.style.color = 'var(--bg-color)';
                e.target.style.background = 'var(--accent-color)';
                setTimeout(() => {
                    e.target.style.color = '';
                    e.target.style.background = '';
                }, 200);
            });
        }
    });
})();