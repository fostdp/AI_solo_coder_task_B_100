import { AssaultCart3D } from './assault_cart_3d.js';

const API_BASE = 'http://127.0.0.1:8080';

const DAMAGE_LEVELS = [
    { label: '完好', cls: 'hud-safe' },
    { label: '轻微', cls: 'hud-safe' },
    { label: '中度', cls: 'hud-warn' },
    { label: '严重', cls: 'hud-warn' },
    { label: '破坏', cls: 'hud-danger' },
];

export class ProtectionPanel {
    constructor(cart3D, apiBase = API_BASE) {
        this.cart = cart3D;
        this.apiBase = apiBase;

        this.stressHistory = [];
        this.impactHistory = [];
        this.deformationThreshold = 15.0;
        this.currentVehicle = 1;

        this._bindEvents();
        this._startClock();

        this.cart.on('fps', (fps) => {
            const el = document.getElementById('clock');
            if (el) {
                const now = new Date();
                const t = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
                el.textContent = `${t} · ${Math.round(fps)}FPS`;
            }
        });
    }

    _bindEvents() {
        const vs = document.getElementById('vehicle-select');
        if (vs) vs.addEventListener('change', e => {
            this.currentVehicle = parseInt(e.target.value);
        });

        const ms = document.getElementById('material-select');
        if (ms) ms.addEventListener('change', e => {
            this.cart.setMaterial(e.target.value);
            this.drawHeatmap();
        });

        const btnSim = document.getElementById('btn-simulate');
        if (btnSim) btnSim.addEventListener('click', () => this.runSimulation());

        const btnEval = document.getElementById('btn-evaluate');
        if (btnEval) btnEval.addEventListener('click', () => this.runAHPEvaluation());

        const btnReset = document.getElementById('btn-reset');
        if (btnReset) btnReset.addEventListener('click', () => this.cart.resetView());

        this._setupToggle('toggle-rocks', 'showRocks');
        this._setupToggle('toggle-cloud', 'showCloud');
        this._setupToggle('toggle-stress', 'showStress');
        this._setupToggle('toggle-wireframe', 'showWireframe');
        this._setupToggle('toggle-rotate', 'autoRotate');

        ['front', 'side', 'top', 'iso'].forEach(v => {
            const el = document.getElementById('view-' + v);
            if (el) el.addEventListener('click', () => this.cart.setView(v));
        });
    }

    _setupToggle(id, prop) {
        const el = document.getElementById(id);
        if (!el) return;
        el.addEventListener('click', () => {
            el.classList.toggle('active');
            const active = el.classList.contains('active');

            if (prop === 'showCloud') {
                if (active) {
                    document.getElementById('toggle-stress')?.classList.remove('active');
                    this.cart.setShowStress(false);
                }
                this.cart.setShowCloud(active);
            } else if (prop === 'showStress') {
                if (active) {
                    document.getElementById('toggle-cloud')?.classList.remove('active');
                    this.cart.setShowCloud(false);
                }
                this.cart.setShowStress(active);
            } else if (prop === 'showRocks') {
                this.cart.setShowRocks(active);
            } else if (prop === 'showWireframe') {
                this.cart.setShowWireframe(active);
            } else if (prop === 'autoRotate') {
                this.cart.setAutoRotate(active);
            }

            this.drawHeatmap();
        });
    }

    _startClock() {
        const tick = () => {
            const el = document.getElementById('clock');
            if (el) {
                const now = new Date();
                const t = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
                el.textContent = `${t} · ${Math.round(this.cart.getFps())}FPS`;
            }
        };
        setInterval(tick, 1000);
        tick();
    }

    async runSimulation() {
        this.addAlert('info', '执行Johnson-Cook高应变率结构仿真...');

        const fx = (Math.random() - 0.5) * 6.5 * 0.7;
        const fz = (Math.random() - 0.5) * 2.8 * 0.7;
        this.cart.triggerImpactFlash(fx, fz);

        try {
            const res = await fetch(`${this.apiBase}/api/simulation/latest?vehicle_id=${this.currentVehicle}`);
            const data = await res.json();
            const sim = data.data;
            this._updateSimulationDisplay(sim);
            this.cart.setDeformationField(sim.deformation_field || [], false);
            this.cart.setDeformationField(sim.stress_field || [], true);

            if (sim.roof_max_deformation_mm > this.deformationThreshold) {
                this.addAlert('danger',
                    `顶棚变形超限(JC模型): ${sim.roof_max_deformation_mm.toFixed(2)}mm > ${this.deformationThreshold}mm`);
            }
            if (sim.is_penetrated) {
                this.addAlert('danger',
                    `防护层击穿(JC动态韧性): ${sim.penetration_depth_mm.toFixed(2)}mm`);
            }
            this.drawHeatmap();
        } catch (e) {
            this.addAlert('warn', '无法连接后端，使用本地Johnson-Cook仿真模型');
            this._runLocalSimulation();
        }
    }

    _runLocalSimulation() {
        const mass = 20 + Math.random() * 180;
        const velocity = 10 + Math.random() * 25;
        const impactEnergy = 0.5 * mass * velocity * velocity;

        const strainRate = velocity / 0.08;
        const JC_A = 60e6, JC_B = 120e6, JC_n = 0.45, JC_C = 0.06;
        const srFactor = 1.0 + JC_C * Math.log(Math.max(strainRate, 1.0));
        const dynYield = JC_A * srFactor;

        const eps_p = Math.pow(Math.max(0, impactEnergy / 50000 - JC_A / JC_B), 1 / JC_n);
        const deformation = 2.0 + eps_p * 80 + Math.random() * 8;
        const stress = (JC_A + JC_B * Math.pow(Math.max(eps_p, 1e-4), JC_n)) * srFactor / 1e6;

        const penetrated = deformation > 22;
        const damage = deformation < 5 ? 0 : deformation < 10 ? 1 : deformation < 16 ? 2 : deformation < 24 ? 3 : 4;
        const modes = ['bending', 'shear', 'punching', 'combined'];

        const sim = {
            roof_max_deformation_mm: deformation,
            roof_plastic_strain: eps_p,
            roof_von_mises_stress_mpa: Math.min(stress, 350),
            impact_energy_j: impactEnergy,
            absorbed_energy_j: impactEnergy * (0.35 + Math.random() * 0.5),
            damage_level: damage,
            penetration_depth_mm: deformation * 0.82,
            is_penetrated: penetrated,
            failure_mode: modes[Math.floor(Math.random() * 4)],
            strain_rate: strainRate,
            deformation_field: this._generateField(deformation),
            stress_field: this._generateField(Math.min(stress, 350)),
        };

        this._updateSimulationDisplay(sim);
        this.cart.setDeformationField(sim.deformation_field, false);
        this.cart.setDeformationField(sim.stress_field, true);

        document.getElementById('m-roof-stress').textContent = sim.roof_von_mises_stress_mpa.toFixed(1);
        document.getElementById('m-impact').textContent = (mass * velocity / 100).toFixed(1);
        document.getElementById('m-mass').textContent = mass.toFixed(1);
        document.getElementById('m-velocity').textContent = velocity.toFixed(1);
        const sr = document.getElementById('m-strain-rate');
        if (sr) sr.textContent = Math.round(sim.strain_rate) + '/s';

        this._addHistoryData(sim.roof_von_mises_stress_mpa, mass * velocity / 100);
        this.drawHeatmap();
        this.drawCharts();

        if (deformation > this.deformationThreshold) {
            this.addAlert('danger',
                `[JC 应变率${Math.round(strainRate)}/s] 变形超限: ${deformation.toFixed(2)}mm`);
        }
        if (penetrated) {
            this.addAlert('danger',
                `[JC动态屈服${Math.round(dynYield / 1e6)}MPa] 防护击穿！`);
        }
    }

    _generateField(maxVal) {
        const n = 10;
        const field = new Array(n * n).fill(0);
        const cx = Math.floor(n / 2) + (Math.random() - 0.5) * 3;
        const cy = Math.floor(n / 2) + (Math.random() - 0.5) * 3;
        const sigma = 2 + Math.random() * 2;
        for (let i = 0; i < n; i++) {
            for (let j = 0; j < n; j++) {
                const d2 = (i - cx) ** 2 + (j - cy) ** 2;
                field[i * n + j] = maxVal * Math.exp(-d2 / (2 * sigma * sigma));
            }
        }
        return field;
    }

    _updateSimulationDisplay(sim) {
        const defEl = document.getElementById('hud-deformation');
        const stressEl = document.getElementById('hud-stress');
        const energyEl = document.getElementById('hud-energy');
        const dmgEl = document.getElementById('hud-damage');

        if (defEl) {
            defEl.innerHTML = `${sim.roof_max_deformation_mm.toFixed(2)}<span style="font-size:12px">mm</span>`;
            defEl.className = 'hud-value ' + (
                sim.roof_max_deformation_mm > this.deformationThreshold ? 'hud-danger' :
                sim.roof_max_deformation_mm > this.deformationThreshold * 0.7 ? 'hud-warn' : 'hud-safe');
        }
        if (stressEl) {
            stressEl.innerHTML = `${sim.roof_von_mises_stress_mpa.toFixed(2)}<span style="font-size:12px">MPa</span>`;
            stressEl.className = 'hud-value ' + (
                sim.roof_von_mises_stress_mpa > 200 ? 'hud-danger' :
                sim.roof_von_mises_stress_mpa > 120 ? 'hud-warn' : 'hud-safe');
        }
        if (energyEl) {
            energyEl.innerHTML = `${sim.impact_energy_j.toFixed(0)}<span style="font-size:12px">J</span>`;
        }
        if (dmgEl) {
            const dl = DAMAGE_LEVELS[Math.min(sim.damage_level, 4)];
            dmgEl.textContent = `${sim.damage_level} ${dl.label}`;
            dmgEl.className = `hud-value ${dl.cls}`;
        }

        const penEl = document.getElementById('m-penetration');
        if (penEl) penEl.textContent = sim.penetration_depth_mm.toFixed(2);

        const modeEl = document.getElementById('m-failure');
        const modeNames = { bending: '弯曲破坏', shear: '剪切破坏', punching: '冲切破坏', combined: '组合破坏' };
        if (modeEl) modeEl.textContent = modeNames[sim.failure_mode] || sim.failure_mode;

        const srEl = document.getElementById('m-strain-rate');
        if (srEl && sim.strain_rate != null) srEl.textContent = Math.round(sim.strain_rate) + '/s';

        this._addHistoryData(sim.roof_von_mises_stress_mpa, sim.impact_energy_j / 500);
        this.drawCharts();
    }

    async runAHPEvaluation() {
        this.addAlert('info', '执行群决策AHP评估(5专家+一致性修正)...');
        try {
            const res = await fetch(`${this.apiBase}/api/evaluate`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ vehicle_id: this.currentVehicle }),
            });
            const data = await res.json();
            this._updateAHPEvaluation(data.data || [], data);
        } catch (e) {
            const dummy = [
                { material_type: 'composite', material_thickness_mm: 48, energy_absorption_score: 0.85, structural_strength_score: 0.82, weight_factor_score: 0.7, cost_factor_score: 0.5, durability_score: 0.8, ahp_weight_score: 0.76, rank_position: 1, is_recommended: true },
                { material_type: 'iron',      material_thickness_mm: 5,  energy_absorption_score: 0.9,  structural_strength_score: 0.95, weight_factor_score: 0.2, cost_factor_score: 0.2, durability_score: 0.95, ahp_weight_score: 0.68, rank_position: 2, is_recommended: false },
                { material_type: 'wood',      material_thickness_mm: 64, energy_absorption_score: 0.45, structural_strength_score: 0.5,  weight_factor_score: 0.85,cost_factor_score: 0.95,durability_score: 0.55, ahp_weight_score: 0.63, rank_position: 3, is_recommended: false },
                { material_type: 'cowhide',   material_thickness_mm: 20, energy_absorption_score: 0.55, structural_strength_score: 0.3,  weight_factor_score: 0.9, cost_factor_score: 0.7, durability_score: 0.35, ahp_weight_score: 0.54, rank_position: 4, is_recommended: false },
            ];
            this._updateAHPEvaluation(dummy, {
                consistency_ratio: 0.06,
                group_consensus_index: 0.89,
                passed_experts: 5,
                total_experts: 5,
            });
        }
    }

    _updateAHPEvaluation(evals, meta = {}) {
        const tbody = document.querySelector('#ahp-table tbody');
        if (!tbody) return;
        tbody.innerHTML = '';
        const matNames = { wood: '木材', cowhide: '牛皮', iron: '铁皮', composite: '复合' };
        evals.forEach(e => {
            const tr = document.createElement('tr');
            if (e.is_recommended) tr.className = 'recommended';
            tr.innerHTML = `
                <td>${e.is_recommended ? '<span class="badge badge-rec">推荐</span> ' : ''}${matNames[e.material_type] || e.material_type}</td>
                <td>${e.material_thickness_mm}</td>
                <td>${(e.energy_absorption_score * 100).toFixed(0)}</td>
                <td>${(e.structural_strength_score * 100).toFixed(0)}</td>
                <td>${(e.weight_factor_score * 100).toFixed(0)}</td>
                <td>${(e.cost_factor_score * 100).toFixed(0)}</td>
                <td>${(e.durability_score * 100).toFixed(0)}</td>
                <td><b>${(e.ahp_weight_score * 100).toFixed(1)}</b></td>
            `;
            tbody.appendChild(tr);
        });

        let msg = `AHP完成：${evals.length}方案`;
        if (meta.consistency_ratio != null) msg += `，一致性CR=${meta.consistency_ratio.toFixed(3)}`;
        if (meta.group_consensus_index != null) msg += `，专家共识=${(meta.group_consensus_index * 100).toFixed(0)}%`;
        if (meta.passed_experts != null) msg += `，${meta.passed_experts}/${meta.total_experts || 5}专家通过`;
        this.addAlert('info', msg);
    }

    _addHistoryData(stress, impact) {
        this.stressHistory.push(stress);
        this.impactHistory.push(impact);
        if (this.stressHistory.length > 60) this.stressHistory.shift();
        if (this.impactHistory.length > 60) this.impactHistory.shift();
    }

    drawCharts() {
        this._drawLineChart('chart-stress', this.stressHistory, '#44aaff', 0, 300);
        this._drawLineChart('chart-impact', this.impactHistory, '#ffaa44', 0, 120);
    }

    _drawLineChart(canvasId, data, color, min, max) {
        const canvas = document.getElementById(canvasId);
        if (!canvas) return;
        if (color.length === 4) {
            color = '#' + color[1] + color[1] + color[2] + color[2] + color[3] + color[3];
        }
        const ctx = canvas.getContext('2d');
        const w = canvas.parentElement.clientWidth;
        const h = canvas.parentElement.clientHeight;
        if (w < 10 || h < 10) return;
        canvas.width = w * devicePixelRatio;
        canvas.height = h * devicePixelRatio;
        canvas.style.width = w + 'px';
        canvas.style.height = h + 'px';
        ctx.scale(devicePixelRatio, devicePixelRatio);

        ctx.fillStyle = '#1a2040';
        ctx.fillRect(0, 0, w, h);

        ctx.strokeStyle = '#2a3050';
        ctx.lineWidth = 1;
        for (let i = 0; i <= 4; i++) {
            const y = h - (h * i / 4);
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(w, y);
            ctx.stroke();
        }

        if (data.length < 2) return;

        const range = max - min;
        ctx.strokeStyle = color;
        ctx.lineWidth = 2;
        ctx.beginPath();
        for (let i = 0; i < data.length; i++) {
            const x = (i / (data.length - 1)) * w;
            const y = h - ((data[i] - min) / range) * h * 0.9 - h * 0.05;
            if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
        }
        ctx.stroke();

        const grad = ctx.createLinearGradient(0, 0, 0, h);
        grad.addColorStop(0, color + '55');
        grad.addColorStop(1, color + '00');
        ctx.fillStyle = grad;
        ctx.lineTo(w, h);
        ctx.lineTo(0, h);
        ctx.closePath();
        ctx.fill();
    }

    drawHeatmap() {
        const canvas = document.getElementById('chart-heatmap');
        if (!canvas) return;
        const ctx = canvas.getContext('2d');
        const w = canvas.parentElement.clientWidth;
        const h = canvas.parentElement.clientHeight;
        if (w < 10) return;
        canvas.width = w * devicePixelRatio;
        canvas.height = h * devicePixelRatio;
        canvas.style.width = w + 'px';
        canvas.style.height = h + 'px';
        ctx.scale(devicePixelRatio, devicePixelRatio);

        ctx.fillStyle = '#1a2040';
        ctx.fillRect(0, 0, w, h);

        const field = this.cart.state?.showStress
            ? (this.cart.state.stressField || [])
            : (this.cart.state?.deformationField || []);

        const n = 10;
        if (!field || field.length !== n * n) return;

        let max = 0;
        field.forEach(v => { if (v > max) max = v; });
        if (max <= 0) max = 1;

        const cellW = w / n;
        const cellH = h / n;

        const colormap = t => {
            t = Math.max(0, Math.min(1, t));
            const stops = [[0,[0,0,1]],[0.25,[0,1,1]],[0.5,[0,1,0]],[0.75,[1,1,0]],[1,[1,0,0]]];
            for (let i = 0; i < stops.length - 1; i++) {
                if (t >= stops[i][0] && t <= stops[i + 1][0]) {
                    const f = (t - stops[i][0]) / (stops[i + 1][0] - stops[i][0]);
                    const c0 = stops[i][1], c1 = stops[i + 1][1];
                    return `rgb(${Math.floor((c0[0]+(c1[0]-c0[0])*f)*255)},${Math.floor((c0[1]+(c1[1]-c0[1])*f)*255)},${Math.floor((c0[2]+(c1[2]-c0[2])*f)*255)})`;
                }
            }
            return 'rgb(255,0,0)';
        };

        for (let i = 0; i < n; i++) {
            for (let j = 0; j < n; j++) {
                const val = field[i * n + j];
                ctx.fillStyle = colormap(val / max);
                ctx.fillRect(j * cellW, i * cellH, cellW + 1, cellH + 1);
            }
        }

        ctx.strokeStyle = '#00000040';
        ctx.lineWidth = 1;
        for (let i = 0; i <= n; i++) {
            ctx.beginPath();
            ctx.moveTo(i * cellW, 0);
            ctx.lineTo(i * cellW, h);
            ctx.stroke();
            ctx.beginPath();
            ctx.moveTo(0, i * cellH);
            ctx.lineTo(w, i * cellH);
            ctx.stroke();
        }
    }

    addAlert(type, message) {
        const list = document.getElementById('alert-list');
        if (!list) return;
        const item = document.createElement('div');
        const cls = type === 'danger' ? '' : type === 'warn' ? 'warn' : 'info';
        const now = new Date();
        const time = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
        item.className = `alert-item ${cls}`;
        item.innerHTML = `<div class="alert-time">${time}</div><div>${message}</div>`;
        list.insertBefore(item, list.firstChild);
        while (list.children.length > 25) list.removeChild(list.lastChild);
    }

    init() {
        setTimeout(() => {
            this._runLocalSimulation();
            this.runAHPEvaluation();
        }, 500);
    }
}

export default ProtectionPanel;
