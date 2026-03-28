/**
 * Rina is not AviUtl - Official Site Script
 * CachyOS / Material Design 3 準拠
 */

document.addEventListener('DOMContentLoaded', () => {
    initScrollElevation();
    initAccordions();
    initSmoothScroll();
});

/**
 * スクロールに応じてヘッダーに Elevation (陰影) を付与する
 */
const initScrollElevation = () => {
    const appBar = document.querySelector('.app-bar');
    
    window.addEventListener('scroll', () => {
        if (window.scrollY > 10) {
            appBar.style.boxShadow = '0 4px 8px rgba(0, 0, 0, 0.5)';
            appBar.style.backgroundColor = 'var(--m3-surface-variant)';
        } else {
            appBar.style.boxShadow = 'none';
            appBar.style.backgroundColor = 'var(--m3-surface)';
        }
    });
};

/**
 * アコーディオン (details/summary) の排他的制御
 * 一つ開いたら他を閉じる挙動（任意）
 */
const initAccordions = () => {
    const accordions = document.querySelectorAll('.m3-accordion');
    
    accordions.forEach(el => {
        el.addEventListener('toggle', () => {
            if (el.open) {
                // 開いた時のフィードバック（必要に応じて処理を追加）
                console.log('Opened section:', el.querySelector('summary').innerText);
            }
        });
    });
};

/**
 * ページ内リンクの微調整（アンカーへのスムーズスクロール）
 */
const initSmoothScroll = () => {
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function (e) {
            e.preventDefault();
            const target = document.querySelector(this.getAttribute('href'));
            if (target) {
                target.scrollIntoView({
                    behavior: 'smooth',
                    block: 'start'
                });
            }
        });
    });
};

/**
 * コピー機能などのユーティリティ（将来の拡張用）
 * ビルドコマンドをクリックでコピーさせるなどの処理を追加可能
 */
const copyToClipboard = (text) => {
    navigator.clipboard.writeText(text).then(() => {
        // M3のSnackbarのような通知を出す処理をここに記述可能
        alert('コマンドをコピーしました');
    });
};