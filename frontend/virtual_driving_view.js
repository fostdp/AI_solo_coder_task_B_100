export class VirtualDrivingView {
    constructor(containerSelector, apiBase) {
        this.container = document.querySelector(containerSelector);
        if (!this.container) throw new Error('Container not found: ' + containerSelector);
        this.apiBase = apiBase || 'http://127.0.0.1:8080';

        this.sessionId = null;
        this.nickname = '游客';
        this.vehicleType = 'fenyun_basic';

        this.inputState = {
            throttle: 0,
            steer: 0,
            brake: false,
        };

        this.vehicleState = {
            position_x: 0,
            position_y: 0,
            heading_deg: 0,
            speed_ms: 0,
            health_percent: 100,
            armor_integrity_percent: 100,
            impacts_received: 0,
            distance_traveled_m: 0,
        };

        this.vehicleProfile = {
            display_name: '轒辒车(基础型)',
            length_m: 6.5,
            width_m: 2.8,
            roof_thickness_mm: 80,
            primary_material: '复合材料(皮-木-铁层合)',
        };

        this.activeAttacks = [];
        this.flashEffects = [];
        this.trailPoints = [];

        this._gameLoopTimer = null;
        this._boundKeyDown = null;
        this._boundKeyUp = null;

        this.dom = {};
        this.canvas = null;
        this.ctx = null;
    }

    async init() {
        await this._createSession();
        this._buildLayout();
        this._bindKeyboardEvents();
        this._startGameLoop();
    }

    async _createSession() {
        try {
            const res = await fetch(`${this.apiBase}/api/user/session/create`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ nickname: '游客', vehicle_type: 'fenyun_basic' }),
            });
            const data = await res.json();
            this.sessionId = data.session_id;
            this.nickname = data.nickname || '游客';
            this.vehicleType = data.vehicle_type || 'fenyun_basic';
        } catch (e) {
            this.sessionId = 'local-' + Math.random().toString(36).slice(2, 10);
            console.warn('使用本地模拟会话:', this.sessionId);
        }
    }

    _buildLayout() {
        this.container.innerHTML = '';
        Object.assign(this.container.style, {
            width: '100%',
            height: '100%',
            display: 'grid',
            gridTemplateRows: '60px 1fr 200px',
            gap: '1px',
            background: '#1a2040',
            fontFamily: '"Microsoft YaHei", "PingFang SC", sans-serif',
            color: '#e0e8ff',
            overflow: 'hidden',
        });

        this._buildHeader();
        this._buildMain();
        this._buildControls();
    }

    _buildHeader() {
        const header = document.createElement('div');
        Object.assign(header.style, {
            display: 'flex',
            alignItems: 'center',
            padding: '0 24px',
            background: 'linear-gradient(90deg, #1a1a3a 0%, #2a1a4a 100%)',
            borderBottom: '2px solid #4a5aaa',
            gap: '24px',
        });

        const title = document.createElement('h1');
        title.textContent = '虚拟驾驶 · 轒辒车攻城体验';
        Object.assign(title.style, {
            fontSize: '18px',
            fontWeight: '600',
            background: 'linear-gradient(90deg, #6af, #a6f)',
            WebkitBackgroundClip: 'text',
            WebkitTextFillColor: 'transparent',
            letterSpacing: '2px',
            margin: 0,
        });
        header.appendChild(title);

        const infoBar = document.createElement('div');
        Object.assign(infoBar.style, {
            display: 'flex',
            gap: '20px',
            marginLeft: 'auto',
            fontSize: '13px',
        });

        const sessionInfo = this._makeInfoItem('会话', this.sessionId ? this.sessionId.slice(0, 8) + '...' : '--');
        const vehicleInfo = this._makeInfoItem('车辆', this.vehicleProfile.display_name);
        const userInfo = this._makeInfoItem('用户', this.nickname);

        this.dom.sessionId = sessionInfo.valueEl;
        this.dom.vehicleType = vehicleInfo.valueEl;
        this.dom.nickname = userInfo.valueEl;

        infoBar.appendChild(sessionInfo.el);
        infoBar.appendChild(vehicleInfo.el);
        infoBar.appendChild(userInfo.el);
        header.appendChild(infoBar);

        this.container.appendChild(header);
    }

    _makeInfoItem(label, value) {
        const el = document.createElement('div');
        Object.assign(el.style, {
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            padding: '4px 12px',
            background: 'rgba(30, 40, 80, 0.6)',
            borderRadius: '4px',
            border: '1px solid #3a4070',
        });
        const labelEl = document.createElement('span');
        labelEl.textContent = label;
        Object.assign(labelEl.style, { color: '#8899bb', fontSize: '12px' });
        const valueEl = document.createElement('span');
        valueEl.textContent = value;
        Object.assign(valueEl.style, {
            fontWeight: '600',
            fontFamily: '"Courier New", monospace',
            color: '#4af',
        });
        el.appendChild(labelEl);
        el.appendChild(valueEl);
        return { el, valueEl };
    }

    _buildMain() {
        const main = document.createElement('div');
        Object.assign(main.style, {
            display: 'grid',
            gridTemplateColumns: '1fr 340px',
            gap: '1px',
            minHeight: 0,
        });

        this._buildViewport(main);
        this._buildHUDPanel(main);

        this.container.appendChild(main);
    }

    _buildViewport(parent) {
        const viewport = document.createElement('div');
        Object.assign(viewport.style, {
            position: 'relative',
            background: 'radial-gradient(ellipse at center, #0d1525 0%, #050810 100%)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            overflow: 'hidden',
        });

        this.canvas = document.createElement('canvas');
        this.canvas.width = 800;
        this.canvas.height = 500;
        Object.assign(this.canvas.style, {
            width: '100%',
            height: '100%',
            maxWidth: '100%',
            maxHeight: '100%',
            objectFit: 'contain',
        });
        this.ctx = this.canvas.getContext('2d');

        const hpBarWrap = document.createElement('div');
        Object.assign(hpBarWrap.style, {
            position: 'absolute',
            top: '16px',
            left: '16px',
            right: '16px',
            display: 'flex',
            flexDirection: 'column',
            gap: '6px',
            zIndex: '10',
        });

        this.dom.hpBar = this._makeStatusBar('车辆HP', 100, 'hp');
        this.dom.armorBar = this._makeStatusBar('装甲完整度', 100, 'armor');
        hpBarWrap.appendChild(this.dom.hpBar.el);
        hpBarWrap.appendChild(this.dom.armorBar.el);

        viewport.appendChild(hpBarWrap);
        viewport.appendChild(this.canvas);
        parent.appendChild(viewport);
    }

    _makeStatusBar(label, value, type) {
        const el = document.createElement('div');
        Object.assign(el.style, {
            background: 'rgba(20, 25, 50, 0.85)',
            border: '1px solid #3a4070',
            borderRadius: '4px',
            padding: '6px 10px',
            backdropFilter: 'blur(6px)',
        });
        const labelRow = document.createElement('div');
        Object.assign(labelRow.style, {
            display: 'flex',
            justifyContent: 'space-between',
            marginBottom: '4px',
            fontSize: '11px',
        });
        const labelEl = document.createElement('span');
        labelEl.textContent = label;
        Object.assign(labelEl.style, { color: '#8899bb', letterSpacing: '1px' });
        const valueEl = document.createElement('span');
        valueEl.textContent = value.toFixed(1) + '%';
        Object.assign(valueEl.style, {
            fontFamily: '"Courier New", monospace',
            fontWeight: '700',
        });
        labelRow.appendChild(labelEl);
        labelRow.appendChild(valueEl);

        const barBg = document.createElement('div');
        Object.assign(barBg.style, {
            width: '100%',
            height: '8px',
            background: '#1a2040',
            borderRadius: '4px',
            overflow: 'hidden',
        });
        const barFill = document.createElement('div');
        Object.assign(barFill.style, {
            width: value + '%',
            height: '100%',
            transition: 'width 0.1s, background 0.3s',
        });
        barBg.appendChild(barFill);

        el.appendChild(labelRow);
        el.appendChild(barBg);

        this._updateBarColor(barFill, value, type);

        return { el, valueEl, barFill, type };
    }

    _updateBarColor(barFill, value, type) {
        const pct = Math.max(0, Math.min(100, value)) / 100;
        let color;
        if (pct > 0.6) {
            color = `rgb(${Math.floor(80 * (1 - pct) * 2.5)}, ${Math.floor(200 + 55 * pct)}, ${Math.floor(80 * (1 - pct))})`;
        } else if (pct > 0.3) {
            color = `rgb(${Math.floor(200 + 55 * (0.6 - pct) / 0.3)}, ${Math.floor(180 * pct / 0.6)}, 40)`;
        } else {
            color = `rgb(${Math.floor(200 + 55 * pct / 0.3)}, ${Math.floor(60 * pct / 0.3)}, ${Math.floor(40 * pct / 0.3)})`;
        }
        barFill.style.background = `linear-gradient(90deg, ${color}dd, ${color})`;
    }

    _buildHUDPanel(parent) {
        const panel = document.createElement('div');
        Object.assign(panel.style, {
            background: '#14182a',
            padding: '16px',
            overflowY: 'auto',
        });

        const title = document.createElement('div');
        title.textContent = '车辆状态 HUD';
        Object.assign(title.style, {
            fontSize: '13px',
            color: '#8af',
            letterSpacing: '2px',
            marginBottom: '12px',
            paddingBottom: '8px',
            borderBottom: '1px solid #2a3050',
            textTransform: 'uppercase',
        });
        panel.appendChild(title);

        this._addMetricRow(panel, '受冲击次数', '0', '次', 'impacts');
        this._addMetricRow(panel, '行驶距离', '0.0', 'm', 'distance');
        this._addMetricRow(panel, '当前速度', '0.0', 'm/s', 'speed');

        const infoTitle = document.createElement('div');
        infoTitle.textContent = '车辆基础信息';
        Object.assign(infoTitle.style, {
            fontSize: '13px',
            color: '#8af',
            letterSpacing: '2px',
            margin: '20px 0 12px 0',
            paddingBottom: '8px',
            borderBottom: '1px solid #2a3050',
            textTransform: 'uppercase',
        });
        panel.appendChild(infoTitle);

        this._addMetricRow(panel, '车长', this.vehicleProfile.length_m.toFixed(1), 'm', null);
        this._addMetricRow(panel, '车宽', this.vehicleProfile.width_m.toFixed(1), 'm', null);
        this._addMetricRow(panel, '装甲厚度', this.vehicleProfile.roof_thickness_mm.toFixed(0), 'mm', null);
        this._addMetricRow(panel, '材料', this.vehicleProfile.primary_material, '', null);

        parent.appendChild(panel);
    }

    _addMetricRow(parent, label, value, unit, key) {
        const row = document.createElement('div');
        Object.assign(row.style, {
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            padding: '8px 0',
            borderBottom: '1px dashed #222a44',
        });
        const labelEl = document.createElement('span');
        labelEl.textContent = label;
        Object.assign(labelEl.style, { fontSize: '12px', color: '#8899bb' });

        const valWrap = document.createElement('span');
        const valEl = document.createElement('span');
        valEl.textContent = value;
        Object.assign(valEl.style, {
            fontSize: '16px',
            fontWeight: '600',
            fontFamily: '"Courier New", monospace',
            color: '#fff',
        });
        const unitEl = document.createElement('span');
        unitEl.textContent = unit;
        Object.assign(unitEl.style, { fontSize: '11px', color: '#667', marginLeft: '4px' });
        valWrap.appendChild(valEl);
        if (unit) valWrap.appendChild(unitEl);

        row.appendChild(labelEl);
        row.appendChild(valWrap);
        parent.appendChild(row);

        if (key) {
            this.dom[key] = valEl;
        }
    }

    _buildControls() {
        const ctrl = document.createElement('div');
        Object.assign(ctrl.style, {
            background: '#14182a',
            padding: '12px 20px',
            display: 'grid',
            gridTemplateColumns: 'repeat(3, 1fr) auto',
            gap: '16px',
            borderTop: '2px solid #2a3050',
            alignItems: 'center',
        });

        this._buildSliderControl(ctrl, '油门', 'throttle', -0.5, 1.0, 0);
        this._buildSliderControl(ctrl, '方向', 'steer', -1.0, 1.0, 0);
        this._buildButtonGroup(ctrl);
        this._buildKeyboardHint(ctrl);

        this.container.appendChild(ctrl);
    }

    _buildSliderControl(parent, label, key, min, max, val) {
        const wrap = document.createElement('div');
        Object.assign(wrap.style, {
            display: 'flex',
            flexDirection: 'column',
            gap: '6px',
        });

        const labelRow = document.createElement('div');
        Object.assign(labelRow.style, {
            display: 'flex',
            justifyContent: 'space-between',
            fontSize: '12px',
        });
        const labelEl = document.createElement('span');
        labelEl.textContent = label;
        Object.assign(labelEl.style, { color: '#8899bb', letterSpacing: '1px' });
        const valEl = document.createElement('span');
        valEl.textContent = val.toFixed(2);
        Object.assign(valEl.style, {
            fontFamily: '"Courier New", monospace',
            color: '#4af',
            fontWeight: '600',
        });
        labelRow.appendChild(labelEl);
        labelRow.appendChild(valEl);

        const slider = document.createElement('input');
        slider.type = 'range';
        slider.min = min;
        slider.max = max;
        slider.step = 0.05;
        slider.value = val;
        Object.assign(slider.style, {
            width: '100%',
            accentColor: '#5a7aff',
        });

        slider.addEventListener('input', (e) => {
            const v = parseFloat(e.target.value);
            valEl.textContent = v.toFixed(2);
            this.inputState[key] = v;
        });

        this.dom[key + 'Slider'] = slider;
        this.dom[key + 'Value'] = valEl;

        wrap.appendChild(labelRow);
        wrap.appendChild(slider);
        parent.appendChild(wrap);
    }

    _buildButtonGroup(parent) {
        const btnWrap = document.createElement('div');
        Object.assign(btnWrap.style, {
            display: 'flex',
            flexDirection: 'column',
            gap: '6px',
        });

        const row1 = document.createElement('div');
        Object.assign(row1.style, { display: 'flex', gap: '8px' });
        const row2 = document.createElement('div');
        Object.assign(row2.style, { display: 'flex', gap: '8px' });

        const brakeBtn = this._makeButton('急刹车', '#8a4a4a');
        const attackBtn = this._makeButton('💥 手动触发攻击', '#8a6a2a');
        const resetBtn = this._makeButton('↻ 重置位置', '#4a6a8a');

        brakeBtn.addEventListener('click', () => {
            this.inputState.brake = true;
            setTimeout(() => { this.inputState.brake = false; }, 500);
        });
        attackBtn.addEventListener('click', () => this._manualAttack());
        resetBtn.addEventListener('click', () => this._resetVehicle());

        row1.appendChild(brakeBtn);
        row1.appendChild(attackBtn);
        row2.appendChild(resetBtn);

        btnWrap.appendChild(row1);
        btnWrap.appendChild(row2);
        parent.appendChild(btnWrap);
    }

    _makeButton(text, bgColor) {
        const btn = document.createElement('button');
        btn.textContent = text;
        Object.assign(btn.style, {
            padding: '8px 16px',
            background: bgColor || 'linear-gradient(90deg, #3a4a8a, #5a3a8a)',
            border: 'none',
            color: '#e0e8ff',
            borderRadius: '4px',
            fontSize: '13px',
            fontWeight: '600',
            letterSpacing: '1px',
            cursor: 'pointer',
            transition: 'all 0.2s',
            whiteSpace: 'nowrap',
        });
        btn.addEventListener('mouseenter', () => {
            btn.style.boxShadow = '0 0 20px rgba(100, 80, 200, 0.4)';
            btn.style.filter = 'brightness(1.15)';
        });
        btn.addEventListener('mouseleave', () => {
            btn.style.boxShadow = 'none';
            btn.style.filter = 'none';
        });
        return btn;
    }

    _buildKeyboardHint(parent) {
        const hint = document.createElement('div');
        Object.assign(hint.style, {
            fontSize: '11px',
            color: '#668',
            lineHeight: '1.8',
            textAlign: 'right',
            paddingLeft: '10px',
            borderLeft: '1px solid #2a3050',
        });
        hint.innerHTML = `
            <div><b style="color:#8af">键盘操作</b></div>
            <div><kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">W</kbd> 前进 &nbsp;
                 <kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">S</kbd> 后退</div>
            <div><kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">A</kbd> 左转 &nbsp;
                 <kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">D</kbd> 右转</div>
            <div><kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">Space</kbd> 刹车 &nbsp;
                 <kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">R</kbd> 重置</div>
            <div><kbd style="background:#1e2544;padding:2px 6px;border-radius:3px;border:1px solid #3a4070">Enter</kbd> 攻击(威力×2)</div>
        `;
        parent.appendChild(hint);
    }

    _bindKeyboardEvents() {
        this._boundKeyDown = (e) => this._handleKeyDown(e);
        this._boundKeyUp = (e) => this._handleKeyUp(e);
        window.addEventListener('keydown', this._boundKeyDown);
        window.addEventListener('keyup', this._boundKeyUp);
    }

    _handleKeyDown(e) {
        switch (e.key) {
            case 'w': case 'W': case 'ArrowUp':
                this.inputState.throttle = 1.0;
                this._updateSliderUI('throttle', 1.0);
                e.preventDefault();
                break;
            case 's': case 'S': case 'ArrowDown':
                this.inputState.throttle = -0.5;
                this._updateSliderUI('throttle', -0.5);
                e.preventDefault();
                break;
            case 'a': case 'A': case 'ArrowLeft':
                this.inputState.steer = -1.0;
                this._updateSliderUI('steer', -1.0);
                e.preventDefault();
                break;
            case 'd': case 'D': case 'ArrowRight':
                this.inputState.steer = 1.0;
                this._updateSliderUI('steer', 1.0);
                e.preventDefault();
                break;
            case ' ':
                this.inputState.brake = true;
                e.preventDefault();
                break;
            case 'r': case 'R':
                this._resetVehicle();
                break;
            case 'Enter':
                this._manualAttack();
                break;
        }
    }

    _handleKeyUp(e) {
        switch (e.key) {
            case 'w': case 'W': case 'ArrowUp':
            case 's': case 'S': case 'ArrowDown':
                this.inputState.throttle = 0;
                this._updateSliderUI('throttle', 0);
                break;
            case 'a': case 'A': case 'ArrowLeft':
            case 'd': case 'D': case 'ArrowRight':
                this.inputState.steer = 0;
                this._updateSliderUI('steer', 0);
                break;
            case ' ':
                this.inputState.brake = false;
                break;
        }
    }

    _updateSliderUI(key, value) {
        if (this.dom[key + 'Slider']) {
            this.dom[key + 'Slider'].value = value;
        }
        if (this.dom[key + 'Value']) {
            this.dom[key + 'Value'].textContent = value.toFixed(2);
        }
    }

    _startGameLoop() {
        this._gameLoopTimer = setInterval(() => this._gameLoopTick(), 60);
    }

    async _gameLoopTick() {
        const dt = 0.06;

        try {
            await fetch(`${this.apiBase}/api/user/session/action`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    session_id: this.sessionId,
                    action: this.inputState.brake ? 'brake' : 'drive',
                    param1: this.inputState.throttle,
                    param2: this.inputState.steer,
                }),
            });
        } catch (e) {}

        try {
            const res = await fetch(`${this.apiBase}/api/user/session/tick`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    session_id: this.sessionId,
                    dt_seconds: dt,
                }),
            });
            const data = await res.json();
            if (data.state) {
                this.vehicleState = { ...this.vehicleState, ...data.state };
            }
            if (data.attacks && Array.isArray(data.attacks)) {
                data.attacks.forEach(atk => {
                    this.activeAttacks.push({
                        ...atk,
                        spawnTime: performance.now(),
                        currentY: 80,
                    });
                    this.flashEffects.push({
                        x: atk.impact_x,
                        y: atk.impact_y,
                        startTime: performance.now(),
                        duration: 500,
                    });
                });
            }
        } catch (e) {
            this._simulateLocalTick(dt);
        }

        this.trailPoints.push({
            x: this.vehicleState.position_x,
            y: this.vehicleState.position_y,
            time: performance.now(),
        });
        if (this.trailPoints.length > 100) this.trailPoints.shift();

        const now = performance.now();
        this.activeAttacks = this.activeAttacks.filter(atk => {
            const elapsed = (now - atk.spawnTime) / 1000;
            atk.currentY = 80 - elapsed * 120;
            return atk.currentY > -10;
        });
        this.flashEffects = this.flashEffects.filter(f => now - f.startTime < f.duration);

        this._updateHUD(this.vehicleState);
        this._drawScene();
    }

    _simulateLocalTick(dt) {
        const throttle = this.inputState.brake ? 0 : this.inputState.throttle;
        const steer = this.inputState.steer;
        const accel = throttle * 3.0;
        if (this.inputState.brake) {
            this.vehicleState.speed_ms *= 0.9;
        } else {
            this.vehicleState.speed_ms += accel * dt;
        }
        this.vehicleState.speed_ms = Math.max(-2, Math.min(4, this.vehicleState.speed_ms));

        this.vehicleState.heading_deg += steer * this.vehicleState.speed_ms * dt * 30;
        const rad = (this.vehicleState.heading_deg * Math.PI) / 180;
        const dx = Math.cos(rad) * this.vehicleState.speed_ms * dt;
        const dy = Math.sin(rad) * this.vehicleState.speed_ms * dt;
        this.vehicleState.position_x += dx;
        this.vehicleState.position_y += dy;
        this.vehicleState.distance_traveled_m += Math.hypot(dx, dy);

        if (Math.random() < 0.02) {
            const atk = {
                event_id: Date.now(),
                impact_x: this.vehicleState.position_x + (Math.random() - 0.5) * 4,
                impact_y: this.vehicleState.position_y + (Math.random() - 0.5) * 2,
                rock_mass_kg: 20 + Math.random() * 50,
                rock_velocity_ms: 10 + Math.random() * 15,
                damage_dealt: 2 + Math.random() * 8,
                timestamp_ms: Date.now(),
            };
            this.activeAttacks.push({
                ...atk,
                spawnTime: performance.now(),
                currentY: 80,
            });
            this.flashEffects.push({
                x: atk.impact_x,
                y: atk.impact_y,
                startTime: performance.now(),
                duration: 500,
            });
            this.vehicleState.impacts_received++;
            this.vehicleState.health_percent = Math.max(0, this.vehicleState.health_percent - atk.damage_dealt);
            this.vehicleState.armor_integrity_percent = Math.max(0, this.vehicleState.armor_integrity_percent - atk.damage_dealt * 0.7);
        }
    }

    async _manualAttack() {
        try {
            const res = await fetch(`${this.apiBase}/api/user/session/attack`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    session_id: this.sessionId,
                    force_multiplier: 2.0,
                }),
            });
            const data = await res.json();
            if (data.state) {
                this.vehicleState = { ...this.vehicleState, ...data.state };
            }
            if (data.attacks && Array.isArray(data.attacks)) {
                data.attacks.forEach(atk => {
                    this.activeAttacks.push({
                        ...atk,
                        spawnTime: performance.now(),
                        currentY: 80,
                    });
                    this.flashEffects.push({
                        x: atk.impact_x,
                        y: atk.impact_y,
                        startTime: performance.now(),
                        duration: 600,
                        intensity: 2.0,
                    });
                });
            }
        } catch (e) {
            for (let i = 0; i < 3; i++) {
                const atk = {
                    event_id: Date.now() + i,
                    impact_x: this.vehicleState.position_x + (Math.random() - 0.5) * 6,
                    impact_y: this.vehicleState.position_y + (Math.random() - 0.5) * 4,
                    rock_mass_kg: 60 + Math.random() * 80,
                    rock_velocity_ms: 15 + Math.random() * 20,
                    damage_dealt: 8 + Math.random() * 15,
                    timestamp_ms: Date.now(),
                };
                this.activeAttacks.push({
                    ...atk,
                    spawnTime: performance.now(),
                    currentY: 80,
                });
                this.flashEffects.push({
                    x: atk.impact_x,
                    y: atk.impact_y,
                    startTime: performance.now(),
                    duration: 600,
                    intensity: 2.0,
                });
                this.vehicleState.impacts_received++;
                this.vehicleState.health_percent = Math.max(0, this.vehicleState.health_percent - atk.damage_dealt);
                this.vehicleState.armor_integrity_percent = Math.max(0, this.vehicleState.armor_integrity_percent - atk.damage_dealt * 0.7);
            }
        }
    }

    async _resetVehicle() {
        try {
            await fetch(`${this.apiBase}/api/user/session/reset`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ session_id: this.sessionId }),
            });
        } catch (e) {}

        this.vehicleState = {
            position_x: 0,
            position_y: 0,
            heading_deg: 0,
            speed_ms: 0,
            health_percent: 100,
            armor_integrity_percent: 100,
            impacts_received: 0,
            distance_traveled_m: 0,
            timestamp_ms: Date.now(),
        };
        this.activeAttacks = [];
        this.flashEffects = [];
        this.trailPoints = [];

        this.inputState.throttle = 0;
        this.inputState.steer = 0;
        this.inputState.brake = false;
        this._updateSliderUI('throttle', 0);
        this._updateSliderUI('steer', 0);
    }

    _updateHUD(state) {
        this._updateBar(this.dom.hpBar, state.health_percent);
        this._updateBar(this.dom.armorBar, state.armor_integrity_percent);

        if (this.dom.impacts) this.dom.impacts.textContent = state.impacts_received;
        if (this.dom.distance) this.dom.distance.textContent = state.distance_traveled_m.toFixed(1);
        if (this.dom.speed) this.dom.speed.textContent = state.speed_ms.toFixed(2);
    }

    _updateBar(barData, value) {
        const v = Math.max(0, Math.min(100, value));
        barData.valueEl.textContent = v.toFixed(1) + '%';
        barData.barFill.style.width = v + '%';
        this._updateBarColor(barData.barFill, v, barData.type);

        if (v < 30) {
            barData.valueEl.style.color = '#f55';
            barData.valueEl.style.textShadow = '0 0 10px #f44';
        } else if (v < 60) {
            barData.valueEl.style.color = '#fa4';
            barData.valueEl.style.textShadow = '0 0 10px #fa4';
        } else {
            barData.valueEl.style.color = '#fff';
            barData.valueEl.style.textShadow = 'none';
        }
    }

    _drawScene() {
        const ctx = this.ctx;
        const W = this.canvas.width;
        const H = this.canvas.height;

        ctx.fillStyle = '#0a0e1a';
        ctx.fillRect(0, 0, W, H);

        const scale = 30;
        const offsetX = W / 2;
        const offsetY = H / 2 + 60;

        const vx = this.vehicleState.position_x;
        const vy = this.vehicleState.position_y;

        this._drawGrid(ctx, W, H, offsetX - vx * scale, offsetY + vy * scale, scale);
        this._drawWall(ctx, offsetX, offsetY, scale, vx, vy);
        this._drawTrail(ctx, scale, offsetX, offsetY, vx, vy);
        this._drawRocks(ctx, scale, offsetX, offsetY, vx, vy);
        this._drawVehicle(ctx, scale, offsetX, offsetY);
        this._drawFlashEffects(ctx, scale, offsetX, offsetY, vx, vy);
    }

    _drawGrid(ctx, W, H, offX, offY, scale) {
        ctx.strokeStyle = '#1a2040';
        ctx.lineWidth = 1;
        const gridSize = scale;

        const startX = ((offX % gridSize) + gridSize) % gridSize;
        const startY = ((offY % gridSize) + gridSize) % gridSize;

        for (let x = startX; x < W; x += gridSize) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, H);
            ctx.stroke();
        }
        for (let y = startY; y < H; y += gridSize) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(W, y);
            ctx.stroke();
        }
    }

    _drawWall(ctx, offX, offY, scale, vx, vy) {
        const wallWorldY = 25;
        const screenY = offY - (wallWorldY - vy) * scale;

        const wallGrad = ctx.createLinearGradient(0, screenY - 30, 0, screenY + 10);
        wallGrad.addColorStop(0, '#5a4030');
        wallGrad.addColorStop(0.5, '#7a5030');
        wallGrad.addColorStop(1, '#3a2820');

        ctx.fillStyle = wallGrad;
        ctx.fillRect(0, screenY - 30, this.canvas.width, 40);

        ctx.strokeStyle = '#2a1810';
        ctx.lineWidth = 1;
        for (let y = screenY - 25; y < screenY + 8; y += 8) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(this.canvas.width, y);
            ctx.stroke();
        }
        for (let i = 0; i < 20; i++) {
            const bx = ((i * 50 + (offX % 50)) % this.canvas.width);
            ctx.beginPath();
            ctx.moveTo(bx, screenY - 30);
            ctx.lineTo(bx, screenY + 10);
            ctx.stroke();
        }

        ctx.fillStyle = '#8a6a4a';
        ctx.font = '12px "Microsoft YaHei"';
        ctx.textAlign = 'center';
        ctx.fillText('⬛ 城墙 · 目标区域', this.canvas.width / 2, screenY - 38);
    }

    _drawVehicle(ctx, scale, offX, offY) {
        const screenX = offX;
        const screenY = offY;
        const headingRad = (this.vehicleState.heading_deg * Math.PI) / 180;

        ctx.save();
        ctx.translate(screenX, screenY);
        ctx.rotate(-headingRad);

        const carW = this.vehicleProfile.width_m * scale * 0.8;
        const carL = this.vehicleProfile.length_m * scale * 0.8;

        ctx.shadowColor = 'rgba(100, 150, 255, 0.4)';
        ctx.shadowBlur = 15;

        const grad = ctx.createLinearGradient(-carL / 2, 0, carL / 2, 0);
        grad.addColorStop(0, '#3a5aaa');
        grad.addColorStop(0.5, '#5a7aff');
        grad.addColorStop(1, '#3a5aaa');
        ctx.fillStyle = grad;
        ctx.fillRect(-carL / 2, -carW / 2, carL, carW);

        ctx.shadowBlur = 0;
        ctx.strokeStyle = '#88aaff';
        ctx.lineWidth = 2;
        ctx.strokeRect(-carL / 2, -carW / 2, carL, carW);

        ctx.fillStyle = '#88aaff';
        ctx.font = 'bold 10px monospace';
        ctx.textAlign = 'center';
        ctx.fillText('轒辒', 0, 4);

        ctx.fillStyle = '#ffaa44';
        ctx.beginPath();
        ctx.moveTo(carL / 2 + 12, 0);
        ctx.lineTo(carL / 2, -7);
        ctx.lineTo(carL / 2, 7);
        ctx.closePath();
        ctx.fill();

        ctx.restore();
    }

    _drawTrail(ctx, scale, offX, offY, vx, vy) {
        if (this.trailPoints.length < 2) return;

        ctx.strokeStyle = 'rgba(100, 180, 255, 0.25)';
        ctx.lineWidth = 2;
        ctx.beginPath();

        for (let i = 0; i < this.trailPoints.length; i++) {
            const p = this.trailPoints[i];
            const sx = offX + (p.x - vx) * scale;
            const sy = offY - (p.y - vy) * scale;
            if (i === 0) ctx.moveTo(sx, sy);
            else ctx.lineTo(sx, sy);
        }
        ctx.stroke();
    }

    _drawRocks(ctx, scale, offX, offY, vx, vy) {
        for (const atk of this.activeAttacks) {
            const sx = offX + (atk.impact_x - vx) * scale;
            const sy = offY - (atk.impact_y - vy) * scale - atk.currentY;
            const r = 6 + atk.rock_mass_kg / 20;

            ctx.shadowColor = 'rgba(255, 100, 80, 0.6)';
            ctx.shadowBlur = 12;

            const grad = ctx.createRadialGradient(sx - r / 3, sy - r / 3, 0, sx, sy, r);
            grad.addColorStop(0, '#ff8866');
            grad.addColorStop(0.6, '#cc4422');
            grad.addColorStop(1, '#882211');
            ctx.fillStyle = grad;

            ctx.beginPath();
            ctx.arc(sx, sy, r, 0, Math.PI * 2);
            ctx.fill();

            ctx.shadowBlur = 0;

            ctx.strokeStyle = 'rgba(255, 100, 80, 0.5)';
            ctx.lineWidth = 2;
            ctx.setLineDash([4, 4]);
            ctx.beginPath();
            ctx.moveTo(sx, sy);
            ctx.lineTo(sx, sy + atk.currentY);
            ctx.stroke();
            ctx.setLineDash([]);
        }
    }

    _drawFlashEffects(ctx, scale, offX, offY, vx, vy) {
        const now = performance.now();
        for (const flash of this.flashEffects) {
            const sx = offX + (flash.x - vx) * scale;
            const sy = offY - (flash.y - vy) * scale;
            const elapsed = now - flash.startTime;
            const t = elapsed / flash.duration;
            const alpha = Math.max(0, 1 - t);
            const intensity = flash.intensity || 1.0;
            const r = (20 + t * 60) * intensity;

            const grad = ctx.createRadialGradient(sx, sy, 0, sx, sy, r);
            grad.addColorStop(0, `rgba(255, 255, 200, ${alpha * 0.9})`);
            grad.addColorStop(0.3, `rgba(255, 200, 80, ${alpha * 0.7})`);
            grad.addColorStop(0.6, `rgba(255, 100, 40, ${alpha * 0.4})`);
            grad.addColorStop(1, 'rgba(255, 50, 20, 0)');

            ctx.fillStyle = grad;
            ctx.beginPath();
            ctx.arc(sx, sy, r, 0, Math.PI * 2);
            ctx.fill();
        }
    }

    destroy() {
        if (this._gameLoopTimer) {
            clearInterval(this._gameLoopTimer);
            this._gameLoopTimer = null;
        }
        if (this._boundKeyDown) {
            window.removeEventListener('keydown', this._boundKeyDown);
            window.removeEventListener('keydown', this._boundKeyUp);
        }
    }
}

export default VirtualDrivingView;
