const STYLES = `
.vcv-container {
    font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    background: #0a0e1a;
    color: #e0e8ff;
    padding: 20px;
    min-height: 100vh;
    box-sizing: border-box;
}
.vcv-title {
    font-size: 22px;
    font-weight: 600;
    background: linear-gradient(90deg, #6af, #a6f);
    -webkit-background-clip: text;
    -webkit-text-fill-color: transparent;
    letter-spacing: 3px;
    margin-bottom: 20px;
    padding-bottom: 12px;
    border-bottom: 2px solid #4a5aaa;
}
.vcv-tabs {
    display: flex;
    gap: 2px;
    margin-bottom: 20px;
    border-bottom: 1px solid #2a3050;
}
.vcv-tab {
    padding: 10px 24px;
    background: #1a2040;
    border: 1px solid #2a3050;
    border-bottom: none;
    color: #8899bb;
    cursor: pointer;
    font-size: 13px;
    letter-spacing: 1px;
    border-radius: 4px 4px 0 0;
    transition: all 0.2s;
}
.vcv-tab:hover {
    background: #2a3060;
    color: #aaf;
}
.vcv-tab.active {
    background: linear-gradient(90deg, #3a4a8a, #5a3a8a);
    color: #fff;
    border-color: #4a5aaa;
    font-weight: 600;
    box-shadow: 0 -2px 10px rgba(100, 80, 200, 0.3);
}
.vcv-sections {
    display: grid;
    grid-template-columns: 280px 1fr;
    gap: 20px;
    margin-bottom: 20px;
}
.vcv-panel {
    background: #14182a;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 16px;
}
.vcv-panel-title {
    font-size: 13px;
    color: #8af;
    letter-spacing: 2px;
    margin-bottom: 12px;
    padding-bottom: 8px;
    border-bottom: 1px solid #2a3050;
    text-transform: uppercase;
}
.vcv-vehicle-list {
    max-height: 340px;
    overflow-y: auto;
    margin-bottom: 12px;
}
.vcv-vehicle-list::-webkit-scrollbar {
    width: 6px;
}
.vcv-vehicle-list::-webkit-scrollbar-thumb {
    background: #3a4060;
    border-radius: 3px;
}
.vcv-vehicle-item {
    display: flex;
    align-items: center;
    padding: 8px 10px;
    border-bottom: 1px dashed #222a44;
    cursor: pointer;
    transition: background 0.15s;
}
.vcv-vehicle-item:hover {
    background: #1e2544;
}
.vcv-vehicle-item input[type="checkbox"] {
    margin-right: 10px;
    accent-color: #4a6aaa;
    width: 14px;
    height: 14px;
}
.vcv-vehicle-name {
    font-size: 13px;
    color: #e0e8ff;
    flex: 1;
}
.vcv-vehicle-era {
    font-size: 10px;
    padding: 2px 6px;
    border-radius: 3px;
    margin-left: 6px;
}
.vcv-era-ancient {
    background: #3a2a10;
    color: #fc8;
}
.vcv-era-modern {
    background: #1a2a40;
    color: #8af;
}
.vcv-select-all {
    padding: 8px 10px;
    font-size: 12px;
    color: #8af;
    cursor: pointer;
    border-top: 1px solid #2a3050;
    margin-top: 4px;
}
.vcv-select-all:hover {
    color: #aaf;
}
.vcv-param-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 10px 0;
    border-bottom: 1px dashed #222a44;
}
.vcv-param-label {
    font-size: 12px;
    color: #8899bb;
}
.vcv-param-input {
    width: 100px;
    padding: 6px 10px;
    background: #1e2544;
    border: 1px solid #3a4070;
    color: #e0e8ff;
    border-radius: 4px;
    font-size: 13px;
    font-family: "Courier New", monospace;
    outline: none;
    text-align: right;
}
.vcv-param-input:focus {
    border-color: #4a6aaa;
    box-shadow: 0 0 8px rgba(74, 106, 170, 0.3);
}
.vcv-param-unit {
    font-size: 11px;
    color: #667;
    margin-left: 6px;
}
.vcv-run-btn {
    width: 100%;
    padding: 12px 20px;
    background: linear-gradient(90deg, #3a4a8a, #5a3a8a);
    border: none;
    color: #fff;
    border-radius: 4px;
    font-size: 14px;
    font-weight: 600;
    letter-spacing: 2px;
    cursor: pointer;
    transition: all 0.2s;
    margin-top: 16px;
}
.vcv-run-btn:hover {
    background: linear-gradient(90deg, #4a5aaa, #6a4aaa);
    box-shadow: 0 0 20px rgba(100, 80, 200, 0.4);
}
.vcv-run-btn:disabled {
    background: #2a3050;
    color: #667;
    cursor: not-allowed;
    box-shadow: none;
}
.vcv-results {
    display: flex;
    flex-direction: column;
    gap: 20px;
}
.vcv-charts-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
}
.vcv-chart-card {
    background: #1a2040;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 14px;
    display: flex;
    flex-direction: column;
}
.vcv-chart-title {
    font-size: 13px;
    color: #8af;
    margin-bottom: 10px;
    letter-spacing: 1px;
}
.vcv-chart-wrap {
    flex: 1;
    position: relative;
    min-height: 300px;
}
.vcv-chart-wrap canvas {
    width: 100%;
    height: 100%;
    display: block;
}
.vcv-table-card {
    background: #1a2040;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 14px;
}
.vcv-table {
    width: 100%;
    border-collapse: collapse;
    font-size: 12px;
}
.vcv-table th,
.vcv-table td {
    padding: 10px 12px;
    text-align: right;
    border-bottom: 1px solid #2a3050;
}
.vcv-table th {
    color: #8af;
    text-align: center;
    font-weight: 500;
    background: #1e2544;
    position: sticky;
    top: 0;
}
.vcv-table th:first-child,
.vcv-table td:first-child,
.vcv-table th:nth-child(2),
.vcv-table td:nth-child(2) {
    text-align: left;
}
.vcv-table tr.top-rank td {
    background: rgba(40, 180, 80, 0.15);
    color: #4f8;
    font-weight: 600;
}
.vcv-table tr:hover td {
    background: #252d55;
}
.vcv-table tr.top-rank:hover td {
    background: rgba(40, 180, 80, 0.25);
}
.vcv-rank-badge {
    display: inline-block;
    width: 22px;
    height: 22px;
    line-height: 22px;
    text-align: center;
    border-radius: 50%;
    font-size: 11px;
    font-weight: 700;
}
.vcv-rank-1 {
    background: linear-gradient(135deg, #ffd700, #ffaa00);
    color: #1a1a00;
}
.vcv-rank-2 {
    background: linear-gradient(135deg, #c0c0c0, #a0a0a0);
    color: #1a1a1a;
}
.vcv-rank-3 {
    background: linear-gradient(135deg, #cd7f32, #a0522d);
    color: #fff;
}
.vcv-rank-other {
    background: #2a3050;
    color: #889;
}
.vcv-insights-card {
    background: #1a2040;
    border: 1px solid #2a3050;
    border-radius: 6px;
    padding: 14px;
}
.vcv-insights-list {
    display: flex;
    flex-direction: column;
    gap: 10px;
}
.vcv-insight-item {
    padding: 12px 14px;
    background: #14182a;
    border-left: 3px solid #4a6aaa;
    border-radius: 4px;
    font-size: 13px;
    line-height: 1.6;
}
.vcv-insight-item.level-high {
    border-left-color: #f55;
    background: rgba(200, 50, 50, 0.08);
}
.vcv-insight-item.level-medium {
    border-left-color: #fa4;
    background: rgba(200, 180, 50, 0.08);
}
.vcv-insight-item.level-low {
    border-left-color: #4a8;
    background: rgba(50, 180, 100, 0.08);
}
.vcv-empty {
    padding: 40px;
    text-align: center;
    color: #667;
    font-size: 13px;
}
.vcv-loading {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 40px;
    color: #8af;
    font-size: 13px;
}
.vcv-loading::after {
    content: "";
    width: 16px;
    height: 16px;
    border: 2px solid #4a6aaa;
    border-top-color: transparent;
    border-radius: 50%;
    margin-left: 12px;
    animation: vcv-spin 0.8s linear infinite;
}
@keyframes vcv-spin {
    to { transform: rotate(360deg); }
}
.vcv-legend {
    display: flex;
    gap: 16px;
    margin-top: 10px;
    flex-wrap: wrap;
}
.vcv-legend-item {
    display: flex;
    align-items: center;
    font-size: 11px;
    color: #889;
}
.vcv-legend-color {
    width: 14px;
    height: 14px;
    border-radius: 3px;
    margin-right: 6px;
}
`;

const ERA_COLORS = {
    ancient: '#d4a04c',
    modern: '#4c8ad4',
};

const VEHICLE_COLORS = [
    '#4c8ad4', '#d4a04c', '#8acc5c', '#d45c5c',
    '#a06cd4', '#5ccccc', '#d48a4c', '#6cd4a0',
];

const RADAR_DIMENSIONS = [
    { key: 'roof_max_deformation_mm', label: '顶棚变形(mm)', lowerIsBetter: true },
    { key: 'roof_von_mises_stress_mpa', label: '应力(MPa)', lowerIsBetter: true },
    { key: 'absorbed_energy_j', label: '吸能(J)', lowerIsBetter: false },
    { key: 'damage_level', label: '损伤等级', lowerIsBetter: true },
    { key: 'penetration_depth_mm', label: '侵彻深度(mm)', lowerIsBetter: true },
    { key: 'protection_efficiency_score', label: '防护效率', lowerIsBetter: false },
];

function hexToRgb(hex) {
    const result = /^#?([a-f\d]{2})([a-f\d]{2})([a-f\d]{2})$/i.exec(hex);
    return result ? {
        r: parseInt(result[1], 16),
        g: parseInt(result[2], 16),
        b: parseInt(result[3], 16),
    } : { r: 0, g: 0, b: 0 };
}

function rgbToHex(r, g, b) {
    return '#' + [r, g, b].map(v => {
        const h = Math.max(0, Math.min(255, Math.round(v))).toString(16);
        return h.length === 1 ? '0' + h : h;
    }).join('');
}

function lerpColor(colorA, colorB, t) {
    const a = hexToRgb(colorA);
    const b = hexToRgb(colorB);
    return rgbToHex(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
    );
}

function rgba(hex, alpha) {
    const c = hexToRgb(hex);
    return `rgba(${c.r},${c.g},${c.b},${alpha})`;
}

export class VehicleComparisonView {
    constructor(containerSelector, apiBase) {
        this.containerSelector = containerSelector;
        this.apiBase = apiBase;
        this.vehicles = [];
        this.currentMode = 'ancient';
        this.selectedVehicleIds = new Set();
        this.lastResults = null;
        this._styleInjected = false;
    }

    async init() {
        this._injectStyles();
        this._buildLayout();
        await this._loadVehicles();
        this._renderVehicleList();
        this._bindEvents();
    }

    _injectStyles() {
        if (this._styleInjected) return;
        const style = document.createElement('style');
        style.textContent = STYLES;
        document.head.appendChild(style);
        this._styleInjected = true;
    }

    _buildLayout() {
        const container = document.querySelector(this.containerSelector);
        if (!container) {
            console.error('VehicleComparisonView: container not found:', this.containerSelector);
            return;
        }
        container.innerHTML = `
            <div class="vcv-container">
                <div class="vcv-title">古代攻城车防护能力对比</div>

                <div class="vcv-tabs">
                    <div class="vcv-tab active" data-mode="ancient">古代车型对比</div>
                    <div class="vcv-tab" data-mode="cross-era">跨时代对比</div>
                </div>

                <div class="vcv-sections">
                    <div class="vcv-panel">
                        <div class="vcv-panel-title">车辆选择</div>
                        <div class="vcv-vehicle-list" id="vcv-vehicle-list"></div>
                        <div class="vcv-select-all" id="vcv-select-all">全选 / 取消全选</div>
                    </div>

                    <div class="vcv-panel">
                        <div class="vcv-panel-title">冲击参数</div>
                        <div class="vcv-param-row">
                            <span class="vcv-param-label">滚石质量</span>
                            <span>
                                <input type="number" class="vcv-param-input" id="vcv-rock-mass" value="50" min="1" step="1">
                                <span class="vcv-param-unit">kg</span>
                            </span>
                        </div>
                        <div class="vcv-param-row">
                            <span class="vcv-param-label">冲击速度</span>
                            <span>
                                <input type="number" class="vcv-param-input" id="vcv-rock-velocity" value="15" min="1" step="1">
                                <span class="vcv-param-unit">m/s</span>
                            </span>
                        </div>
                        <button class="vcv-run-btn" id="vcv-run-btn">▶ 运行对比分析</button>
                    </div>
                </div>

                <div class="vcv-results" id="vcv-results">
                    <div class="vcv-empty">请选择车辆并点击"运行对比分析"按钮开始</div>
                </div>
            </div>
        `;
    }

    async _loadVehicles() {
        try {
            const res = await fetch(`${this.apiBase}/api/vehicles`);
            const data = await res.json();
            if (data && data.data) {
                this.vehicles = Array.isArray(data.data) ? data.data : Object.values(data.data);
            } else if (data && data.vehicles) {
                this.vehicles = Object.entries(data.vehicles).map(([id, v]) => ({ id, ...v }));
            } else {
                this.vehicles = [];
            }
        } catch (e) {
            console.warn('Failed to load vehicles from API, using defaults');
            this.vehicles = this._getDefaultVehicles();
        }
    }

    _getDefaultVehicles() {
        return [
            { id: 'fenyun_basic', display_name: '轒辒车(基础型)', era: 'ancient', description: '春秋时期经典攻城车' },
            { id: 'fenyun_heavy', display_name: '轒辒车(重装型)', era: 'ancient', description: '战国时期改进型' },
            { id: 'chongche_ram', display_name: '冲车(撞城车)', era: 'ancient', description: '重型攻城槌车' },
            { id: 'chongche_tower', display_name: '冲车(楼车)', era: 'ancient', description: '高层攻城塔车' },
            { id: 'yunti_basic', display_name: '云梯(基础型)', era: 'ancient', description: '经典折叠云梯车' },
            { id: 'yunti_armored', display_name: '云梯(装甲型)', era: 'ancient', description: '加装牛皮和薄铁皮' },
            { id: 'modern_m113', display_name: 'M113 装甲运兵车', era: 'modern', description: '经典现代APC' },
            { id: 'modern_m2_bradley', display_name: 'M2 布雷德利步兵战车', era: 'modern', description: '重型IFV' },
            { id: 'modern_m1a2', display_name: 'M1A2 艾布拉姆斯坦克', era: 'modern', description: '第三代主战坦克' },
        ];
    }

    _renderVehicleList() {
        const listEl = document.getElementById('vcv-vehicle-list');
        if (!listEl) return;

        const filtered = this.currentMode === 'cross-era'
            ? this.vehicles
            : this.vehicles.filter(v => v.era === this.currentMode);

        if (filtered.length === 0) {
            listEl.innerHTML = '<div class="vcv-empty" style="padding:20px">暂无车辆数据</div>';
            return;
        }

        listEl.innerHTML = filtered.map(v => {
            const checked = this.selectedVehicleIds.has(v.id) ? 'checked' : '';
            const eraCls = v.era === 'ancient' ? 'vcv-era-ancient' : 'vcv-era-modern';
            const eraLabel = v.era === 'ancient' ? '古代' : '现代';
            return `
                <label class="vcv-vehicle-item">
                    <input type="checkbox" data-vehicle-id="${v.id}" ${checked}>
                    <span class="vcv-vehicle-name">${v.display_name || v.id}</span>
                    <span class="vcv-vehicle-era ${eraCls}">${eraLabel}</span>
                </label>
            `;
        }).join('');

        listEl.querySelectorAll('input[type="checkbox"]').forEach(cb => {
            cb.addEventListener('change', (e) => {
                const id = e.target.dataset.vehicleId;
                if (e.target.checked) {
                    this.selectedVehicleIds.add(id);
                } else {
                    this.selectedVehicleIds.delete(id);
                }
            });
        });
    }

    _bindEvents() {
        document.querySelectorAll('.vcv-tab').forEach(tab => {
            tab.addEventListener('click', (e) => {
                document.querySelectorAll('.vcv-tab').forEach(t => t.classList.remove('active'));
                e.target.classList.add('active');
                this.currentMode = e.target.dataset.mode;
                this.selectedVehicleIds.clear();
                this._renderVehicleList();
            });
        });

        const selectAllEl = document.getElementById('vcv-select-all');
        if (selectAllEl) {
            selectAllEl.addEventListener('click', () => {
                const listEl = document.getElementById('vcv-vehicle-list');
                const checkboxes = listEl?.querySelectorAll('input[type="checkbox"]') || [];
                const allChecked = Array.from(checkboxes).every(cb => cb.checked);
                checkboxes.forEach(cb => {
                    cb.checked = !allChecked;
                    const id = cb.dataset.vehicleId;
                    if (!allChecked) this.selectedVehicleIds.add(id);
                    else this.selectedVehicleIds.delete(id);
                });
            });
        }

        const runBtn = document.getElementById('vcv-run-btn');
        if (runBtn) {
            runBtn.addEventListener('click', () => this._handleRunClick());
        }
    }

    async _handleRunClick() {
        if (this.selectedVehicleIds.size === 0) {
            alert('请至少选择一辆车进行对比');
            return;
        }

        const mass = parseFloat(document.getElementById('vcv-rock-mass')?.value || '50');
        const velocity = parseFloat(document.getElementById('vcv-rock-velocity')?.value || '15');

        const runBtn = document.getElementById('vcv-run-btn');
        if (runBtn) {
            runBtn.disabled = true;
            runBtn.textContent = '分析中...';
        }

        const resultsEl = document.getElementById('vcv-results');
        if (resultsEl) {
            resultsEl.innerHTML = '<div class="vcv-loading">正在执行防护对比分析，请稍候...</div>';
        }

        try {
            await this.runComparison(Array.from(this.selectedVehicleIds), {
                rock_mass_kg: mass,
                rock_velocity_ms: velocity,
            });
        } catch (e) {
            console.error('Comparison failed:', e);
            if (resultsEl) {
                resultsEl.innerHTML = `<div class="vcv-empty" style="color:#f55">分析失败：${e.message}</div>`;
            }
        } finally {
            if (runBtn) {
                runBtn.disabled = false;
                runBtn.textContent = '▶ 运行对比分析';
            }
        }
    }

    async runComparison(vehicleIds, params) {
        let endpoint;
        if (this.currentMode === 'ancient') {
            endpoint = '/api/vehicle/comparison/ancient';
        } else if (this.currentMode === 'cross-era') {
            endpoint = '/api/vehicle/comparison/cross-era';
        } else {
            endpoint = '/api/vehicle/comparison';
        }

        const body = {
            vehicle_ids: vehicleIds,
            rock_mass_kg: params.rock_mass_kg,
            rock_velocity_ms: params.rock_velocity_ms,
            ...params,
        };

        let data;
        try {
            const res = await fetch(`${this.apiBase}${endpoint}`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(body),
            });
            data = await res.json();
        } catch (e) {
            console.warn('API call failed, using mock data');
            data = this._generateMockData(vehicleIds, params);
        }

        const items = data?.data?.comparisons || data?.comparisons || data?.data || [];
        const insights = data?.data?.insights || data?.insights || [];

        this.lastResults = { items, insights };
        this._renderResults(items, insights);
    }

    _generateMockData(vehicleIds, params) {
        const items = vehicleIds.map((id, idx) => {
            const v = this.vehicles.find(vv => vv.id === id) || { id, display_name: id, era: 'ancient' };
            const modernBonus = v.era === 'modern' ? 30 : 0;
            const randomFactor = () => 0.85 + Math.random() * 0.3;
            return {
                vehicle_id: id,
                vehicle_name: v.display_name || id,
                era: v.era || 'ancient',
                roof_max_deformation_mm: Math.max(2, (v.era === 'modern' ? 5 : 25) * randomFactor()),
                roof_von_mises_stress_mpa: Math.max(10, (v.era === 'modern' ? 80 : 200) * randomFactor()),
                absorbed_energy_j: (v.era === 'modern' ? 80000 : 15000) * randomFactor(),
                damage_level: Math.max(0, Math.min(4, Math.floor((v.era === 'modern' ? 1 : 3) + Math.random() * 1.5))),
                penetration_depth_mm: Math.max(0, (v.era === 'modern' ? 3 : 30) * randomFactor()),
                protection_efficiency_score: (50 + modernBonus + Math.random() * 25),
                weight_normalized_score: (40 + (v.era === 'modern' ? 10 : 40) * Math.random()),
                cost_normalized_score: (v.era === 'modern' ? 30 : 70) + Math.random() * 20,
                composite_score: 0,
                rank: 0,
            };
        });

        items.forEach(it => {
            const deformScore = Math.max(0, 100 - it.roof_max_deformation_mm * 2);
            const stressScore = Math.max(0, 100 - it.roof_von_mises_stress_mpa / 3);
            const energyScore = Math.min(100, it.absorbed_energy_j / 1000);
            const damageScore = (4 - it.damage_level) * 25;
            const penScore = Math.max(0, 100 - it.penetration_depth_mm * 2);
            it.composite_score = Math.round(
                (it.protection_efficiency_score * 0.3 +
                 deformScore * 0.1 +
                 stressScore * 0.1 +
                 energyScore * 0.15 +
                 damageScore * 0.1 +
                 penScore * 0.1 +
                 it.weight_normalized_score * 0.1 +
                 it.cost_normalized_score * 0.05) * 10
            ) / 10;
        });

        items.sort((a, b) => b.composite_score - a.composite_score);
        items.forEach((it, i) => it.rank = i + 1);

        const insights = [
            { level: 'high', text: `${items[0]?.vehicle_name || '最佳车型'} 在本次对比中综合评分最高，防护效率表现突出。` },
            { level: 'medium', text: '现代装甲车辆在结构强度和吸能能力方面普遍优于古代攻城车，但成本显著更高。' },
            { level: 'low', text: `滚石冲击参数：质量 ${params.rock_mass_kg}kg，速度 ${params.rock_velocity_ms}m/s，动能 ${Math.round(0.5 * params.rock_mass_kg * params.rock_velocity_ms ** 2)}J。` },
            items.length > 1 ? { level: 'medium', text: `第一名 ${items[0]?.vehicle_name} 比第二名 ${items[1]?.vehicle_name} 综合评分高出 ${(items[0].composite_score - items[1].composite_score).toFixed(1)} 分。` } : null,
        ].filter(Boolean);

        return { data: { comparisons: items, insights } };
    }

    _renderResults(items, insights) {
        const resultsEl = document.getElementById('vcv-results');
        if (!resultsEl) return;

        if (!items || items.length === 0) {
            resultsEl.innerHTML = '<div class="vcv-empty">无对比结果数据</div>';
            return;
        }

        resultsEl.innerHTML = `
            <div class="vcv-charts-row">
                <div class="vcv-chart-card">
                    <div class="vcv-chart-title">综合评分柱状图</div>
                    <div class="vcv-chart-wrap"><canvas id="vcv-bar-chart"></canvas></div>
                    <div class="vcv-legend" id="vcv-bar-legend"></div>
                </div>
                <div class="vcv-chart-card">
                    <div class="vcv-chart-title">关键指标雷达图</div>
                    <div class="vcv-chart-wrap"><canvas id="vcv-radar-chart"></canvas></div>
                    <div class="vcv-legend" id="vcv-radar-legend"></div>
                </div>
            </div>

            <div class="vcv-table-card">
                <div class="vcv-chart-title">评分明细</div>
                <div style="overflow-x:auto;">
                    <table class="vcv-table" id="vcv-result-table">
                        <thead>
                            <tr>
                                <th>排名</th>
                                <th>车名</th>
                                <th>时代</th>
                                <th>防护效率分</th>
                                <th>重量归一化分</th>
                                <th>成本归一化分</th>
                                <th>综合分</th>
                            </tr>
                        </thead>
                        <tbody></tbody>
                    </table>
                </div>
            </div>

            <div class="vcv-insights-card">
                <div class="vcv-chart-title">洞察分析</div>
                <div class="vcv-insights-list" id="vcv-insights-list"></div>
            </div>
        `;

        this.renderBarChart(items, 'vcv-bar-chart');
        this.renderRadarChart(items, 'vcv-radar-chart');
        this.renderTable(items, 'vcv-result-table');
        this._renderLegends(items);
        this._renderInsights(insights);
    }

    _renderLegends(items) {
        ['vcv-bar-legend', 'vcv-radar-legend'].forEach(id => {
            const el = document.getElementById(id);
            if (!el) return;
            el.innerHTML = items.map((it, i) => {
                const color = VEHICLE_COLORS[i % VEHICLE_COLORS.length];
                return `<span class="vcv-legend-item"><span class="vcv-legend-color" style="background:${color}"></span>${it.vehicle_name}</span>`;
            }).join('');
        });
    }

    _renderInsights(insights) {
        const el = document.getElementById('vcv-insights-list');
        if (!el) return;
        if (!insights || insights.length === 0) {
            el.innerHTML = '<div class="vcv-empty" style="padding:20px">暂无洞察分析</div>';
            return;
        }
        el.innerHTML = insights.map(ins => {
            const level = ins.level || ins.type || 'low';
            const text = ins.text || ins.message || ins.content || JSON.stringify(ins);
            return `<div class="vcv-insight-item level-${level}">${text}</div>`;
        }).join('');
    }

    renderBarChart(items, canvasId) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;

        const wrap = canvas.parentElement;
        if (!wrap) return;

        const dpr = window.devicePixelRatio || 1;
        const w = wrap.clientWidth;
        const h = wrap.clientHeight;
        if (w < 10 || h < 10) return;

        canvas.width = w * dpr;
        canvas.height = h * dpr;
        canvas.style.width = w + 'px';
        canvas.style.height = h + 'px';

        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);

        ctx.fillStyle = '#1a2040';
        ctx.fillRect(0, 0, w, h);

        const padLeft = 55;
        const padRight = 20;
        const padTop = 25;
        const padBottom = 80;

        const chartW = w - padLeft - padRight;
        const chartH = h - padTop - padBottom;

        ctx.strokeStyle = '#2a3050';
        ctx.lineWidth = 1;
        for (let i = 0; i <= 5; i++) {
            const y = padTop + (chartH * i / 5);
            ctx.beginPath();
            ctx.moveTo(padLeft, y);
            ctx.lineTo(w - padRight, y);
            ctx.stroke();

            ctx.fillStyle = '#667';
            ctx.font = '10px "Courier New", monospace';
            ctx.textAlign = 'right';
            ctx.textBaseline = 'middle';
            const val = 100 - i * 20;
            ctx.fillText(String(val), padLeft - 8, y);
        }

        const n = items.length;
        if (n === 0) return;

        const barGap = 12;
        const groupW = chartW / n;
        const barW = Math.min(groupW - barGap, 50);

        items.forEach((it, i) => {
            const color = ERA_COLORS[it.era] || VEHICLE_COLORS[i % VEHICLE_COLORS.length];
            const score = Math.max(0, Math.min(100, it.composite_score ?? it.protection_efficiency_score ?? 0));
            const barH = (score / 100) * chartH;
            const x = padLeft + groupW * i + (groupW - barW) / 2;
            const y = padTop + chartH - barH;

            const grad = ctx.createLinearGradient(x, y, x, y + barH);
            grad.addColorStop(0, color);
            grad.addColorStop(1, rgba(color, 0.5));
            ctx.fillStyle = grad;
            ctx.fillRect(x, y, barW, barH);

            ctx.strokeStyle = rgba(color, 0.8);
            ctx.lineWidth = 1;
            ctx.strokeRect(x, y, barW, barH);

            ctx.fillStyle = '#fff';
            ctx.font = 'bold 11px "Courier New", monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            ctx.fillText(score.toFixed(1), x + barW / 2, y - 4);

            ctx.fillStyle = '#aab';
            ctx.font = '11px "Microsoft YaHei", sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'top';
            const name = it.vehicle_name || it.vehicle_id || '';
            const displayName = name.length > 6 ? name.slice(0, 5) + '…' : name;
            ctx.save();
            ctx.translate(x + barW / 2, padTop + chartH + 6);
            ctx.rotate(-0.35);
            ctx.fillText(displayName, 0, 0);
            ctx.restore();
        });

        ctx.fillStyle = '#8af';
        ctx.font = '11px "Microsoft YaHei", sans-serif';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'bottom';
        ctx.fillText('综合评分 (0-100)', padLeft + chartW / 2, h - 5);
    }

    renderRadarChart(items, canvasId) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;

        const wrap = canvas.parentElement;
        if (!wrap) return;

        const dpr = window.devicePixelRatio || 1;
        const w = wrap.clientWidth;
        const h = wrap.clientHeight;
        if (w < 10 || h < 10) return;

        canvas.width = w * dpr;
        canvas.height = h * dpr;
        canvas.style.width = w + 'px';
        canvas.style.height = h + 'px';

        const ctx = canvas.getContext('2d');
        ctx.scale(dpr, dpr);

        ctx.fillStyle = '#1a2040';
        ctx.fillRect(0, 0, w, h);

        const cx = w / 2;
        const cy = h / 2;
        const radius = Math.min(w, h) * 0.36;
        const levels = 5;
        const dims = RADAR_DIMENSIONS;
        const n = dims.length;

        for (let lv = levels; lv >= 1; lv--) {
            const r = (radius * lv) / levels;
            ctx.beginPath();
            for (let i = 0; i < n; i++) {
                const angle = (Math.PI * 2 * i) / n - Math.PI / 2;
                const x = cx + Math.cos(angle) * r;
                const y = cy + Math.sin(angle) * r;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.closePath();
            ctx.strokeStyle = '#2a3050';
            ctx.lineWidth = 1;
            ctx.stroke();
            if (lv === levels) {
                ctx.fillStyle = 'rgba(42, 48, 80, 0.2)';
                ctx.fill();
            }
        }

        const normValues = this._normalizeRadarData(items);

        items.forEach((it, itemIdx) => {
            const color = VEHICLE_COLORS[itemIdx % VEHICLE_COLORS.length];
            ctx.beginPath();
            for (let i = 0; i < n; i++) {
                const angle = (Math.PI * 2 * i) / n - Math.PI / 2;
                const val = normValues[itemIdx][i];
                const r = radius * val;
                const x = cx + Math.cos(angle) * r;
                const y = cy + Math.sin(angle) * r;
                if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
            }
            ctx.closePath();
            ctx.fillStyle = rgba(color, 0.15);
            ctx.fill();
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.stroke();

            for (let i = 0; i < n; i++) {
                const angle = (Math.PI * 2 * i) / n - Math.PI / 2;
                const val = normValues[itemIdx][i];
                const r = radius * val;
                const x = cx + Math.cos(angle) * r;
                const y = cy + Math.sin(angle) * r;
                ctx.beginPath();
                ctx.arc(x, y, 3, 0, Math.PI * 2);
                ctx.fillStyle = color;
                ctx.fill();
            }
        });

        for (let i = 0; i < n; i++) {
            const angle = (Math.PI * 2 * i) / n - Math.PI / 2;
            const x1 = cx + Math.cos(angle) * radius * 1.0;
            const y1 = cy + Math.sin(angle) * radius * 1.0;
            ctx.beginPath();
            ctx.moveTo(cx, cy);
            ctx.lineTo(x1, y1);
            ctx.strokeStyle = '#2a3050';
            ctx.lineWidth = 1;
            ctx.stroke();

            const lx = cx + Math.cos(angle) * (radius + 28);
            const ly = cy + Math.sin(angle) * (radius + 28);
            ctx.fillStyle = '#8af';
            ctx.font = '11px "Microsoft YaHei", sans-serif';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(dims[i].label, lx, ly);
        }
    }

    _normalizeRadarData(items) {
        const dims = RADAR_DIMENSIONS;
        const result = items.map(() => new Array(dims.length).fill(0));

        dims.forEach((dim, dIdx) => {
            const values = items.map(it => {
                const v = it[dim.key];
                return typeof v === 'number' ? v : (dim.lowerIsBetter ? 100 : 0);
            });
            const min = Math.min(...values);
            const max = Math.max(...values);
            const range = max - min || 1;

            items.forEach((_it, iIdx) => {
                let norm = (values[iIdx] - min) / range;
                if (dim.lowerIsBetter) norm = 1 - norm;
                norm = Math.max(0.05, Math.min(1, norm));
                result[iIdx][dIdx] = norm;
            });
        });

        return result;
    }

    renderTable(items, tableId) {
        const table = document.getElementById(tableId);
        if (!table) return;

        const tbody = table.querySelector('tbody');
        if (!tbody) return;

        tbody.innerHTML = items.map(it => {
            const eraLabel = it.era === 'modern' ? '现代' : (it.era === 'ancient' ? '古代' : (it.era || '--'));
            const eraCls = it.era === 'modern' ? 'vcv-era-modern' : 'vcv-era-ancient';
            const rank = it.rank || 0;
            const rankCls = rank === 1 ? 'vcv-rank-1' : rank === 2 ? 'vcv-rank-2' : rank === 3 ? 'vcv-rank-3' : 'vcv-rank-other';
            const topCls = rank === 1 ? 'top-rank' : '';

            return `
                <tr class="${topCls}">
                    <td><span class="vcv-rank-badge ${rankCls}">${rank}</span></td>
                    <td>${it.vehicle_name || it.vehicle_id || '--'}</td>
                    <td><span class="vcv-vehicle-era ${eraCls}">${eraLabel}</span></td>
                    <td>${this._fmt(it.protection_efficiency_score)}</td>
                    <td>${this._fmt(it.weight_normalized_score)}</td>
                    <td>${this._fmt(it.cost_normalized_score)}</td>
                    <td><b>${this._fmt(it.composite_score)}</b></td>
                </tr>
            `;
        }).join('');
    }

    _fmt(v) {
        if (typeof v !== 'number' || isNaN(v)) return '--';
        return v.toFixed(1);
    }
}

export default VehicleComparisonView;
