async function fetchReleases() {
    const repo = 'taisho-guy/Rina';
    const tagsUrl = `https://codeberg.org/api/v1/repos/${repo}/tags?limit=11`;
    const releasesUrl = `https://codeberg.org/api/v1/repos/${repo}/releases?limit=10`;
    const tbody = document.getElementById('release-list');

    try {
        const [tagsRes, releasesRes] = await Promise.all([
            fetch(tagsUrl),
            fetch(releasesUrl)
        ]);

        if (!tagsRes.ok) throw new Error('Tags API fetch failed');
        const tags = await tagsRes.json();
        
        const releaseMap = {};
        if (releasesRes.ok) {
            const releases = await releasesRes.json();
            releases.forEach(r => { releaseMap[r.tag_name] = r.assets; });
        }

        if (!tags || tags.length === 0) {
            tbody.innerHTML = '<tr><td colspan="5" style="text-align: center;">公開されているリリースがありません。</td></tr>';
            return;
        }

        tbody.innerHTML = '';
        tags.slice(0, 10).forEach((tagObj, index) => {
            const tag = tagObj.name;
            const prevTag = tags[index + 1] ? tags[index + 1].name : null;
            const assets = releaseMap[tag] || [];
            
            const tr = document.createElement('tr');

            const tdTag = document.createElement('td');
            tdTag.innerHTML = `<b>${tag}</b>`;

            const tdCompare = document.createElement('td');
            const aComp = document.createElement('a');
            aComp.href = prevTag 
                ? `https://codeberg.org/taisho-guy/Rina/compare/${prevTag}...${tag}`
                : `https://codeberg.org/taisho-guy/Rina/releases/tag/${tag}`;
            aComp.textContent = prevTag ? `${prevTag}...${tag}` : tag;
            
            tdCompare.appendChild(aComp);

            const tdBinary = document.createElement('td');
            if (assets.length > 0) {
                assets.forEach(asset => {
                    const a = document.createElement('a');
                    a.href = `https://codeberg.org/taisho-guy/Rina/releases/download/${tag}/${asset.name}`;
                    a.textContent = asset.name;
                    tdBinary.appendChild(a);
                    tdBinary.appendChild(document.createElement('br'));
                });
            } else {
                tdBinary.innerHTML = '<span style="color: #666; font-size: 0.8em;">(ビルドなし)</span>';
            }

            const tdSource = document.createElement('td');
            const aSrc = document.createElement('a');
            aSrc.href = `https://codeberg.org/taisho-guy/Rina/archive/${tag}.zip`;
            aSrc.textContent = `Rina-${tag}.zip`;
            tdSource.appendChild(aSrc);

            const tdDate = document.createElement('td');
            tdDate.textContent = new Date(tagObj.commit.created).toLocaleDateString('ja-JP');

            tr.appendChild(tdTag);
            tr.appendChild(tdCompare);
            tr.appendChild(tdBinary);
            tr.appendChild(tdSource);
            tr.appendChild(tdDate);
            tbody.appendChild(tr);
        });

        // 表の最後に「過去のリリース情報」の行を追加
        const footTr = document.createElement('tr');
        footTr.innerHTML = `
            <td colspan="5" style="text-align: center; padding: 8px; vertical-align: middle;">
                <a href="https://codeberg.org/taisho-guy/Rina/releases" target="_blank" style="font-weight: bold;">
                    過去のリリース情報
                </a>
            </td>`;
        tbody.appendChild(footTr);
    } catch (error) {
        tbody.innerHTML = '<tr><td colspan="5" style="text-align: center; color: #8C1D18;">リリースの取得に失敗しました。</td></tr>';
        console.error(error);
    }
}
document.addEventListener('DOMContentLoaded', fetchReleases);

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