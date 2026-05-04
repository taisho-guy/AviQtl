async function fetchReleases() {
    const repo = 'taisho-guy/AviQtl';
    const releasesUrl = `https://codeberg.org/api/v1/repos/${repo}/releases?limit=2`;
    
    const elements = {
        linux: document.getElementById('link-linux'),
        win: document.getElementById('link-windows'),
        apple: document.getElementById('link-apple'),
        source: document.getElementById('link-source'),
        info: document.getElementById('download-info-row')
    };

    try {
        const res = await fetch(releasesUrl);
        if (!res.ok) throw new Error('API request failed');
        
        const releases = await res.json();
        if (!releases || releases.length === 0) throw new Error('No releases found');

        const latest = releases[0];
        const prev = releases[1];
        const tag = latest.tag_name;
        const assets = latest.assets;

        const findAsset = (pattern) => assets.find(a => a.name.toLowerCase().includes(pattern.toLowerCase()));
        
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

        updateItem(elements.linux, findAsset('linux'));
        updateItem(elements.win, findAsset('win'));
        updateItem(elements.apple, findAsset('mac') || findAsset('apple') || findAsset('darwin'));

        if (elements.source) {
            elements.source.href = `https://codeberg.org/${repo}/archive/${tag}.zip`;
        }

        // 情報バーの更新
        if (elements.info) {
            const date = new Date(latest.created_at).toLocaleDateString('ja-JP');
            const compareUrl = prev 
                ? `https://codeberg.org/${repo}/compare/${prev.tag_name}...${tag}`
                : `https://codeberg.org/${repo}/releases/tag/${tag}`;
                
            const pastUrl = `https://codeberg.org/${repo}/releases`;

            elements.info.innerHTML = `最新: ${tag} (${date}) / <a href="${compareUrl}" target="_blank">更新内容</a> / <a href="${pastUrl}" target="_blank">過去のリリース</a>`;
        }

    } catch (error) {
        console.error(error);
        if (elements.info) elements.info.textContent = 'リリースの取得に失敗しました。';
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
                e.target.style.color = '#0d0b14';
                e.target.style.background = '#b366ff';
                setTimeout(() => {
                    e.target.style.color = '';
                    e.target.style.background = '';
                }, 200);
            });
        }
    });
})();