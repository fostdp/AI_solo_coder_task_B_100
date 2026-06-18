const ERA_STYLES = `
.ecv-container {
    font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    background: #0a0e1a;
    color: #e0e8ff;
    padding: 20px;
    min-height: 100vh;
    box-sizing: border-box;
}
.ecv-title {
    font-size: 22px;
    font-weight: 600;
    background: linear-gradient(90deg, #f84, #f48);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    letter-spacing: 3px;
    margin-bottom: 20px;
    padding-bottom: 12px;
    border-bottom: 2px solid #a64;
}
.ecv-subtitle {
    font-size: 12px;
    color: #8899bb;
    letter-spacing: 1px;
    margin-bottom: 16px;
}
.ecv-sections {
    display: grid;
    grid-template-columns: 280px 1fr;
    gap: 20px;
    margin-bottom: 20px;
}
.ecv-panel {
    background: #14182a;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 16px;
}
.ecv-panel-title {
    font-size: 13px;
    color: #fa4;
    letter-spacing: 2px;
    margin-bottom: 12px;
    padding-bottom: 8px;
    border-bottom: 1px solid #2a3050;
    text-transform: uppercase;
}
.ecv-param-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px 0;
    border-bottom: 1px dashed #222a44;
}
.ecv-param-label {
    font-size: 12px;
    color: #8899bb;
}
.ecv-param-input {
    width: 100px;
    padding: 6px 10px;
    background: #1e2544;
    border: 1px solid #3a4070;
    color: #e0e8ff;
    border-radius: 4px;
    font-size: 13px;
    font-family: "Courier New", monospace;
    outline: none;
}
.ecv-param-input:focus {
    border-color: #6af;
    box-shadow: 0 0 8px rgba(100, 170, 255, 0.3);
}
.ecv-btn {
    width: 100%;
    padding: 10px 16px;
    background: linear-gradient(90deg, #a64, #f63);
    border: none;
    color: #fff;
    border-radius: 4px;
    font-size: 13px;
    font-weight: 600;
    letter-spacing: 2px;
    cursor: pointer;
    transition: all 0.2s;
    margin-top: 12px;
}
.ecv-btn:hover {
    background: linear-gradient(90deg, #c75, #f84);
    box-shadow: 0 0 20px rgba(255, 130, 50, 0.4);
}
.ecv-btn:disabled {
    opacity: 0.5;
    cursor: not-allowed;
}
.ecv-gap-section {
    background: linear-gradient(135deg, #1a1020 0%, #201030 100%);
    border: 1px solid #4a3a60;
    border-radius: 8px;
    padding: 20px;
    margin-bottom: 20px;
}
.ecv-gap-title {
    font-size: 16px;
    color: #fa4;
    font-weight: 600;
    margin-bottom: 16px;
    letter-spacing: 1px;
}
.ecv-gap-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 16px;
}
.ecv-gap-card {
    background: rgba(20, 15, 30, 0.6);
    border: 1px solid #3a2a50;
    border-radius: 6px;
    padding: 14px;
    text-align: center;
}
.ecv-gap-label {
    font-size: 11px;
    color: #8899bb;
    letter-spacing: 1px;
    margin-bottom: 8px;
    text-transform: uppercase;
}
.ecv-gap-value {
    font-size: 24px;
    font-weight: 700;
    font-family: "Courier New", monospace;
    color: #fc8;
    text-shadow: 0 0 10px rgba(255, 200, 100, 0.5);
}
.ecv-gap-unit {
    font-size: 12px;
    color: #887;
    margin-left: 4px;
}
.ecv-summary {
    margin-top: 16px;
    padding: 12px 16px;
    background: rgba(255, 170, 50, 0.1);
    border-left: 3px solid #fa4;
    border-radius: 4px;
    font-size: 13px;
    line-height: 1.6;
    color: #fc8;
}
.ecv-timeline-section {
    background: #14182a;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 20px;
    margin-bottom: 20px;
}
.ecv-timeline-title {
    font-size: 14px;
    color: #8af;
    font-weight: 600;
    margin-bottom: 16px;
    letter-spacing: 1px;
}
.ecv-timeline {
    position: relative;
    padding-left: 30px;
}
.ecv-timeline::before {
    content: "";
    position: absolute;
    left: 10px;
    top: 0;
    bottom: 0;
    width: 2px;
    background: linear-gradient(to bottom, #fc8, #8af);
}
.ecv-timeline-item {
    position: relative;
    padding: 12px 0;
    border-bottom: 1px dashed #222a44;
}
.ecv-timeline-item::before {
    content: "";
    position: absolute;
    left: -26px;
    top: 16px;
    width: 12px;
    height: 12px;
    border-radius: 50%;
    background: #6af;
    box-shadow: 0 0 8px rgba(100, 170, 255, 0.6);
}
.ecv-timeline-year {
    font-size: 14px;
    font-weight: 600;
    color: #fc8;
    font-family: "Courier New", monospace;
    margin-bottom: 4px;
}
.ecv-timeline-name {
    font-size: 13px;
    color: #e0e8ff;
    margin-bottom: 4px;
}
.ecv-timeline-score {
    font-size: 11px;
    color: #8899bb;
}
.ecv-timeline-milestone {
    font-size: 11px;
    color: #fa4;
    margin-top: 4px;
}
.ecv-insights-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 16px;
    margin-bottom: 20px;
}
.ecv-insights-card {
    background: #14182a;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 16px;
}
.ecv-insights-title {
    font-size: 13px;
    color: #8af;
    font-weight: 600;
    margin-bottom: 12px;
    letter-spacing: 1px;
    padding-bottom: 8px;
    border-bottom: 1px solid #2a3050;
}
.ecv-insights-list {
    list-style: none;
    padding: 0;
    margin: 0;
}
.ecv-insights-list li {
    padding: 8px 0;
    font-size: 12px;
    line-height: 1.6;
    color: #aabbdd;
    border-bottom: 1px dashed #1e2440;
    padding-left: 16px;
    position: relative;
}
.ecv-insights-list li::before {
    content: "▸";
    position: absolute;
    left: 0;
    color: #6af;
}
.ecv-insights-list li:last-child {
    border-bottom: none;
}
.ecv-fun-facts {
    background: linear-gradient(135deg, #1a2010 0%, #203010 100%);
    border: 1px solid #4a5a30;
    border-radius: 6px;
    padding: 20px;
}
.ecv-fun-title {
    font-size: 14px;
    color: #8f4;
    font-weight: 600;
    margin-bottom: 12px;
    letter-spacing: 1px;
}
.ecv-fun-list {
    list-style: none;
    padding: 0;
    margin: 0;
}
.ecv-fun-list li {
    padding: 10px 0;
    font-size: 12px;
    line-height: 1.6;
    color: #afc;
    border-bottom: 1px dashed #2a3a20;
}
.ecv-fun-list li:last-child {
    border-bottom: none;
}
.ecv-loading {
    text-align: center;
    padding: 40px;
    color: #8899bb;
    font-size: 14px;
}
.ecv-loading::after {
    content: "";
    display: inline-block;
    width: 20px;
    height: 20px;
    border: 2px solid #4a6aaa;
    border-top-color: transparent;
    border-radius: 50%;
    animation: ecv-spin 1s linear infinite;
    margin-left: 10px;
    vertical-align: middle;
}
@keyframes ecv-spin {
    to { transform: rotate(360deg); }
}
.ecv-error {
    padding: 16px;
    background: rgba(255, 50, 50, 0.1);
    border: 1px solid #a44;
    border-radius: 4px;
    color: #f88;
    font-size: 13px;
}
.ecv-compare-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 16px;
}
.ecv-era-badge {
    display: inline-block;
    padding: 4px 12px;
    border-radius: 12px;
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 1px;
}
.ecv-badge-ancient {
    background: #3a2a10;
    color: #fc8;
}
.ecv-badge-modern {
    background: #1a2a40;
    color: #8af;
}
`;

class EraComparisonView {
    constructor(containerSelector, apiBase) {
        this.container = document.querySelector(containerSelector);
        this.apiBase = apiBase;
        this.rockMass = 5.0;
        this.rockVelocity = 30.0;
        this.currentResult = null;
        this.styleInjected = false;
    }

    init() {
        if (!this.styleInjected) {
            this._injectStyles();
            this.styleInjected = true;
        }
        this._render();
        this._bindEvents();
        this._loadComparison();
    }

    _injectStyles() {
        const style = document.createElement('style');
        style.textContent = ERA_STYLES;
        document.head.appendChild(style);
    }

    _render() {
        if (!this.container) return;

        this.container.innerHTML = `
            <div class="ecv-container">
                <div class="ecv-title">⏳ 跨时代防护对比</div>
                <div class="ecv-subtitle">跨越两千五百年的防护技术演进 · 古代战车 vs 现代装甲车</div>

                <div class="ecv-sections">
                    <div class="ecv-panel">
                        <div class="ecv-panel-title">冲击参数</div>
                        <div class="ecv-param-row">
                            <span class="ecv-param-label">滚石质量 (kg)</span>
                            <input type="number" class="ecv-param-input" id="ecv-rock-mass" value="${this.rockMass}" min="0.1" step="0.5">
                        </div>
                        <div class="ecv-param-row">
                            <span class="ecv-param-label">冲击速度 (m/s)</span>
                            <input type="number" class="ecv-param-input" id="ecv-rock-velocity" value="${this.rockVelocity}" min="0.1" step="1">
                        </div>
                        <button class="ecv-btn" id="ecv-btn-compare">▶ 开始对比</button>
                    </div>

                    <div class="ecv-panel" style="min-height: 200px;">
                        <div class="ecv-panel-title">对比结果</div>
                        <div id="ecv-result">
                            <div class="ecv-loading">正在加载对比数据...</div>
                        </div>
                    </div>
                </div>

                <div id="ecv-gap-section" style="display:none;">
                </div>

                <div id="ecv-timeline-section" style="display:none;">
                </div>

                <div id="ecv-insights-section" style="display:none;">
                </div>

                <div id="ecv-fun-section" style="display:none;">
                </div>
            </div>
        `;
    }

    _bindEvents() {
        const btnCompare = document.getElementById('ecv-btn-compare');
        if (btnCompare) {
            btnCompare.addEventListener('click', () => this._loadComparison());
        }

        const massInput = document.getElementById('ecv-rock-mass');
        const velocityInput = document.getElementById('ecv-rock-velocity');

        if (massInput) {
            massInput.addEventListener('change', (e) => {
                this.rockMass = parseFloat(e.target.value) || 5.0;
            });
        }
        if (velocityInput) {
            velocityInput.addEventListener('change', (e) => {
                this.rockVelocity = parseFloat(e.target.value) || 30.0;
            });
        }
    }

    async _loadComparison() {
        const resultEl = document.getElementById('ecv-result');
        if (!resultEl) return;

        resultEl.innerHTML = '<div class="ecv-loading">正在进行跨时代对比计算...</div>';

        try {
            const response = await fetch(
                `${this.apiBase}/api/era/compare?rock_mass=${this.rockMass}&rock_velocity=${this.rockVelocity}`
            );

            if (!response.ok) {
                throw new Error(`HTTP ${response.status}`);
            }

            const data = await response.json();
            this.currentResult = data;
            this._renderResult(data);
            this._renderGapMetrics(data.gap_metrics);
            this._renderTimeline(data);
            this._renderInsights(data);
            this._renderFunFacts(data);
        } catch (err) {
            resultEl.innerHTML = `<div class="ecv-error">加载失败: ${err.message}</div>`;
        }
    }

    _renderResult(data) {
        const resultEl = document.getElementById('ecv-result');
        if (!resultEl) return;

        const ancientBest = data.ancient_summary?.items?.[0];
        const modernBest = data.modern_summary?.items?.[0];

        resultEl.innerHTML = `
            <div style="display:grid; grid-template-columns:1fr 1fr; gap:16px;">
                <div>
                    <div class="ecv-era-badge ecv-badge-ancient" style="margin-bottom:8px;">古代阵营</div>
                    ${ancientBest ? `
                        <div style="font-size:14px; font-weight:600; margin-bottom:4px;">${ancientBest.display_name || ancientBest.vehicle_id}</div>
                        <div style="font-size:12px; color:#8899bb;">综合评分: <span style="color:#fc8; font-weight:600;">${ancientBest.overall_score?.toFixed(1) || '-'}</span></div>
                    ` : '<div style="color:#667;">暂无数据</div>'}
                </div>
                <div>
                    <div class="ecv-era-badge ecv-badge-modern" style="margin-bottom:8px;">现代阵营</div>
                    ${modernBest ? `
                        <div style="font-size:14px; font-weight:600; margin-bottom:4px;">${modernBest.display_name || modernBest.vehicle_id}</div>
                        <div style="font-size:12px; color:#8899bb;">综合评分: <span style="color:#8af; font-weight:600;">${modernBest.overall_score?.toFixed(1) || '-'}</span></div>
                    ` : '<div style="color:#667;">暂无数据</div>'}
                </div>
            </div>
            <div style="margin-top:16px; padding-top:12px; border-top:1px solid #2a3050;">
                <div style="font-size:12px; color:#8899bb; margin-bottom:6px;">🏆 全场最佳</div>
                <div style="font-size:16px; font-weight:600; color:#fc8;">
                    ${data.best_overall_vehicle_id || '-'}
                </div>
            </div>
        `;
    }

    _renderGapMetrics(gap) {
        const section = document.getElementById('ecv-gap-section');
        if (!section || !gap) return;

        section.style.display = 'block';
        section.innerHTML = `
            <div class="ecv-gap-section">
                <div class="ecv-gap-title">📊 时代差距指标</div>
                <div class="ecv-gap-grid">
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">防护系数</div>
                        <div class="ecv-gap-value">${gap.protection_factor?.toFixed(2) || '-'}<span class="ecv-gap-unit">倍</span></div>
                    </div>
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">重量效率</div>
                        <div class="ecv-gap-value">${gap.weight_efficiency_factor?.toFixed(2) || '-'}<span class="ecv-gap-unit">倍</span></div>
                    </div>
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">成本效率</div>
                        <div class="ecv-gap-value">${gap.cost_efficiency_factor?.toFixed(2) || '-'}<span class="ecv-gap-unit">倍</span></div>
                    </div>
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">速度提升</div>
                        <div class="ecv-gap-value">${gap.speed_factor?.toFixed(1) || '-'}<span class="ecv-gap-unit">倍</span></div>
                    </div>
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">技术差距</div>
                        <div class="ecv-gap-value">${gap.technology_gap_years?.toFixed(0) || '-'}<span class="ecv-gap-unit">年</span></div>
                    </div>
                    <div class="ecv-gap-card">
                        <div class="ecv-gap-label">STANAG 等级差</div>
                        <div class="ecv-gap-value">${gap.stanag_level_gap || 0}<span class="ecv-gap-unit">级</span></div>
                    </div>
                </div>
                ${gap.summary_text ? `
                    <div class="ecv-summary">
                        ${gap.summary_text}
                    </div>
                ` : ''}
            </div>
        `;
    }

    _renderTimeline(data) {
        const section = document.getElementById('ecv-timeline-section');
        if (!section) return;

        const timeline = data.timeline || [];
        if (timeline.length === 0) return;

        section.style.display = 'block';
        section.innerHTML = `
            <div class="ecv-timeline-section">
                <div class="ecv-timeline-title">📜 防护技术演进时间线</div>
                <div class="ecv-timeline">
                    ${timeline.map(entry => `
                        <div class="ecv-timeline-item">
                            <div class="ecv-timeline-year">
                                ${entry.year > 0 ? `公元 ${entry.year} 年` : `公元前 ${Math.abs(entry.year)} 年`}
                            </div>
                            <div class="ecv-timeline-name">${entry.display_name || entry.vehicle_id}</div>
                            <div class="ecv-timeline-score">防护评分: ${entry.protection_score?.toFixed(1) || '-'}</div>
                            ${entry.milestone ? `<div class="ecv-timeline-milestone">★ ${entry.milestone}</div>` : ''}
                        </div>
                    `).join('')}
                </div>
            </div>
        `;
    }

    _renderInsights(data) {
        const section = document.getElementById('ecv-insights-section');
        if (!section) return;

        const historical = data.historical_insights || [];
        const tech = data.technology_insights || [];

        if (historical.length === 0 && tech.length === 0) return;

        section.style.display = 'block';
        section.innerHTML = `
            <div class="ecv-insights-grid">
                ${historical.length > 0 ? `
                    <div class="ecv-insights-card">
                        <div class="ecv-insights-title">📚 历史洞察</div>
                        <ul class="ecv-insights-list">
                            ${historical.map(h => `<li>${h}</li>`).join('')}
                        </ul>
                    </div>
                ` : ''}
                ${tech.length > 0 ? `
                    <div class="ecv-insights-card">
                        <div class="ecv-insights-title">🔬 技术洞察</div>
                        <ul class="ecv-insights-list">
                            ${tech.map(t => `<li>${t}</li>`).join('')}
                        </ul>
                    </div>
                ` : ''}
            </div>
        `;
    }

    _renderFunFacts(data) {
        const section = document.getElementById('ecv-fun-section');
        if (!section) return;

        const funFacts = data.fun_facts || [];
        if (funFacts.length === 0) return;

        section.style.display = 'block';
        section.innerHTML = `
            <div class="ecv-fun-facts">
                <div class="ecv-fun-title">💡 趣味事实</div>
                <ul class="ecv-fun-list">
                    ${funFacts.map(f => `<li>${f}</li>`).join('')}
                </ul>
            </div>
        `;
    }
}

export { EraComparisonView };
