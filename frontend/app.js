import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const API_BASE = 'http://127.0.0.1:8080';

const state = {
    scene: null, camera: null, renderer: null, controls: null,
    vehicle: null, roofMesh: null, roofOriginalGeometry: null,
    rockParticleSystem: null, rockVelocities: [],
    showRocks: true, showCloud: true, showStress: false, showWireframe: false,
    autoRotate: false,
    currentVehicle: 1, currentMaterial: 'wood',
    deformationField: new Array(100).fill(0),
    stressField: new Array(100).fill(0),
    stressHistory: [],
    impactHistory: [],
    deformationThreshold: 15.0,
    clock: new THREE.Clock(),
};

const GRID_N = 10;
const ROOF_LENGTH = 6.5;
const ROOF_WIDTH = 2.8;

const DAMAGE_LEVELS = [
    { label: '完好', color: '#4af', cls: 'hud-safe' },
    { label: '轻微', color: '#4fa', cls: 'hud-safe' },
    { label: '中度', color: '#fa4', cls: 'hud-warn' },
    { label: '严重', color: '#f84', cls: 'hud-warn' },
    { label: '破坏', color: '#f44', cls: 'hud-danger' },
];

function colormap(t) {
    t = Math.max(0, Math.min(1, t));
    const stops = [
        [0.00, [0, 0, 1]],
        [0.25, [0, 1, 1]],
        [0.50, [0, 1, 0]],
        [0.75, [1, 1, 0]],
        [1.00, [1, 0, 0]],
    ];
    for (let i = 0; i < stops.length - 1; i++) {
        if (t >= stops[i][0] && t <= stops[i + 1][0]) {
            const f = (t - stops[i][0]) / (stops[i + 1][0] - stops[i][0]);
            const c0 = stops[i][1], c1 = stops[i + 1][1];
            return new THREE.Color(
                c0[0] + (c1[0] - c0[0]) * f,
                c0[1] + (c1[1] - c0[1]) * f,
                c0[2] + (c1[2] - c0[2]) * f,
            );
        }
    }
    return new THREE.Color(1, 0, 0);
}

function initScene() {
    const container = document.getElementById('canvas-container');
    const w = container.clientWidth, h = container.clientHeight;

    state.scene = new THREE.Scene();
    state.scene.background = new THREE.Color(0x050810);
    state.scene.fog = new THREE.Fog(0x050810, 15, 40);

    state.camera = new THREE.PerspectiveCamera(50, w / h, 0.1, 1000);
    state.camera.position.set(10, 7, 10);

    state.renderer = new THREE.WebGLRenderer({ antialias: true });
    state.renderer.setPixelRatio(window.devicePixelRatio);
    state.renderer.setSize(w, h);
    state.renderer.shadowMap.enabled = true;
    state.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    container.appendChild(state.renderer.domElement);

    state.controls = new OrbitControls(state.camera, state.renderer.domElement);
    state.controls.enableDamping = true;
    state.controls.dampingFactor = 0.05;
    state.controls.target.set(0, 1.5, 0);
    state.controls.minDistance = 4;
    state.controls.maxDistance = 25;

    const ambient = new THREE.AmbientLight(0x404860, 0.5);
    state.scene.add(ambient);

    const dir = new THREE.DirectionalLight(0xffeedd, 1.2);
    dir.position.set(8, 12, 6);
    dir.castShadow = true;
    dir.shadow.mapSize.set(2048, 2048);
    dir.shadow.camera.left = -10;
    dir.shadow.camera.right = 10;
    dir.shadow.camera.top = 10;
    dir.shadow.camera.bottom = -10;
    state.scene.add(dir);

    const rim = new THREE.DirectionalLight(0x6688ff, 0.4);
    rim.position.set(-6, 4, -6);
    state.scene.add(rim);

    const groundGeo = new THREE.PlaneGeometry(60, 60);
    const groundMat = new THREE.MeshStandardMaterial({
        color: 0x1a2030, roughness: 0.9, metalness: 0.0,
    });
    const ground = new THREE.Mesh(groundGeo, groundMat);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    state.scene.add(ground);

    const grid = new THREE.GridHelper(60, 60, 0x2a3050, 0x1a2040);
    grid.position.y = 0.001;
    state.scene.add(grid);

    createVehicle();
    createRockParticles();

    window.addEventListener('resize', onResize);
    animate();
}

function createVehicle() {
    const group = new THREE.Group();

    const woodMat = new THREE.MeshStandardMaterial({
        color: 0x7a5a3a, roughness: 0.85, metalness: 0.05,
    });
    const ironMat = new THREE.MeshStandardMaterial({
        color: 0x7a7a8a, roughness: 0.4, metalness: 0.7,
    });
    const hideMat = new THREE.MeshStandardMaterial({
        color: 0x6a3a2a, roughness: 0.7, metalness: 0.0,
    });
    const mat = state.currentMaterial === 'iron' ? ironMat :
                state.currentMaterial === 'cowhide' ? hideMat : woodMat;

    const chassisGeo = new THREE.BoxGeometry(ROOF_LENGTH, 0.35, ROOF_WIDTH);
    const chassis = new THREE.Mesh(chassisGeo, woodMat);
    chassis.position.set(0, 0.6, 0);
    chassis.castShadow = true;
    chassis.receiveShadow = true;
    group.add(chassis);

    const sideWallGeo = new THREE.BoxGeometry(ROOF_LENGTH, 2.2, 0.15);
    const leftWall = new THREE.Mesh(sideWallGeo, mat);
    leftWall.position.set(0, 1.8, ROOF_WIDTH / 2 - 0.075);
    leftWall.castShadow = true;
    leftWall.receiveShadow = true;
    group.add(leftWall);
    const rightWall = new THREE.Mesh(sideWallGeo, mat);
    rightWall.position.set(0, 1.8, -ROOF_WIDTH / 2 + 0.075);
    rightWall.castShadow = true;
    rightWall.receiveShadow = true;
    group.add(rightWall);

    const frontWallGeo = new THREE.BoxGeometry(0.2, 2.2, ROOF_WIDTH - 0.3);
    const frontWall = new THREE.Mesh(frontWallGeo, mat);
    frontWall.position.set(-ROOF_LENGTH / 2 + 0.1, 1.8, 0);
    frontWall.castShadow = true;
    frontWall.receiveShadow = true;
    group.add(frontWall);
    const backWall = new THREE.Mesh(frontWallGeo, mat);
    backWall.position.set(ROOF_LENGTH / 2 - 0.1, 1.8, 0);
    backWall.castShadow = true;
    backWall.receiveShadow = true;
    group.add(backWall);

    const roofGeo = new THREE.PlaneGeometry(ROOF_LENGTH, ROOF_WIDTH, GRID_N - 1, GRID_N - 1);
    roofGeo.rotateX(-Math.PI / 2);
    const roofPositions = roofGeo.attributes.position;
    for (let i = 0; i < roofPositions.count; i++) {
        const y = roofPositions.getY(i);
        roofPositions.setY(i, y + 2.9);
    }
    roofPositions.needsUpdate = true;
    roofGeo.computeVertexNormals();

    state.roofOriginalGeometry = roofGeo.clone();

    const roofMat = new THREE.MeshStandardMaterial({
        color: 0x8a6a4a, roughness: 0.8, metalness: 0.05,
        vertexColors: true, side: THREE.DoubleSide,
    });
    const roof = new THREE.Mesh(roofGeo, roofMat);
    roof.castShadow = true;
    roof.receiveShadow = true;
    group.add(roof);
    state.roofMesh = roof;

    const ridgeGeo = new THREE.BoxGeometry(ROOF_LENGTH, 0.15, 0.2);
    const ridge = new THREE.Mesh(ridgeGeo, ironMat);
    ridge.position.set(0, 3.05, 0);
    ridge.castShadow = true;
    group.add(ridge);

    const wheelPositions = [
        [-ROOF_LENGTH / 2 + 0.6, 0, ROOF_WIDTH / 2 - 0.2],
        [ROOF_LENGTH / 2 - 0.6, 0, ROOF_WIDTH / 2 - 0.2],
        [-ROOF_LENGTH / 2 + 0.6, 0, -ROOF_WIDTH / 2 + 0.2],
        [ROOF_LENGTH / 2 - 0.6, 0, -ROOF_WIDTH / 2 + 0.2],
    ];
    const wheelGeo = new THREE.CylinderGeometry(0.5, 0.5, 0.2, 16);
    wheelGeo.rotateZ(Math.PI / 2);
    wheelPositions.forEach(pos => {
        const wheel = new THREE.Mesh(wheelGeo, woodMat);
        wheel.position.set(pos[0], 0.5, pos[2]);
        wheel.castShadow = true;
        wheel.receiveShadow = true;
        group.add(wheel);

        const hubGeo = new THREE.CylinderGeometry(0.15, 0.15, 0.22, 8);
        hubGeo.rotateZ(Math.PI / 2);
        const hub = new THREE.Mesh(hubGeo, ironMat);
        hub.position.copy(wheel.position);
        group.add(hub);
    });

    const ramGeo = new THREE.CylinderGeometry(0.25, 0.25, 1.8, 12);
    ramGeo.rotateZ(Math.PI / 2);
    const ram = new THREE.Mesh(ramGeo, ironMat);
    ram.position.set(-ROOF_LENGTH / 2 - 0.6, 1.8, 0);
    ram.castShadow = true;
    group.add(ram);

    const ramTipGeo = new THREE.ConeGeometry(0.35, 0.5, 8);
    ramTipGeo.rotateZ(-Math.PI / 2);
    const ramTip = new THREE.Mesh(ramTipGeo, ironMat);
    ramTip.position.set(-ROOF_LENGTH / 2 - 1.75, 1.8, 0);
    ramTip.castShadow = true;
    group.add(ramTip);

    state.vehicle = group;
    state.scene.add(group);

    resetRoofColors();
}

function resetRoofColors() {
    if (!state.roofMesh) return;
    const geo = state.roofMesh.geometry;
    const count = geo.attributes.position.count;
    const colors = new Float32Array(count * 3);
    const c = colormap(0);
    for (let i = 0; i < count; i++) {
        colors[i * 3] = c.r;
        colors[i * 3 + 1] = c.g;
        colors[i * 3 + 2] = c.b;
    }
    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    geo.attributes.color.needsUpdate = true;
}

function createRockParticles() {
    const count = 50;
    const positions = new Float32Array(count * 3);
    const colors = new Float32Array(count * 3);
    const sizes = new Float32Array(count);
    state.rockVelocities = [];

    for (let i = 0; i < count; i++) {
        positions[i * 3] = (Math.random() - 0.5) * ROOF_LENGTH;
        positions[i * 3 + 1] = 5 + Math.random() * 10;
        positions[i * 3 + 2] = (Math.random() - 0.5) * ROOF_WIDTH;

        const c = new THREE.Color();
        c.setHSL(0.08, 0.3, 0.3 + Math.random() * 0.2);
        colors[i * 3] = c.r;
        colors[i * 3 + 1] = c.g;
        colors[i * 3 + 2] = c.b;

        sizes[i] = 0.15 + Math.random() * 0.3;
        state.rockVelocities.push(new THREE.Vector3(
            (Math.random() - 0.5) * 0.5,
            -1 - Math.random() * 2,
            (Math.random() - 0.5) * 0.5,
        ));
    }

    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
    geo.setAttribute('size', new THREE.BufferAttribute(sizes, 1));

    const mat = new THREE.PointsMaterial({
        size: 0.35,
        vertexColors: true,
        sizeAttenuation: true,
        transparent: true,
        opacity: 0.9,
    });

    state.rockParticleSystem = new THREE.Points(geo, mat);
    state.scene.add(state.rockParticleSystem);
}

function updateRocks(dt) {
    if (!state.showRocks || !state.rockParticleSystem) return;
    const positions = state.rockParticleSystem.geometry.attributes.position.array;
    const count = positions.length / 3;
    for (let i = 0; i < count; i++) {
        positions[i * 3] += state.rockVelocities[i].x * dt * 3;
        positions[i * 3 + 1] += state.rockVelocities[i].y * dt * 3;
        positions[i * 3 + 2] += state.rockVelocities[i].z * dt * 3;
        state.rockVelocities[i].y -= 9.8 * dt * 0.1;

        if (positions[i * 3 + 1] < 0.5) {
            positions[i * 3] = (Math.random() - 0.5) * ROOF_LENGTH;
            positions[i * 3 + 1] = 8 + Math.random() * 8;
            positions[i * 3 + 2] = (Math.random() - 0.5) * ROOF_WIDTH;
            state.rockVelocities[i].set(
                (Math.random() - 0.5) * 0.5,
                -1 - Math.random() * 2,
                (Math.random() - 0.5) * 0.5,
            );
        }
    }
    state.rockParticleSystem.geometry.attributes.position.needsUpdate = true;
}

function applyDeformationToRoof() {
    if (!state.roofMesh) return;
    const geo = state.roofMesh.geometry;
    const origPos = state.roofOriginalGeometry.attributes.position;
    const pos = geo.attributes.position;
    const colors = geo.attributes.color.array;

    let maxDef = 0;
    const field = state.showStress ? state.stressField : state.deformationField;
    field.forEach(v => { if (v > maxDef) maxDef = v; });
    if (maxDef <= 0) maxDef = 1;

    for (let gi = 0; gi < GRID_N; gi++) {
        for (let gj = 0; gj < GRID_N; gj++) {
            const idx = gi * GRID_N + gj;
            const vIdx = idx;
            const val = field[idx];
            const normalized = val / maxDef;

            if (!state.showStress && state.showCloud) {
                const defMm = val;
                const origY = origPos.getY(vIdx);
                pos.setY(vIdx, origY - defMm / 1000 * 3);
            }

            const c = colormap(state.showCloud || state.showStress ? normalized : 0);
            colors[vIdx * 3] = c.r;
            colors[vIdx * 3 + 1] = c.g;
            colors[vIdx * 3 + 2] = c.b;
        }
    }

    pos.needsUpdate = true;
    geo.attributes.color.needsUpdate = true;
    geo.computeVertexNormals();

    if (state.roofMesh.material) {
        state.roofMesh.material.wireframe = state.showWireframe;
    }
}

function onResize() {
    const container = document.getElementById('canvas-container');
    const w = container.clientWidth, h = container.clientHeight;
    state.camera.aspect = w / h;
    state.camera.updateProjectionMatrix();
    state.renderer.setSize(w, h);
}

function animate() {
    requestAnimationFrame(animate);
    const dt = state.clock.getDelta();
    updateRocks(dt);
    if (state.autoRotate && state.vehicle) {
        state.vehicle.rotation.y += dt * 0.2;
    }
    state.controls.update();
    state.renderer.render(state.scene, state.camera);
}

async function runSimulation() {
    addAlert('info', '执行结构仿真计算...');
    try {
        const res = await fetch(`${API_BASE}/api/simulation/latest?vehicle_id=${state.currentVehicle}`);
        const data = await res.json();
        const sim = data.data;
        updateSimulationDisplay(sim);
        state.deformationField = sim.deformation_field || [];
        state.stressField = sim.stress_field || [];
        applyDeformationToRoof();

        if (sim.roof_max_deformation_mm > state.deformationThreshold) {
            addAlert('danger', `顶棚变形超限: ${sim.roof_max_deformation_mm.toFixed(2)}mm > ${state.deformationThreshold}mm`);
        }
        if (sim.is_penetrated) {
            addAlert('danger', `防护层击穿！侵彻深度: ${sim.penetration_depth_mm.toFixed(2)}mm`);
        }
        drawHeatmap();
    } catch (e) {
        addAlert('warn', '无法连接后端，使用本地仿真模型');
        runLocalSimulation();
    }
}

function runLocalSimulation() {
    const mass = 20 + Math.random() * 180;
    const velocity = 10 + Math.random() * 15;
    const impactEnergy = 0.5 * mass * velocity * velocity;
    const deformation = Math.random() * 25;
    const stress = 30 + Math.random() * 200;
    const penetrated = deformation > 20;
    const damage = deformation < 5 ? 0 : deformation < 10 ? 1 : deformation < 15 ? 2 : deformation < 22 ? 3 : 4;

    const modes = ['bending', 'shear', 'punching', 'combined'];
    const modeNames = { bending: '弯曲破坏', shear: '剪切破坏', punching: '冲切破坏', combined: '组合破坏' };

    const sim = {
        roof_max_deformation_mm: deformation,
        roof_plastic_strain: Math.random() * 0.05,
        roof_von_mises_stress_mpa: stress,
        impact_energy_j: impactEnergy,
        absorbed_energy_j: impactEnergy * (0.3 + Math.random() * 0.5),
        damage_level: damage,
        penetration_depth_mm: deformation * 0.8,
        is_penetrated: penetrated,
        failure_mode: modes[Math.floor(Math.random() * 4)],
    };
    sim.deformation_field = generateField(deformation);
    sim.stress_field = generateField(stress);

    updateSimulationDisplay(sim);
    state.deformationField = sim.deformation_field;
    state.stressField = sim.stress_field;
    applyDeformationToRoof();

    document.getElementById('m-roof-stress').textContent = stress.toFixed(1);
    document.getElementById('m-impact').textContent = (mass * velocity / 100).toFixed(1);
    document.getElementById('m-mass').textContent = mass.toFixed(1);
    document.getElementById('m-velocity').textContent = velocity.toFixed(1);

    addHistoryData(stress, mass * velocity / 100);
    drawHeatmap();
    drawCharts();

    if (deformation > state.deformationThreshold) {
        addAlert('danger', `顶棚变形超限: ${deformation.toFixed(2)}mm > ${state.deformationThreshold}mm`);
    }
    if (penetrated) {
        addAlert('danger', `防护层击穿！`);
    }
}

function generateField(maxVal) {
    const field = new Array(GRID_N * GRID_N).fill(0);
    const cx = Math.floor(GRID_N / 2) + (Math.random() - 0.5) * 3;
    const cy = Math.floor(GRID_N / 2) + (Math.random() - 0.5) * 3;
    const sigma = 2 + Math.random() * 2;
    for (let i = 0; i < GRID_N; i++) {
        for (let j = 0; j < GRID_N; j++) {
            const d2 = (i - cx) ** 2 + (j - cy) ** 2;
            field[i * GRID_N + j] = maxVal * Math.exp(-d2 / (2 * sigma * sigma));
        }
    }
    return field;
}

function updateSimulationDisplay(sim) {
    const defEl = document.getElementById('hud-deformation');
    const stressEl = document.getElementById('hud-stress');
    const energyEl = document.getElementById('hud-energy');
    const dmgEl = document.getElementById('hud-damage');

    defEl.innerHTML = `${sim.roof_max_deformation_mm.toFixed(2)}<span style="font-size:12px">mm</span>`;
    defEl.className = 'hud-value ' + (sim.roof_max_deformation_mm > state.deformationThreshold ? 'hud-danger' :
                                       sim.roof_max_deformation_mm > state.deformationThreshold * 0.7 ? 'hud-warn' : 'hud-safe');

    stressEl.innerHTML = `${sim.roof_von_mises_stress_mpa.toFixed(2)}<span style="font-size:12px">MPa</span>`;
    stressEl.className = 'hud-value ' + (sim.roof_von_mises_stress_mpa > 200 ? 'hud-danger' :
                                          sim.roof_von_mises_stress_mpa > 120 ? 'hud-warn' : 'hud-safe');

    energyEl.innerHTML = `${sim.impact_energy_j.toFixed(0)}<span style="font-size:12px">J</span>`;

    const dl = DAMAGE_LEVELS[Math.min(sim.damage_level, 4)];
    dmgEl.textContent = `${sim.damage_level} ${dl.label}`;
    dmgEl.className = `hud-value ${dl.cls}`;

    document.getElementById('m-penetration').textContent = sim.penetration_depth_mm.toFixed(2);
    const modeNames = { bending: '弯曲破坏', shear: '剪切破坏', punching: '冲切破坏', combined: '组合破坏' };
    document.getElementById('m-failure').textContent = modeNames[sim.failure_mode] || sim.failure_mode;

    addHistoryData(sim.roof_von_mises_stress_mpa, sim.impact_energy_j / 500);
    drawCharts();
}

async function runAHPEvaluation() {
    addAlert('info', '执行AHP层次分析评估...');
    try {
        const res = await fetch(`${API_BASE}/api/evaluate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ vehicle_id: state.currentVehicle }),
        });
        const data = await res.json();
        updateAHPEvaluation(data.data || []);
    } catch (e) {
        const dummy = [
            { material_type: 'composite', material_thickness_mm: 48, energy_absorption_score: 0.85, structural_strength_score: 0.82, weight_factor_score: 0.7, cost_factor_score: 0.5, durability_score: 0.8, ahp_weight_score: 0.76, rank_position: 1, is_recommended: true },
            { material_type: 'iron',      material_thickness_mm: 5,  energy_absorption_score: 0.9,  structural_strength_score: 0.95, weight_factor_score: 0.2, cost_factor_score: 0.2, durability_score: 0.95, ahp_weight_score: 0.68, rank_position: 2, is_recommended: false },
            { material_type: 'wood',      material_thickness_mm: 64, energy_absorption_score: 0.45, structural_strength_score: 0.5,  weight_factor_score: 0.85,cost_factor_score: 0.95,durability_score: 0.55, ahp_weight_score: 0.63, rank_position: 3, is_recommended: false },
            { material_type: 'cowhide',   material_thickness_mm: 20, energy_absorption_score: 0.55, structural_strength_score: 0.3,  weight_factor_score: 0.9, cost_factor_score: 0.7, durability_score: 0.35, ahp_weight_score: 0.54, rank_position: 4, is_recommended: false },
        ];
        updateAHPEvaluation(dummy);
    }
}

function updateAHPEvaluation(evals) {
    const tbody = document.querySelector('#ahp-table tbody');
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
    addAlert('info', `AHP评估完成，共${evals.length}个方案`);
}

function addHistoryData(stress, impact) {
    state.stressHistory.push(stress);
    state.impactHistory.push(impact);
    if (state.stressHistory.length > 50) state.stressHistory.shift();
    if (state.impactHistory.length > 50) state.impactHistory.shift();
}

function drawCharts() {
    drawLineChart('chart-stress', state.stressHistory, '#4af', 0, 250);
    drawLineChart('chart-impact', state.impactHistory, '#fa4', 0, 100);
}

function drawLineChart(canvasId, data, color, min, max) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.parentElement.clientWidth;
    const h = canvas.parentElement.clientHeight;
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
    grad.addColorStop(0, color + '44');
    grad.addColorStop(1, color + '00');
    ctx.fillStyle = grad;
    ctx.lineTo(w, h);
    ctx.lineTo(0, h);
    ctx.closePath();
    ctx.fill();
}

function drawHeatmap() {
    const canvas = document.getElementById('chart-heatmap');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const w = canvas.parentElement.clientWidth;
    const h = canvas.parentElement.clientHeight;
    canvas.width = w * devicePixelRatio;
    canvas.height = h * devicePixelRatio;
    canvas.style.width = w + 'px';
    canvas.style.height = h + 'px';
    ctx.scale(devicePixelRatio, devicePixelRatio);

    ctx.fillStyle = '#1a2040';
    ctx.fillRect(0, 0, w, h);

    const field = state.showStress ? state.stressField : state.deformationField;
    if (!field || field.length !== GRID_N * GRID_N) return;

    let max = 0;
    field.forEach(v => { if (v > max) max = v; });
    if (max <= 0) max = 1;

    const cellW = w / GRID_N;
    const cellH = h / GRID_N;

    for (let i = 0; i < GRID_N; i++) {
        for (let j = 0; j < GRID_N; j++) {
            const val = field[i * GRID_N + j];
            const c = colormap(val / max);
            ctx.fillStyle = `rgb(${Math.floor(c.r * 255)},${Math.floor(c.g * 255)},${Math.floor(c.b * 255)})`;
            ctx.fillRect(j * cellW, i * cellH, cellW + 1, cellH + 1);
        }
    }

    ctx.strokeStyle = '#00000040';
    ctx.lineWidth = 1;
    for (let i = 0; i <= GRID_N; i++) {
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

function addAlert(type, message) {
    const list = document.getElementById('alert-list');
    const item = document.createElement('div');
    const cls = type === 'danger' ? '' : type === 'warn' ? 'warn' : 'info';
    const now = new Date();
    const time = `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
    item.className = `alert-item ${cls}`;
    item.innerHTML = `<div class="alert-time">${time}</div><div>${message}</div>`;
    list.insertBefore(item, list.firstChild);
    while (list.children.length > 20) list.removeChild(list.lastChild);
}

function setupUI() {
    document.getElementById('vehicle-select').addEventListener('change', e => {
        state.currentVehicle = parseInt(e.target.value);
    });
    document.getElementById('material-select').addEventListener('change', e => {
        state.currentMaterial = e.target.value;
        if (state.vehicle) {
            state.scene.remove(state.vehicle);
            state.vehicle = null;
            createVehicle();
        }
    });
    document.getElementById('btn-simulate').addEventListener('click', runSimulation);
    document.getElementById('btn-evaluate').addEventListener('click', runAHPEvaluation);
    document.getElementById('btn-reset').addEventListener('click', () => {
        state.controls.reset();
        state.camera.position.set(10, 7, 10);
        state.controls.target.set(0, 1.5, 0);
        if (state.vehicle) state.vehicle.rotation.set(0, 0, 0);
    });

    function setupToggle(id, key) {
        document.getElementById(id).addEventListener('click', function () {
            this.classList.toggle('active');
            state[key] = this.classList.contains('active');
            if (key === 'showCloud' || key === 'showStress') {
                if (key === 'showStress' && state.showStress) {
                    document.getElementById('toggle-cloud').classList.remove('active');
                    state.showCloud = false;
                }
                if (key === 'showCloud' && state.showCloud) {
                    document.getElementById('toggle-stress').classList.remove('active');
                    state.showStress = false;
                }
            }
            if (state.rockParticleSystem) state.rockParticleSystem.visible = state.showRocks;
            applyDeformationToRoof();
            drawHeatmap();
        });
    }
    setupToggle('toggle-rocks', 'showRocks');
    setupToggle('toggle-cloud', 'showCloud');
    setupToggle('toggle-stress', 'showStress');
    setupToggle('toggle-wireframe', 'showWireframe');
    setupToggle('toggle-rotate', 'autoRotate');

    const views = {
        'view-front': [0, 3, 12],
        'view-side':  [12, 3, 0],
        'view-top':   [0.1, 15, 0.1],
        'view-iso':   [10, 7, 10],
    };
    Object.keys(views).forEach(id => {
        document.getElementById(id).addEventListener('click', () => {
            const [x, y, z] = views[id];
            state.camera.position.set(x, y, z);
            state.controls.target.set(0, 1.5, 0);
            state.controls.update();
        });
    });

    function tick() {
        const now = new Date();
        document.getElementById('clock').textContent =
            `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
    }
    setInterval(tick, 1000);
    tick();
}

window.addEventListener('DOMContentLoaded', () => {
    initScene();
    setupUI();
    setTimeout(() => {
        runLocalSimulation();
        runAHPEvaluation();
    }, 500);
});
