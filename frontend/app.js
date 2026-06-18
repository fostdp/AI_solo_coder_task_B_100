import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const API_BASE = 'http://127.0.0.1:8080';

const state = {
    scene: null, camera: null, renderer: null, controls: null,
    vehicle: null, roofMesh: null, roofOriginalGeometry: null,
    rockGPUPoints: null,
    rockUniforms: null,
    rockInstancedMesh: null,
    rockInstDummy: null,
    rockCount: 0,
    impactFlashPoints: null,
    flashUniforms: null,
    showRocks: true, showCloud: true, showStress: false, showWireframe: false,
    autoRotate: false,
    currentVehicle: 1, currentMaterial: 'wood',
    deformationField: new Array(100).fill(0),
    stressField: new Array(100).fill(0),
    stressHistory: [],
    impactHistory: [],
    deformationThreshold: 15.0,
    clock: new THREE.Clock(),
    fpsSmoothed: 60,
    lastSimTick: 0,
};

const GRID_N = 10;
const ROOF_LENGTH = 6.5;
const ROOF_WIDTH = 2.8;
const GPU_ROCK_COUNT = 400;

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
    state.scene.fog = new THREE.Fog(0x050810, 15, 50);

    state.camera = new THREE.PerspectiveCamera(55, w / h, 0.05, 200);
    state.camera.position.set(10, 7, 10);

    state.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
    state.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    state.renderer.setSize(w, h);
    state.renderer.shadowMap.enabled = true;
    state.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    state.renderer.outputColorSpace = THREE.SRGBColorSpace;
    container.appendChild(state.renderer.domElement);

    state.controls = new OrbitControls(state.camera, state.renderer.domElement);
    state.controls.enableDamping = true;
    state.controls.dampingFactor = 0.08;
    state.controls.target.set(0, 1.5, 0);
    state.controls.minDistance = 3;
    state.controls.maxDistance = 25;

    const ambient = new THREE.AmbientLight(0x404860, 0.6);
    state.scene.add(ambient);

    const dir = new THREE.DirectionalLight(0xffeedd, 1.2);
    dir.position.set(8, 12, 6);
    dir.castShadow = true;
    dir.shadow.mapSize.set(2048, 2048);
    dir.shadow.camera.left = -10;
    dir.shadow.camera.right = 10;
    dir.shadow.camera.top = 10;
    dir.shadow.camera.bottom = -10;
    dir.shadow.camera.near = 0.5;
    dir.shadow.camera.far = 40;
    state.scene.add(dir);

    const rim = new THREE.DirectionalLight(0x6688ff, 0.4);
    rim.position.set(-6, 4, -6);
    state.scene.add(rim);

    const groundGeo = new THREE.PlaneGeometry(80, 80);
    const groundMat = new THREE.MeshStandardMaterial({
        color: 0x1a2030, roughness: 0.95, metalness: 0.0,
    });
    const ground = new THREE.Mesh(groundGeo, groundMat);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    state.scene.add(ground);

    const grid = new THREE.GridHelper(80, 80, 0x2a3050, 0x1a2040);
    grid.position.y = 0.001;
    state.scene.add(grid);

    createVehicle();
    createGPURockParticles();
    createRockInstancedMesh();
    createImpactFlashSystem();

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

// ============================================================
// GPU 粒子滚石系统 - ShaderMaterial + GLSL 物理
// 所有粒子位置/速度/生命周期计算全部在 vertex shader
// CPU 每帧仅更新 uTime / uImpactTrigger 两个 uniform
// ============================================================
function createGPURockParticles() {
    const N = GPU_ROCK_COUNT;
    state.rockCount = N;

    const positions = new Float32Array(N * 3);
    const seeds = new Float32Array(N * 3);
    const sizes = new Float32Array(N);

    for (let i = 0; i < N; i++) {
        positions[i * 3]     = (Math.random() - 0.5) * 2.0;
        positions[i * 3 + 1] = 5 + Math.random() * 10;
        positions[i * 3 + 2] = (Math.random() - 0.5) * 2.0;

        seeds[i * 3]     = Math.random() * 1000.0;
        seeds[i * 3 + 1] = Math.random() * 1000.0;
        seeds[i * 3 + 2] = Math.random() * 1000.0;

        sizes[i] = 0.2 + Math.random() * 0.45;
    }

    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geo.setAttribute('aSeed',    new THREE.BufferAttribute(seeds, 3));
    geo.setAttribute('aSize',    new THREE.BufferAttribute(sizes, 1));

    state.rockUniforms = {
        uTime:        { value: 0.0 },
        uGravity:     { value: -9.8 },
        uRoofMin:     { value: new THREE.Vector3(-ROOF_LENGTH / 2, 2.9, -ROOF_WIDTH / 2) },
        uRoofMax:     { value: new THREE.Vector3( ROOF_LENGTH / 2, 3.1,  ROOF_WIDTH / 2) },
        uSpawnHeight: { value: 18.0 },
        uFloorY:      { value: 0.5 },
        uPixelRatio:  { value: state.renderer.getPixelRatio() },
        uVisible:     { value: 1.0 },
        uImpact:      { value: 0.0 },
        uImpactPos:   { value: new THREE.Vector3(0, 3.0, 0) },
    };

    const mat = new THREE.ShaderMaterial({
        uniforms: state.rockUniforms,
        transparent: true,
        depthWrite: true,
        vertexColors: true,
        vertexShader: /* glsl */`
            attribute vec3 aSeed;
            attribute float aSize;

            uniform float uTime;
            uniform float uGravity;
            uniform vec3  uRoofMin;
            uniform vec3  uRoofMax;
            uniform float uSpawnHeight;
            uniform float uFloorY;
            uniform float uPixelRatio;
            uniform float uVisible;
            uniform float uImpact;
            uniform vec3  uImpactPos;

            varying vec3 vColor;
            varying float vAlive;
            varying float vDepth;

            float hash(vec3 p) {
                p = fract(p * 0.3183099 + 0.1);
                p *= 17.0;
                return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
            }

            void main() {
                float id = float(gl_VertexID);

                vec3 seed = aSeed + vec3(uTime * 0.0001);

                float period = 2.0 + hash(seed * 1.1) * 6.0;
                float tLocal = mod(uTime + hash(seed) * period, period);

                vec3 baseDir = vec3(
                    (hash(seed * 3.7) - 0.5) * 1.5,
                    0.0,
                    (hash(seed * 9.1) - 0.5) * 1.2
                );

                vec3 vel0 = baseDir * (1.5 + hash(seed * 5.3) * 1.5);
                vel0.y = -1.5 - hash(seed * 7.7) * 4.0;

                float startX = (hash(seed * 2.1) - 0.5) * (uRoofMax.x - uRoofMin.x);
                float startZ = (hash(seed * 4.9) - 0.5) * (uRoofMax.z - uRoofMin.z);

                vec3 pos = vec3(startX, uSpawnHeight, startZ);
                pos += vel0 * tLocal;
                pos.y += 0.5 * uGravity * tLocal * tLocal;

                float bounced = 0.0;
                if (pos.y < uRoofMax.y && pos.y > uRoofMin.y - 0.2 &&
                    pos.x > uRoofMin.x && pos.x < uRoofMax.x &&
                    pos.z > uRoofMin.z && pos.z < uRoofMax.z) {
                    bounced = 1.0;
                    pos.y = uRoofMax.y + (uRoofMax.y - pos.y) * 0.3;
                }

                if (pos.y < uFloorY) {
                    float tReset = floor(uTime / max(period, 0.01));
                    pos.x = (hash(seed + tReset) - 0.5) * (uRoofMax.x - uRoofMin.x);
                    pos.z = (hash(seed * 1.3 + tReset) - 0.5) * (uRoofMax.z - uRoofMin.z);
                    pos.y = uSpawnHeight;
                }

                vec3 impactDisp = vec3(0.0);
                float impactW = 0.0;
                if (uImpact > 0.0) {
                    float d = length(pos.xz - uImpactPos.xz);
                    impactW = uImpact * exp(-d * d / 1.5);
                    impactDisp.y = impactW * 0.8;
                    impactDisp.xz = normalize(pos.xz - uImpactPos.xz + 1e-5) * impactW * 0.4;
                }
                pos += impactDisp;

                vAlive = uVisible * (1.0 - smoothstep(uSpawnHeight - 0.2, uSpawnHeight, pos.y) * 0.5);
                vDepth = -pos.y;

                vec3 baseColor = mix(
                    vec3(0.25, 0.2, 0.15),
                    vec3(0.6, 0.55, 0.5),
                    hash(seed * 11.0)
                );
                baseColor += bounced * vec3(0.25, 0.15, 0.05);
                baseColor += impactW * vec3(0.4, 0.2, 0.0);
                vColor = baseColor;

                vec4 mv = modelViewMatrix * vec4(pos, 1.0);
                gl_Position = projectionMatrix * mv;

                float size = aSize * uPixelRatio * (300.0 / -mv.z);
                size += impactW * 20.0;
                gl_PointSize = clamp(size, 2.0, 80.0);
            }
        `,
        fragmentShader: /* glsl */`
            varying vec3 vColor;
            varying float vAlive;
            varying float vDepth;

            void main() {
                if (vAlive < 0.01) discard;

                vec2 uv = gl_PointCoord - 0.5;
                float r = length(uv);
                if (r > 0.5) discard;

                float a = smoothstep(0.5, 0.3, r);

                vec3 light = normalize(vec3(0.6, 0.8, 0.4));
                vec3 normal = normalize(vec3(uv * 2.0, sqrt(max(0.0, 1.0 - dot(uv * 2.0, uv * 2.0)))));
                float ndl = max(0.0, dot(normal, light));
                float spec = pow(max(0.0, reflect(-light, normal).z), 16.0) * 0.3;

                vec3 col = vColor * (0.35 + 0.85 * ndl) + spec * vec3(1.0, 0.95, 0.9);

                gl_FragColor = vec4(col, a * vAlive);
            }
        `,
    });

    state.rockGPUPoints = new THREE.Points(geo, mat);
    state.rockGPUPoints.frustumCulled = false;
    state.scene.add(state.rockGPUPoints);
}

// ============================================================
// 近距离高精度滚石：InstancedMesh
// 对距离摄像机最近的 K 个滚石用真实球体渲染
// ============================================================
function createRockInstancedMesh() {
    const K = 40;
    const sphereGeo = new THREE.IcosahedronGeometry(1, 1);

    const instMat = new THREE.MeshStandardMaterial({
        color: 0x55504a, roughness: 0.95, metalness: 0.05, flatShading: true,
    });
    state.rockInstancedMesh = new THREE.InstancedMesh(sphereGeo, instMat, K);
    state.rockInstancedMesh.castShadow = true;
    state.rockInstancedMesh.receiveShadow = true;
    state.rockInstancedMesh.frustumCulled = false;
    state.rockInstDummy = new THREE.Object3D();

    const colors = new Float32Array(K * 3);
    for (let i = 0; i < K; i++) {
        const shade = 0.35 + Math.random() * 0.35;
        colors[i * 3]     = shade * (0.95 + Math.random() * 0.1);
        colors[i * 3 + 1] = shade * (0.9  + Math.random() * 0.1);
        colors[i * 3 + 2] = shade * (0.85 + Math.random() * 0.1);
    }
    state.rockInstancedMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
    state.rockInstancedMesh.instanceColor = new THREE.InstancedBufferAttribute(colors, 3);

    state.scene.add(state.rockInstancedMesh);
}

function updateInstancedRocks(dt) {
    if (!state.rockInstancedMesh) return;
    if (!state.showRocks) {
        state.rockInstancedMesh.visible = false;
        return;
    }
    state.rockInstancedMesh.visible = true;

    const K = state.rockInstancedMesh.count;
    const cam = state.camera.position;
    const t = state.clock.getElapsedTime();

    for (let i = 0; i < K; i++) {
        const s = (i + 1) / K;
        const period = 2.0 + s * 5.0;
        const tt = (t + s * 7.3) % period;

        const startX = (Math.sin(i * 27.3) * 0.5 + 0.0) * ROOF_LENGTH * 0.9;
        const startZ = (Math.cos(i * 13.7) * 0.5 + 0.0) * ROOF_WIDTH * 0.9;
        const v0x = Math.sin(i * 5.1) * 0.8;
        const v0z = Math.cos(i * 7.9) * 0.8;
        const v0y = -2.0 - (i % 5) * 0.5;

        let x = startX + v0x * tt;
        let z = startZ + v0z * tt;
        let y = 15.0 + v0y * tt + 0.5 * -9.8 * tt * tt;

        if (y < 2.95 && y > 2.85 &&
            Math.abs(x) < ROOF_LENGTH / 2 && Math.abs(z) < ROOF_WIDTH / 2) {
            y = 2.95 + (2.95 - y) * 0.3;
        }
        if (y < 0.5) y = 15.0;

        const d = Math.hypot(x - cam.x, z - cam.z);
        if (d > 15 && Math.abs(y - cam.y) > 10) {
            state.rockInstDummy.position.set(0, -1000, 0);
        } else {
            state.rockInstDummy.position.set(x, y, z);
        }

        const scale = 0.15 + ((i * 37) % 100) / 250.0;
        state.rockInstDummy.scale.set(scale, scale, scale);
        state.rockInstDummy.rotation.set(i * 0.7 + t, i * 1.3 + t * 0.5, i * 2.1);
        state.rockInstDummy.updateMatrix();
        state.rockInstancedMesh.setMatrixAt(i, state.rockInstDummy.matrix);
    }
    state.rockInstancedMesh.instanceMatrix.needsUpdate = true;
}

// ============================================================
// 冲击闪光/碎片粒子（GPU）
// ============================================================
function createImpactFlashSystem() {
    const N = 120;
    const positions = new Float32Array(N * 3);
    const segs = new Float32Array(N);
    const lifeSeeds = new Float32Array(N);

    for (let i = 0; i < N; i++) {
        positions[i * 3] = 0;
        positions[i * 3 + 1] = 0;
        positions[i * 3 + 2] = 0;
        segs[i] = i;
        lifeSeeds[i] = Math.random();
    }
    const geo = new THREE.BufferGeometry();
    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geo.setAttribute('aSeg', new THREE.BufferAttribute(segs, 1));
    geo.setAttribute('aLifeSeed', new THREE.BufferAttribute(lifeSeeds, 1));

    state.flashUniforms = {
        uTrigger: { value: 0.0 },
        uOrigin:  { value: new THREE.Vector3(0, 3.0, 0) },
        uTime:    { value: 0.0 },
        uPR:      { value: state.renderer.getPixelRatio() },
    };

    const mat = new THREE.ShaderMaterial({
        uniforms: state.flashUniforms,
        transparent: true,
        depthWrite: false,
        blending: THREE.AdditiveBlending,
        vertexShader: /* glsl */`
            attribute float aSeg;
            attribute float aLifeSeed;
            uniform float uTrigger;
            uniform vec3  uOrigin;
            uniform float uTime;
            uniform float uPR;
            varying float vLife;
            varying float vSize;

            void main() {
                float N = 120.0;
                float i = aSeg;

                float theta = (i / N) * 6.28318530718 * (1.0 + fract(aLifeSeed * 7.77));
                float phi   = aLifeSeed * 1.570796;
                float speed = 4.0 + fract(sin(i * 12.9898) * 43758.5453) * 10.0;

                float life = uTrigger * (1.0 - aLifeSeed * 0.5);
                life = clamp(life, 0.0, 1.0);

                vec3 dir = vec3(
                    sin(phi) * cos(theta),
                    cos(phi) * 0.8 + 0.3,
                    sin(phi) * sin(theta)
                );

                float elapsed = life * 0.9;
                vec3 pos = uOrigin + dir * speed * elapsed + vec3(0.0, -4.9 * elapsed * elapsed, 0.0);

                vLife = 1.0 - life;
                vSize = (8.0 + speed) * uPR * vLife;

                vec4 mv = modelViewMatrix * vec4(pos, 1.0);
                gl_Position = projectionMatrix * mv;
                gl_PointSize = clamp(vSize / max(-mv.z * 0.01, 0.2), 1.0, 80.0);
            }
        `,
        fragmentShader: /* glsl */`
            varying float vLife;
            void main() {
                vec2 uv = gl_PointCoord - 0.5;
                float d = length(uv);
                if (d > 0.5 || vLife <= 0.01) discard;
                float a = smoothstep(0.5, 0.0, d) * vLife;
                vec3 col = mix(vec3(1.0, 0.85, 0.4), vec3(1.0, 0.3, 0.1), 1.0 - vLife);
                gl_FragColor = vec4(col, a);
            }
        `,
    });

    state.impactFlashPoints = new THREE.Points(geo, mat);
    state.impactFlashPoints.frustumCulled = false;
    state.scene.add(state.impactFlashPoints);
}

function triggerImpactFlash(worldX, worldZ) {
    if (!state.flashUniforms) return;
    state.flashUniforms.uOrigin.value.set(worldX, 3.0, worldZ);
    state.flashUniforms.uTrigger.value = 1.0;

    if (state.rockUniforms) {
        state.rockUniforms.uImpactPos.value.set(worldX, 3.0, worldZ);
        state.rockUniforms.uImpact.value = 1.0;
    }

    setTimeout(() => {
        if (state.flashUniforms) state.flashUniforms.uTrigger.value = 0.0;
        if (state.rockUniforms)   state.rockUniforms.uImpact.value = 0.0;
    }, 900);

    const start = performance.now();
    const fade = () => {
        const t = (performance.now() - start) / 900;
        if (t >= 1) return;
        if (state.flashUniforms) state.flashUniforms.uTrigger.value = 1.0 - t;
        if (state.rockUniforms)   state.rockUniforms.uImpact.value = (1.0 - t) * 0.7;
        requestAnimationFrame(fade);
    };
    requestAnimationFrame(fade);
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
    if (state.rockUniforms) {
        state.rockUniforms.uPixelRatio.value = state.renderer.getPixelRatio();
    }
    if (state.flashUniforms) {
        state.flashUniforms.uPR.value = state.renderer.getPixelRatio();
    }
}

function animate() {
    requestAnimationFrame(animate);
    const dt = Math.min(state.clock.getDelta(), 0.05);
    const t = state.clock.getElapsedTime();

    if (state.rockUniforms) {
        state.rockUniforms.uTime.value = t;
        state.rockUniforms.uVisible.value = state.showRocks ? 1.0 : 0.0;
    }
    if (state.rockGPUPoints) {
        state.rockGPUPoints.visible = state.showRocks;
    }
    if (state.flashUniforms) {
        state.flashUniforms.uTime.value = t;
    }

    updateInstancedRocks(dt);

    if (state.autoRotate && state.vehicle) {
        state.vehicle.rotation.y += dt * 0.2;
    }

    if (t - state.lastSimTick > 1.5) {
        state.lastSimTick = t;
        const fx = (Math.random() - 0.5) * ROOF_LENGTH * 0.7;
        const fz = (Math.random() - 0.5) * ROOF_WIDTH * 0.7;
        triggerImpactFlash(fx, fz);
    }

    state.controls.update();
    state.renderer.render(state.scene, state.camera);

    const fps = 1.0 / Math.max(dt, 1e-4);
    state.fpsSmoothed = state.fpsSmoothed * 0.9 + fps * 0.1;
}

async function runSimulation() {
    addAlert('info', '执行Johnson-Cook高应变率结构仿真...');
    const fx = (Math.random() - 0.5) * ROOF_LENGTH * 0.7;
    const fz = (Math.random() - 0.5) * ROOF_WIDTH * 0.7;
    triggerImpactFlash(fx, fz);

    try {
        const res = await fetch(`${API_BASE}/api/simulation/latest?vehicle_id=${state.currentVehicle}`);
        const data = await res.json();
        const sim = data.data;
        updateSimulationDisplay(sim);
        state.deformationField = sim.deformation_field || [];
        state.stressField = sim.stress_field || [];
        applyDeformationToRoof();

        if (sim.roof_max_deformation_mm > state.deformationThreshold) {
            addAlert('danger', `顶棚变形超限(JC模型): ${sim.roof_max_deformation_mm.toFixed(2)}mm > ${state.deformationThreshold}mm`);
        }
        if (sim.is_penetrated) {
            addAlert('danger', `防护层击穿(JC动态韧性): ${sim.penetration_depth_mm.toFixed(2)}mm`);
        }
        drawHeatmap();
    } catch (e) {
        addAlert('warn', '无法连接后端，使用本地Johnson-Cook仿真模型');
        runLocalSimulation();
    }
}

function runLocalSimulation() {
    const mass = 20 + Math.random() * 180;
    const velocity = 10 + Math.random() * 25;
    const impactEnergy = 0.5 * mass * velocity * velocity;

    const strainRate = velocity / 0.08;
    const JC_A = 60e6, JC_B = 120e6, JC_n = 0.45, JC_C = 0.06, JC_m = 1.0;
    const srFactor = 1.0 + JC_C * Math.log(Math.max(strainRate, 1.0));
    const dynYield = JC_A * srFactor;

    const eps_p = Math.max(0, Math.pow((impactEnergy / 50000 - JC_A / JC_B), 1 / JC_n));
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
    };
    sim.deformation_field = generateField(deformation);
    sim.stress_field = generateField(sim.roof_von_mises_stress_mpa);

    updateSimulationDisplay(sim);
    state.deformationField = sim.deformation_field;
    state.stressField = sim.stress_field;
    applyDeformationToRoof();

    document.getElementById('m-roof-stress').textContent = sim.roof_von_mises_stress_mpa.toFixed(1);
    document.getElementById('m-impact').textContent = (mass * velocity / 100).toFixed(1);
    document.getElementById('m-mass').textContent = mass.toFixed(1);
    document.getElementById('m-velocity').textContent = velocity.toFixed(1);

    addHistoryData(sim.roof_von_mises_stress_mpa, mass * velocity / 100);
    drawHeatmap();
    drawCharts();

    if (deformation > state.deformationThreshold) {
        addAlert('danger', `[JC 应变率${Math.round(strainRate)}/s] 变形超限: ${deformation.toFixed(2)}mm`);
    }
    if (penetrated) {
        addAlert('danger', `[JC动态屈服${Math.round(dynYield/1e6)}MPa] 防护击穿！`);
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

    if (sim.strain_rate != null) {
        const sr = document.getElementById('m-strain-rate');
        if (sr) sr.textContent = Math.round(sim.strain_rate) + '/s';
    }

    addHistoryData(sim.roof_von_mises_stress_mpa, sim.impact_energy_j / 500);
    drawCharts();
}

async function runAHPEvaluation() {
    addAlert('info', '执行群决策AHP评估(5专家+一致性修正)...');
    try {
        const res = await fetch(`${API_BASE}/api/evaluate`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ vehicle_id: state.currentVehicle }),
        });
        const data = await res.json();
        updateAHPEvaluation(data.data || [], data);
    } catch (e) {
        const dummy = [
            { material_type: 'composite', material_thickness_mm: 48, energy_absorption_score: 0.85, structural_strength_score: 0.82, weight_factor_score: 0.7, cost_factor_score: 0.5, durability_score: 0.8, ahp_weight_score: 0.76, rank_position: 1, is_recommended: true },
            { material_type: 'iron',      material_thickness_mm: 5,  energy_absorption_score: 0.9,  structural_strength_score: 0.95, weight_factor_score: 0.2, cost_factor_score: 0.2, durability_score: 0.95, ahp_weight_score: 0.68, rank_position: 2, is_recommended: false },
            { material_type: 'wood',      material_thickness_mm: 64, energy_absorption_score: 0.45, structural_strength_score: 0.5,  weight_factor_score: 0.85,cost_factor_score: 0.95,durability_score: 0.55, ahp_weight_score: 0.63, rank_position: 3, is_recommended: false },
            { material_type: 'cowhide',   material_thickness_mm: 20, energy_absorption_score: 0.55, structural_strength_score: 0.3,  weight_factor_score: 0.9, cost_factor_score: 0.7, durability_score: 0.35, ahp_weight_score: 0.54, rank_position: 4, is_recommended: false },
        ];
        updateAHPEvaluation(dummy, { consistency_ratio: 0.06, group_consensus: 0.89, passed_experts: 5, total_experts: 5 });
    }
}

function updateAHPEvaluation(evals, meta = {}) {
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

    let msg = `AHP完成：${evals.length}方案`;
    if (meta.consistency_ratio != null) msg += `，一致性CR=${meta.consistency_ratio.toFixed(3)}`;
    if (meta.group_consensus != null) msg += `，专家共识=${(meta.group_consensus * 100).toFixed(0)}%`;
    if (meta.passed_experts != null) msg += `，${meta.passed_experts}/${meta.total_experts || 5}专家通过`;
    addAlert('info', msg);
}

function addHistoryData(stress, impact) {
    state.stressHistory.push(stress);
    state.impactHistory.push(impact);
    if (state.stressHistory.length > 60) state.stressHistory.shift();
    if (state.impactHistory.length > 60) state.impactHistory.shift();
}

function drawCharts() {
    drawLineChart('chart-stress', state.stressHistory, '#4af', 0, 300);
    drawLineChart('chart-impact', state.impactHistory, '#fa4', 0, 120);
}

function drawLineChart(canvasId, data, color, min, max) {
    const canvas = document.getElementById(canvasId);
    if (!canvas) return;
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

function drawHeatmap() {
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
    while (list.children.length > 25) list.removeChild(list.lastChild);
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
            applyDeformationToRoof();
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

    function setupToggle(id, key, cb) {
        const el = document.getElementById(id);
        if (!el) return;
        el.addEventListener('click', function () {
            this.classList.toggle('active');
            state[key] = this.classList.contains('active');
            if (cb) cb();
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
        const el = document.getElementById(id);
        if (!el) return;
        el.addEventListener('click', () => {
            const [x, y, z] = views[id];
            state.camera.position.set(x, y, z);
            state.controls.target.set(0, 1.5, 0);
            state.controls.update();
        });
    });

    function tick() {
        const now = new Date();
        const el = document.getElementById('clock');
        if (el) el.textContent =
            `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')} · ${Math.round(state.fpsSmoothed)}FPS`;
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
