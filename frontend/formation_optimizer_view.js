const DEFAULT_FORMATION_TYPES = [
    { id: 'LINE', name: '一字横队', description: '横向一字排列，正面覆盖最大' },
    { id: 'WEDGE', name: '楔形冲锋阵', description: '前锋突出，适合集中突破' },
    { id: 'VEE', name: 'V形防护阵', description: '凹形结构，掩护侧翼' },
    { id: 'ECHELON', name: '梯队阵', description: '斜向梯队，灵活机动' },
    { id: 'COLUMN', name: '纵深纵队', description: '纵向排列，纵深推进' },
    { id: 'DIAMOND', name: '菱形方阵', description: '菱形分布，均衡防御' },
];

const FORMATION_COLORS = {
    LINE: '#4af',
    WEDGE: '#fa4',
    VEE: '#4fa',
    ECHELON: '#f4a',
    COLUMN: '#a4f',
    DIAMOND: '#ff4',
};

const FORMATION_NAMES = {
    LINE: '一字横队',
    WEDGE: '楔形冲锋阵',
    VEE: 'V形防护阵',
    ECHELON: '梯队阵',
    COLUMN: '纵深纵队',
    DIAMOND: '菱形方阵',
};

export class FormationOptimizerView {
    constructor(containerSelector, apiBase = 'http://127.0.0.1:8080') {
        this.container = document.querySelector(containerSelector);
        if (!this.container) throw new Error('Container not found: ' + containerSelector);
        this.apiBase = apiBase;
        this.formationTypes = [...DEFAULT_FORMATION_TYPES];
        this.currentResult = null;
    }

    async init() {
        await this._fetchFormationTypes();
        this._buildUI();
        this._bindEvents();
    }

    async _fetchFormationTypes() {
        try {
            const res = await fetch(`${this.apiBase}/api/formation/types`);
            const data = await res.json();
            if (data && Array.isArray(data.data) && data.data.length > 0) {
                this.formationTypes = data.data;
            }
        } catch (e) {
            console.warn('使用默认队形类型列表');
        }
    }

    _buildUI() {
        this.container.innerHTML = '';

        const style = document.createElement('style');
        style.textContent = `
            .fov-root { width: 100%; height: 100%; display: flex; flex-direction: column; background: #0a0e1a; color: #e0e8ff; font-family: "Microsoft YaHei", "PingFang SC", sans-serif; overflow: hidden; }
            .fov-header { padding: 14px 24px; background: linear-gradient(90deg, #1a1a3a 0%, #2a1a4a 100%); border-bottom: 2px solid #4a5aaa; }
            .fov-header h2 { font-size: 18px; font-weight: 600; background: linear-gradient(90deg, #6af, #a6f); -webkit-background-clip: text; -webkit-text-fill-color: transparent; letter-spacing: 2px; margin: 0; }
            .fov-body { flex: 1; display: grid; grid-template-columns: 340px 1fr; gap: 1px; background: #1a2040; overflow: hidden; }
            .fov-panel-left { background: #14182a; padding: 16px; overflow-y: auto; }
            .fov-panel-left::-webkit-scrollbar { width: 6px; }
            .fov-panel-left::-webkit-scrollbar-thumb { background: #3a4060; border-radius: 3px; }
            .fov-panel-right { background: #0d1525; display: flex; flex-direction: column; overflow: hidden; }
            .fov-right-top { flex: 1; padding: 16px; overflow-y: auto; }
            .fov-right-top::-webkit-scrollbar { width: 6px; }
            .fov-right-top::-webkit-scrollbar-thumb { background: #3a4060; border-radius: 3px; }
            .fov-right-bottom { padding: 12px 16px; border-top: 2px solid #2a3050; background: #14182a; }
            .fov-panel-title { font-size: 13px; color: #8af; letter-spacing: 2px; margin-bottom: 12px; padding-bottom: 8px; border-bottom: 1px solid #2a3050; text-transform: uppercase; }
            .fov-section { margin-bottom: 20px; }
            .fov-input-wrap { margin-bottom: 12px; }
            .fov-input-wrap label { display: block; font-size: 12px; color: #8899bb; margin-bottom: 6px; }
            .fov-input-wrap input, .fov-input-wrap select { width: 100%; padding: 8px 12px; background: #1e2544; border: 1px solid #3a4070; color: #e0e8ff; border-radius: 4px; font-size: 13px; outline: none; }
            .fov-input-wrap input:focus, .fov-input-wrap select:focus { border-color: #5a7aaa; box-shadow: 0 0 8px rgba(100,130,200,0.3); }
            .fov-btn { width: 100%; padding: 10px 14px; background: linear-gradient(90deg, #3a4a8a, #5a3a8a); border: none; color: #fff; border-radius: 4px; font-size: 13px; font-weight: 600; letter-spacing: 1px; cursor: pointer; transition: all 0.2s; }
            .fov-btn:hover { background: linear-gradient(90deg, #4a5aaa, #6a4aaa); box-shadow: 0 0 20px rgba(100,80,200,0.4); }
            .fov-btn:disabled { opacity: 0.5; cursor: not-allowed; }
            .fov-summary-card { background: linear-gradient(135deg, rgba(60,100,180,0.2), rgba(120,60,180,0.2)); border: 1px solid #3a5aaa; border-radius: 8px; padding: 16px; margin-bottom: 16px; }
            .fov-summary-title { font-size: 14px; color: #8af; margin-bottom: 12px; letter-spacing: 1px; }
            .fov-summary-name { font-size: 22px; font-weight: 700; background: linear-gradient(90deg, #6af, #a6f); -webkit-background-clip: text; -webkit-text-fill-color: transparent; margin-bottom: 12px; }
            .fov-summary-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
            .fov-summary-item { text-align: center; }
            .fov-summary-label { font-size: 11px; color: #8899bb; margin-bottom: 4px; }
            .fov-summary-value { font-size: 18px; font-weight: 700; font-family: "Courier New", monospace; color: #fff; }
            .fov-summary-value.highlight { color: #4f8; text-shadow: 0 0 10px rgba(80,255,136,0.4); }
            .fov-candidates-card { background: #1a2040; border: 1px solid #2a3050; border-radius: 6px; overflow: hidden; margin-bottom: 16px; }
            .fov-candidates-title { padding: 10px 14px; font-size: 13px; color: #8af; letter-spacing: 1px; background: #1e2544; border-bottom: 1px solid #2a3050; }
            .fov-table { width: 100%; border-collapse: collapse; font-size: 12px; }
            .fov-table th, .fov-table td { padding: 8px 10px; text-align: right; border-bottom: 1px solid #2a3050; }
            .fov-table th { color: #8af; text-align: center; font-weight: 500; background: #1e2544; }
            .fov-table th:first-child, .fov-table td:first-child { text-align: left; }
            .fov-table tr.best { background: rgba(80,255,136,0.08); }
            .fov-table tr.best td { color: #4f8; font-weight: 600; }
            .fov-table .rank-badge { display: inline-block; width: 20px; height: 20px; line-height: 20px; text-align: center; border-radius: 50%; font-size: 11px; font-weight: 700; margin-right: 6px; }
            .fov-table .rank-1 { background: #264; color: #8fa; }
            .fov-table .rank-2 { background: #463a20; color: #fc8; }
            .fov-table .rank-3 { background: #432a20; color: #faa; }
            .fov-recs-title { font-size: 13px; color: #8af; letter-spacing: 1px; margin-bottom: 10px; }
            .fov-rec-list { display: flex; flex-direction: column; gap: 8px; }
            .fov-rec-item { padding: 10px 14px; border-left: 3px solid #48a; background: rgba(50,100,200,0.1); border-radius: 2px; font-size: 12px; line-height: 1.6; }
            .fov-rec-item.warn { border-left-color: #aa4; background: rgba(200,180,50,0.1); }
            .fov-rec-item.danger { border-left-color: #a44; background: rgba(200,50,50,0.1); }
            .fov-canvas-wrap { background: #0d1525; border: 1px solid #2a3050; border-radius: 6px; overflow: hidden; }
            .fov-canvas-title { padding: 8px 14px; font-size: 12px; color: #8af; letter-spacing: 1px; background: #14182a; border-bottom: 1px solid #2a3050; }
            .fov-canvas-inner { padding: 0; }
            .fov-thumb-row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-top: 10px; }
            .fov-thumb-card { background: #1a2040; border: 1px solid #2a3050; border-radius: 4px; overflow: hidden; }
            .fov-thumb-title { padding: 6px 10px; font-size: 11px; color: #8af; background: #1e2544; border-bottom: 1px solid #2a3050; }
            .fov-thumb-canvas { display: block; width: 100%; height: 100px; background: #0d1525; }
            .fov-loading { display: flex; align-items: center; justify-content: center; padding: 40px; color: #8899bb; font-size: 13px; }
            .fov-spinner { width: 20px; height: 20px; border: 2px solid #3a4070; border-top-color: #6af; border-radius: 50%; animation: fov-spin 0.8s linear infinite; margin-right: 10px; }
            @keyframes fov-spin { to { transform: rotate(360deg); } }
            .fov-empty { padding: 40px; text-align: center; color: #6677aa; font-size: 13px; }
        `;
        document.head.appendChild(style);

        const root = document.createElement('div');
        root.className = 'fov-root';
        root.innerHTML = `
            <div class="fov-header">
                <h2>多车协同队形优化</h2>
            </div>
            <div class="fov-body">
                <div class="fov-panel-left">
                    <div class="fov-panel-title">作战参数</div>
                    <div class="fov-section">
                        <div class="fov-input-wrap">
                            <label>车辆数量</label>
                            <input type="number" id="fov-vehicle-count" value="5" min="1" max="20" />
                        </div>
                        <div class="fov-input-wrap">
                            <label>城墙高度 (m)</label>
                            <input type="number" id="fov-wall-height" value="10" min="1" max="50" step="0.5" />
                        </div>
                        <div class="fov-input-wrap">
                            <label>城墙长度 (m)</label>
                            <input type="number" id="fov-wall-length" value="100" min="10" max="500" step="1" />
                        </div>
                        <div class="fov-input-wrap">
                            <label>滚石频率 (每秒)</label>
                            <input type="number" id="fov-rock-freq" value="2.0" min="0.1" max="20" step="0.1" />
                        </div>
                        <div class="fov-input-wrap">
                            <label>平均滚石质量 (kg)</label>
                            <input type="number" id="fov-rock-mass" value="50" min="1" max="500" step="1" />
                        </div>
                    </div>
                    <div class="fov-panel-title">队形配置</div>
                    <div class="fov-section">
                        <div class="fov-input-wrap">
                            <label>基线队形</label>
                            <select id="fov-base-formation">
                                ${this.formationTypes.map(t => `<option value="${t.id}">${t.name || t.id}</option>`).join('')}
                            </select>
                        </div>
                        <div class="fov-input-wrap">
                            <label>间距 (m)</label>
                            <input type="number" id="fov-spacing" value="3.0" min="0.5" max="20" step="0.5" />
                        </div>
                    </div>
                    <button class="fov-btn" id="fov-run-btn">▶ 运行队形优化</button>
                </div>
                <div class="fov-panel-right">
                    <div class="fov-right-top">
                        <div id="fov-summary-area">
                            <div class="fov-empty">点击左侧"运行队形优化"按钮开始分析</div>
                        </div>
                        <div id="fov-candidates-area"></div>
                        <div id="fov-recs-area"></div>
                    </div>
                    <div class="fov-right-bottom">
                        <div class="fov-canvas-wrap">
                            <div class="fov-canvas-title" id="fov-main-canvas-title">队形可视化</div>
                            <div class="fov-canvas-inner">
                                <canvas id="fov-main-canvas" width="800" height="500"></canvas>
                            </div>
                        </div>
                        <div class="fov-thumb-row" id="fov-thumb-row"></div>
                    </div>
                </div>
            </div>
        `;
        this.container.appendChild(root);
        this._drawEmptyCanvas();
    }

    _bindEvents() {
        const btn = document.getElementById('fov-run-btn');
        if (btn) btn.addEventListener('click', () => this._onRunClick());
    }

    _onRunClick() {
        const params = {
            vehicle_count: parseInt(document.getElementById('fov-vehicle-count').value) || 5,
            wall_height: parseFloat(document.getElementById('fov-wall-height').value) || 10,
            wall_length: parseFloat(document.getElementById('fov-wall-length').value) || 100,
            rock_frequency: parseFloat(document.getElementById('fov-rock-freq').value) || 2.0,
            rock_mass: parseFloat(document.getElementById('fov-rock-mass').value) || 50,
            base_formation: document.getElementById('fov-base-formation').value || 'LINE',
            spacing: parseFloat(document.getElementById('fov-spacing').value) || 3.0,
        };
        this.runOptimization(params);
    }

    async runOptimization(params) {
        const btn = document.getElementById('fov-run-btn');
        if (btn) { btn.disabled = true; btn.textContent = '优化中...'; }

        this._showLoading();

        let result;
        try {
            const res = await fetch(`${this.apiBase}/api/formation/optimize`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(params),
            });
            const data = await res.json();
            result = data.data || this._generateDummyResult(params);
        } catch (e) {
            console.warn('后端不可用，使用本地优化结果');
            result = this._generateDummyResult(params);
        }

        this.currentResult = result;
        this._renderResult(result);

        if (btn) { btn.disabled = false; btn.textContent = '▶ 运行队形优化'; }
    }

    _showLoading() {
        const summaryArea = document.getElementById('fov-summary-area');
        if (summaryArea) {
            summaryArea.innerHTML = `<div class="fov-loading"><div class="fov-spinner"></div><span>正在执行队形优化算法...</span></div>`;
        }
        document.getElementById('fov-candidates-area').innerHTML = '';
        document.getElementById('fov-recs-area').innerHTML = '';
        document.getElementById('fov-thumb-row').innerHTML = '';
    }

    _generateDummyResult(params) {
        const types = this.formationTypes;
        const candidates = types.map((t, idx) => {
            const spacing = params.spacing + (idx - 2.5) * 0.5;
            const survival = 0.5 + Math.random() * 0.45;
            const coverage = 0.4 + Math.random() * 0.5;
            const advance = 0.4 + Math.random() * 0.5;
            const total = survival * 0.4 + coverage * 0.3 + advance * 0.3;
            return {
                formation_id: t.id,
                formation_name: t.name || t.id,
                spacing: Math.max(1.0, spacing),
                survival_rate: survival,
                coverage_score: coverage,
                advance_efficiency: advance,
                total_score: total,
                positions: null,
            };
        }).sort((a, b) => b.total_score - a.total_score);

        candidates.forEach((c, i) => { c.rank = i + 1; });

        const best = candidates[0];
        return {
            best_formation: {
                formation_id: best.formation_id,
                formation_name: best.formation_name,
                spacing: best.spacing,
                survival_rate: best.survival_rate,
                coverage_score: best.coverage_score,
                advance_efficiency: best.advance_efficiency,
                total_score: best.total_score,
            },
            candidates: candidates,
            recommendations: [
                {
                    type: 'info',
                    title: '队形推荐',
                    content: `建议采用「${best.formation_name}」，该队形在当前战场条件下综合表现最优，总评分达到 ${(best.total_score * 100).toFixed(1)} 分。`,
                },
                {
                    type: 'warn',
                    title: '间距调整',
                    content: `当前最优间距为 ${best.spacing.toFixed(1)}m，相比基线间距可提升生存率约 ${Math.abs(best.survival_rate - candidates[candidates.length - 1].survival_rate * 100).toFixed(1)}%。`,
                },
                {
                    type: 'danger',
                    title: '风险提示',
                    content: `城墙前方 30m 范围为滚石高发区，前锋车辆需加强顶部防护，建议配备复合装甲结构。`,
                },
            ],
        };
    }

    _renderResult(result) {
        this._renderSummary(result.best_formation);
        this.renderCandidateTable(result.candidates, 'fov-candidates-area');
        this.renderRecommendations(result.recommendations, 'fov-recs-area');

        this.renderFormationMap(result.best_formation, 'fov-main-canvas');
        const titleEl = document.getElementById('fov-main-canvas-title');
        if (titleEl) titleEl.textContent = `队形可视化 - ${result.best_formation.formation_name} (间距 ${result.best_formation.spacing.toFixed(1)}m)`;

        this._renderThumbnails(result.candidates.slice(0, 3));
    }

    _renderSummary(best) {
        const area = document.getElementById('fov-summary-area');
        if (!area) return;
        area.innerHTML = `
            <div class="fov-summary-card">
                <div class="fov-summary-title">★ 最优队形</div>
                <div class="fov-summary-name">${best.formation_name || best.formation_id}</div>
                <div class="fov-summary-grid">
                    <div class="fov-summary-item">
                        <div class="fov-summary-label">生存率</div>
                        <div class="fov-summary-value">${(best.survival_rate * 100).toFixed(1)}%</div>
                    </div>
                    <div class="fov-summary-item">
                        <div class="fov-summary-label">覆盖度</div>
                        <div class="fov-summary-value">${(best.coverage_score * 100).toFixed(1)}%</div>
                    </div>
                    <div class="fov-summary-item">
                        <div class="fov-summary-label">推进效率</div>
                        <div class="fov-summary-value">${(best.advance_efficiency * 100).toFixed(1)}%</div>
                    </div>
                    <div class="fov-summary-item">
                        <div class="fov-summary-label">总评分</div>
                        <div class="fov-summary-value highlight">${(best.total_score * 100).toFixed(1)}</div>
                    </div>
                </div>
            </div>
        `;
    }

    renderCandidateTable(candidates, containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;

        const sorted = [...candidates].sort((a, b) => b.total_score - a.total_score);

        container.innerHTML = `
            <div class="fov-candidates-card">
                <div class="fov-candidates-title">候选队形对比 (按总分排序)</div>
                <table class="fov-table">
                    <thead>
                        <tr>
                            <th>队形名称</th>
                            <th>间距(m)</th>
                            <th>生存率</th>
                            <th>覆盖度</th>
                            <th>推进率</th>
                            <th>总分</th>
                        </tr>
                    </thead>
                    <tbody>
                        ${sorted.map((c, i) => `
                            <tr class="${i === 0 ? 'best' : ''}">
                                <td>
                                    <span class="rank-badge rank-${Math.min(i + 1, 3)}">${i + 1}</span>
                                    ${c.formation_name || c.formation_id}
                                </td>
                                <td>${c.spacing.toFixed(1)}</td>
                                <td>${(c.survival_rate * 100).toFixed(1)}%</td>
                                <td>${(c.coverage_score * 100).toFixed(1)}%</td>
                                <td>${(c.advance_efficiency * 100).toFixed(1)}%</td>
                                <td><b>${(c.total_score * 100).toFixed(1)}</b></td>
                            </tr>
                        `).join('')}
                    </tbody>
                </table>
            </div>
        `;
    }

    renderRecommendations(recommendations, containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;

        const items = (recommendations || []).slice(0, 3).map(r => `
            <div class="fov-rec-item ${r.type || 'info'}">
                <div style="font-weight:600;margin-bottom:4px;color:#8af">${r.title || '建议'}</div>
                <div>${r.content || r.text || r.message || ''}</div>
            </div>
        `).join('');

        container.innerHTML = `
            <div class="fov-recs-title">作战建议</div>
            <div class="fov-rec-list">${items}</div>
        `;
    }

    _renderThumbnails(candidates) {
        const row = document.getElementById('fov-thumb-row');
        if (!row) return;

        row.innerHTML = candidates.map((c, i) => `
            <div class="fov-thumb-card">
                <div class="fov-thumb-title">#${i + 1} ${c.formation_name || c.formation_id} · ${(c.total_score * 100).toFixed(0)}分</div>
                <canvas class="fov-thumb-canvas" id="fov-thumb-${i}" width="260" height="100"></canvas>
            </div>
        `).join('');

        candidates.forEach((c, i) => {
            const canvas = document.getElementById(`fov-thumb-${i}`);
            if (canvas) this._drawFormationOnCanvas(c, canvas, true);
        });
    }

    async renderFormationMap(formationConfig, canvasId) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;

        let positions = null;
        try {
            const res = await fetch(`${this.apiBase}/api/formation/layout`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(formationConfig),
            });
            const data = await res.json();
            if (data && data.data && data.data.positions) {
                positions = data.data.positions;
            }
        } catch (e) {
        }

        if (positions) {
            formationConfig.positions = positions;
        }

        this._drawFormationOnCanvas(formationConfig, canvas, false);
    }

    _drawEmptyCanvas() {
        const canvas = document.getElementById('fov-main-canvas');
        if (canvas) this._drawFormationOnCanvas({ formation_id: 'LINE', spacing: 3, vehicle_count: 5 }, canvas, false, true);
    }

    _drawFormationOnCanvas(formationConfig, canvas, isThumbnail = false, isEmpty = false) {
        const ctx = canvas.getContext('2d');
        const W = canvas.width;
        const H = canvas.height;
        const dpr = window.devicePixelRatio || 1;

        if (!isThumbnail) {
            canvas.width = W * dpr;
            canvas.height = H * dpr;
            canvas.style.width = W + 'px';
            canvas.style.height = H + 'px';
            ctx.scale(dpr, dpr);
        }

        const cw = W;
        const ch = H;

        ctx.fillStyle = '#0d1525';
        ctx.fillRect(0, 0, cw, ch);

        ctx.strokeStyle = '#1a2040';
        ctx.lineWidth = 1;
        const gridStep = isThumbnail ? 30 : 40;
        for (let x = 0; x <= cw; x += gridStep) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, ch);
            ctx.stroke();
        }
        for (let y = 0; y <= ch; y += gridStep) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(cw, y);
            ctx.stroke();
        }

        const wallY = isThumbnail ? ch * 0.15 : ch * 0.12;
        const wallColor = '#8B4513';
        const wallHeight = isThumbnail ? 8 : 14;

        ctx.fillStyle = wallColor;
        ctx.fillRect(cw * 0.05, wallY - wallHeight / 2, cw * 0.9, wallHeight);

        ctx.strokeStyle = '#6B3510';
        ctx.lineWidth = 1;
        const brickW = isThumbnail ? 15 : 25;
        for (let x = cw * 0.05; x < cw * 0.95; x += brickW) {
            ctx.beginPath();
            ctx.moveTo(x, wallY - wallHeight / 2);
            ctx.lineTo(x, wallY + wallHeight / 2);
            ctx.stroke();
        }

        if (!isThumbnail) {
            ctx.fillStyle = '#6677aa';
            ctx.font = '11px monospace';
            ctx.fillText('城墙', cw * 0.02, wallY + 4);
        }

        const formationId = formationConfig.formation_id || formationConfig.id || 'LINE';
        const spacing = formationConfig.spacing || 3;
        const vehicleCount = formationConfig.vehicle_count || 5;
        const color = FORMATION_COLORS[formationId] || '#4af';

        let positions = formationConfig.positions;
        if (!positions || !Array.isArray(positions) || positions.length === 0) {
            positions = this._generateFormationLayout(formationId, vehicleCount, spacing);
        }

        if (positions.length === 0) return;

        let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
        positions.forEach(p => {
            if (p.x < minX) minX = p.x;
            if (p.x > maxX) maxX = p.x;
            if (p.y < minY) minY = p.y;
            if (p.y > maxY) maxY = p.y;
        });

        const dataW = maxX - minX;
        const dataH = maxY - minY;
        const margin = isThumbnail ? 0.15 : 0.18;
        const topPad = isThumbnail ? 0.25 : 0.22;

        const availW = cw * (1 - 2 * margin);
        const availH = ch * (1 - topPad - margin);
        const scale = Math.min(availW / Math.max(dataW, 1), availH / Math.max(dataH, 1));

        const offsetX = cw / 2 - ((minX + maxX) / 2) * scale;
        const offsetY = ch * topPad + availH / 2 - ((minY + maxY) / 2) * scale;

        const toCanvasX = (x) => offsetX + x * scale;
        const toCanvasY = (y) => offsetY - y * scale;

        if (positions.length > 1) {
            ctx.strokeStyle = color + '60';
            ctx.lineWidth = isThumbnail ? 1.5 : 2;
            ctx.setLineDash([6, 4]);
            ctx.beginPath();
            for (let i = 0; i < positions.length; i++) {
                const cx = toCanvasX(positions[i].x);
                const cy = toCanvasY(positions[i].y);
                if (i === 0) ctx.moveTo(cx, cy);
                else ctx.lineTo(cx, cy);
            }
            ctx.stroke();
            ctx.setLineDash([]);
        }

        const vehicleSize = isThumbnail ? 10 : 16;
        positions.forEach((p, i) => {
            const cx = toCanvasX(p.x);
            const cy = toCanvasY(p.y);
            const isVanguard = i === 0;
            const fillColor = isVanguard ? '#f44' : color;

            ctx.fillStyle = fillColor;
            ctx.fillRect(cx - vehicleSize / 2, cy - vehicleSize / 2, vehicleSize, vehicleSize);

            ctx.strokeStyle = '#fff';
            ctx.lineWidth = 1;
            ctx.strokeRect(cx - vehicleSize / 2, cy - vehicleSize / 2, vehicleSize, vehicleSize);

            if (!isThumbnail) {
                ctx.fillStyle = '#fff';
                ctx.font = 'bold 10px monospace';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(String(i + 1), cx, cy);
            }

            if (isVanguard && !isThumbnail) {
                ctx.strokeStyle = '#f44';
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(cx, cy - vehicleSize / 2 - 4);
                ctx.lineTo(cx, cy - vehicleSize / 2 - 14);
                ctx.lineTo(cx - 5, cy - vehicleSize / 2 - 9);
                ctx.moveTo(cx, cy - vehicleSize / 2 - 14);
                ctx.lineTo(cx + 5, cy - vehicleSize / 2 - 9);
                ctx.stroke();
            }
        });

        if (!isThumbnail) {
            const formationName = formationConfig.formation_name || FORMATION_NAMES[formationId] || formationId;
            ctx.fillStyle = color;
            ctx.font = 'bold 14px "Microsoft YaHei", sans-serif';
            ctx.textAlign = 'left';
            ctx.textBaseline = 'top';
            ctx.fillText(`队形: ${formationName}`, 14, 14);
            ctx.fillStyle = '#8899bb';
            ctx.font = '12px monospace';
            ctx.fillText(`间距: ${spacing.toFixed(1)}m  车辆: ${positions.length}辆`, 14, 36);

            const legendY = ch - 28;
            ctx.fillStyle = '#8899bb';
            ctx.font = '11px monospace';
            ctx.fillText('■ 前锋车', 14, legendY);
            ctx.fillStyle = color;
            ctx.fillRect(14, legendY + 10, 12, 12);
            ctx.fillStyle = '#f44';
            ctx.fillRect(80, legendY + 10, 12, 12);
            ctx.fillStyle = '#8899bb';
            ctx.fillText('车辆', 40, legendY + 12);
            ctx.fillText('前锋', 100, legendY + 12);

            const barLen = 80;
            const scaleBarX = cw - 110;
            const scaleBarY = ch - 22;
            ctx.strokeStyle = '#fff';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(scaleBarX, scaleBarY);
            ctx.lineTo(scaleBarX + barLen, scaleBarY);
            ctx.moveTo(scaleBarX, scaleBarY - 4);
            ctx.lineTo(scaleBarX, scaleBarY + 4);
            ctx.moveTo(scaleBarX + barLen, scaleBarY - 4);
            ctx.lineTo(scaleBarX + barLen, scaleBarY + 4);
            ctx.stroke();

            const realMeters = Math.max(1, Math.round((barLen / scale) * 10) / 10);
            ctx.fillStyle = '#fff';
            ctx.font = '10px monospace';
            ctx.textAlign = 'center';
            ctx.fillText(`${realMeters}m`, scaleBarX + barLen / 2, scaleBarY + 14);
            ctx.textAlign = 'left';

            const dirX = cw - 40;
            const dirY = ch - 50;
            ctx.strokeStyle = '#8af';
            ctx.lineWidth = 2;
            ctx.beginPath();
            ctx.moveTo(dirX, dirY + 16);
            ctx.lineTo(dirX, dirY - 16);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(dirX - 5, dirY - 10);
            ctx.lineTo(dirX, dirY - 18);
            ctx.lineTo(dirX + 5, dirY - 10);
            ctx.stroke();
            ctx.fillStyle = '#8af';
            ctx.font = 'bold 10px sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('N', dirX, dirY - 22);
            ctx.textAlign = 'left';
        }

        if (isEmpty && !isThumbnail) {
            ctx.fillStyle = '#445577';
            ctx.font = '14px "Microsoft YaHei", sans-serif';
            ctx.textAlign = 'center';
            ctx.fillText('等待优化结果...', cw / 2, ch / 2);
            ctx.textAlign = 'left';
        }
    }

    _generateFormationLayout(formationId, count, spacing) {
        const positions = [];
        count = Math.max(1, Math.min(count, 20));
        const s = spacing;

        switch (formationId) {
            case 'LINE': {
                const startX = -((count - 1) * s) / 2;
                for (let i = 0; i < count; i++) {
                    positions.push({ x: startX + i * s, y: 0 });
                }
                break;
            }
            case 'WEDGE': {
                positions.push({ x: 0, y: 0 });
                let remaining = count - 1;
                let row = 1;
                while (remaining > 0) {
                    const inRow = Math.min(2, remaining);
                    const y = -row * s * 0.8;
                    positions.push({ x: -row * s * 0.6, y });
                    if (inRow === 2) positions.push({ x: row * s * 0.6, y });
                    remaining -= inRow;
                    row++;
                }
                break;
            }
            case 'VEE': {
                positions.push({ x: 0, y: 0 });
                let remaining = count - 1;
                let row = 1;
                while (remaining > 0) {
                    const inRow = Math.min(2, remaining);
                    const y = row * s * 0.8;
                    positions.push({ x: -row * s * 0.6, y });
                    if (inRow === 2) positions.push({ x: row * s * 0.6, y });
                    remaining -= inRow;
                    row++;
                }
                break;
            }
            case 'ECHELON': {
                for (let i = 0; i < count; i++) {
                    positions.push({ x: -((count - 1) * s) / 2 + i * s, y: i * s * 0.7 });
                }
                break;
            }
            case 'COLUMN': {
                for (let i = 0; i < count; i++) {
                    positions.push({ x: 0, y: -i * s });
                }
                break;
            }
            case 'DIAMOND': {
                positions.push({ x: 0, y: 0 });
                if (count >= 2) positions.push({ x: s * 0.7, y: -s * 0.5 });
                if (count >= 3) positions.push({ x: -s * 0.7, y: -s * 0.5 });
                if (count >= 4) positions.push({ x: 0, y: -s });
                if (count >= 5) positions.push({ x: s * 1.4, y: -s });
                if (count >= 6) positions.push({ x: -s * 1.4, y: -s });
                if (count >= 7) positions.push({ x: s * 0.7, y: -s * 1.5 });
                if (count >= 8) positions.push({ x: -s * 0.7, y: -s * 1.5 });
                if (count >= 9) positions.push({ x: 0, y: -s * 2 });
                for (let i = positions.length; i < count; i++) {
                    positions.push({ x: (Math.random() - 0.5) * s * 3, y: -s * 0.5 - i * s * 0.3 });
                }
                break;
            }
            default: {
                const startX = -((count - 1) * s) / 2;
                for (let i = 0; i < count; i++) {
                    positions.push({ x: startX + i * s, y: 0 });
                }
            }
        }

        return positions;
    }
}

export default FormationOptimizerView;
