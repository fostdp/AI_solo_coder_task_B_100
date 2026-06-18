import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const GRID_N = 10;
const ROOF_LENGTH = 6.5;
const ROOF_WIDTH = 2.8;
const GPU_ROCK_COUNT = 400;

const DAMAGE_LEVELS = [
    { label: '完好', color: '#4af' },
    { label: '轻微', color: '#4fa' },
    { label: '中度', color: '#fa4' },
    { label: '严重', color: '#f84' },
    { label: '破坏', color: '#f44' },
];

export class AssaultCart3D {
    constructor(containerSelector = '#canvas-container') {
        this.container = document.querySelector(containerSelector);
        if (!this.container) throw new Error('Container not found: ' + containerSelector);

        this.state = {
            showRocks: true,
            showCloud: true,
            showStress: false,
            showWireframe: false,
            autoRotate: false,
            currentMaterial: 'wood',
            deformationField: new Array(100).fill(0),
            stressField: new Array(100).fill(0),
            clock: new THREE.Clock(),
            fpsSmoothed: 60,
            lastSimTick: 0,
        };

        this.scene = null;
        this.camera = null;
        this.renderer = null;
        this.controls = null;
        this.vehicle = null;
        this.roofMesh = null;
        this.roofOriginalGeometry = null;

        this.rockGPUPoints = null;
        this.rockUniforms = null;
        this.rockInstancedMesh = null;
        this.rockInstDummy = null;
        this.impactFlashPoints = null;
        this.flashUniforms = null;

        this._animating = false;
        this._animId = null;

        this._listeners = {};

        this.init();
    }

    on(event, callback) {
        if (!this._listeners[event]) this._listeners[event] = [];
        this._listeners[event].push(callback);
    }

    _emit(event, data) {
        if (this._listeners[event]) {
            this._listeners[event].forEach(cb => cb(data));
        }
    }

    init() {
        const w = this.container.clientWidth, h = this.container.clientHeight;

        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(0x050810);
        this.scene.fog = new THREE.Fog(0x050810, 15, 50);

        this.camera = new THREE.PerspectiveCamera(55, w / h, 0.05, 200);
        this.camera.position.set(10, 7, 10);

        this.renderer = new THREE.WebGLRenderer({ antialias: true, powerPreference: 'high-performance' });
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.setSize(w, h);
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.renderer.outputColorSpace = THREE.SRGBColorSpace;
        this.container.appendChild(this.renderer.domElement);

        this.controls = new OrbitControls(this.camera, this.renderer.domElement);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.08;
        this.controls.target.set(0, 1.5, 0);
        this.controls.minDistance = 3;
        this.controls.maxDistance = 25;

        this._setupLights();
        this._setupGround();

        this.createVehicle();
        this.createGPURockParticles();
        this.createRockInstancedMesh();
        this.createImpactFlashSystem();

        this.resetRoofColors();

        window.addEventListener('resize', () => this.onResize());
    }

    _setupLights() {
        const ambient = new THREE.AmbientLight(0x404860, 0.6);
        this.scene.add(ambient);

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
        this.scene.add(dir);

        const rim = new THREE.DirectionalLight(0x6688ff, 0.4);
        rim.position.set(-6, 4, -6);
        this.scene.add(rim);
    }

    _setupGround() {
        const groundGeo = new THREE.PlaneGeometry(80, 80);
        const groundMat = new THREE.MeshStandardMaterial({
            color: 0x1a2030, roughness: 0.95, metalness: 0.0,
        });
        const ground = new THREE.Mesh(groundGeo, groundMat);
        ground.rotation.x = -Math.PI / 2;
        ground.receiveShadow = true;
        this.scene.add(ground);

        const grid = new THREE.GridHelper(80, 80, 0x2a3050, 0x1a2040);
        grid.position.y = 0.001;
        this.scene.add(grid);
    }

    setMaterial(materialName) {
        this.state.currentMaterial = materialName;
        if (this.vehicle) {
            this.scene.remove(this.vehicle);
            this.vehicle = null;
            this.createVehicle();
            this.applyDeformationToRoof();
        }
    }

    getMaterial() { return this.state.currentMaterial; }

    createVehicle() {
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
        const mat = this.state.currentMaterial === 'iron' ? ironMat :
                    this.state.currentMaterial === 'cowhide' ? hideMat : woodMat;

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

        this.roofOriginalGeometry = roofGeo.clone();

        const roofMat = new THREE.MeshStandardMaterial({
            color: 0x8a6a4a, roughness: 0.8, metalness: 0.05,
            vertexColors: true, side: THREE.DoubleSide,
        });
        const roof = new THREE.Mesh(roofGeo, roofMat);
        roof.castShadow = true;
        roof.receiveShadow = true;
        group.add(roof);
        this.roofMesh = roof;

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

        this.vehicle = group;
        this.scene.add(group);
    }

    resetRoofColors() {
        if (!this.roofMesh) return;
        const geo = this.roofMesh.geometry;
        const count = geo.attributes.position.count;
        const colors = new Float32Array(count * 3);
        const c = this.colormap(0);
        for (let i = 0; i < count; i++) {
            colors[i * 3] = c.r;
            colors[i * 3 + 1] = c.g;
            colors[i * 3 + 2] = c.b;
        }
        geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
        geo.attributes.color.needsUpdate = true;
    }

    colormap(t) {
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

    createGPURockParticles() {
        const N = GPU_ROCK_COUNT;

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

        this.rockUniforms = {
            uTime:        { value: 0.0 },
            uGravity:     { value: -9.8 },
            uRoofMin:     { value: new THREE.Vector3(-ROOF_LENGTH / 2, 2.9, -ROOF_WIDTH / 2) },
            uRoofMax:     { value: new THREE.Vector3( ROOF_LENGTH / 2, 3.1,  ROOF_WIDTH / 2) },
            uSpawnHeight: { value: 18.0 },
            uFloorY:      { value: 0.5 },
            uPixelRatio:  { value: this.renderer.getPixelRatio() },
            uVisible:     { value: 1.0 },
            uImpact:      { value: 0.0 },
            uImpactPos:   { value: new THREE.Vector3(0, 3.0, 0) },
        };

        const mat = new THREE.ShaderMaterial({
            uniforms: this.rockUniforms,
            transparent: true,
            depthWrite: true,
            vertexColors: true,
            vertexShader: `
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

float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

void main() {
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
            fragmentShader: `
varying vec3 vColor;
varying float vAlive;

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

        this.rockGPUPoints = new THREE.Points(geo, mat);
        this.rockGPUPoints.frustumCulled = false;
        this.scene.add(this.rockGPUPoints);
    }

    createRockInstancedMesh() {
        const K = 40;
        const sphereGeo = new THREE.IcosahedronGeometry(1, 1);
        const instMat = new THREE.MeshStandardMaterial({
            color: 0x55504a, roughness: 0.95, metalness: 0.05, flatShading: true,
        });
        this.rockInstancedMesh = new THREE.InstancedMesh(sphereGeo, instMat, K);
        this.rockInstancedMesh.castShadow = true;
        this.rockInstancedMesh.receiveShadow = true;
        this.rockInstancedMesh.frustumCulled = false;
        this.rockInstDummy = new THREE.Object3D();

        const colors = new Float32Array(K * 3);
        for (let i = 0; i < K; i++) {
            const shade = 0.35 + Math.random() * 0.35;
            colors[i * 3]     = shade * (0.95 + Math.random() * 0.1);
            colors[i * 3 + 1] = shade * (0.9  + Math.random() * 0.1);
            colors[i * 3 + 2] = shade * (0.85 + Math.random() * 0.1);
        }
        this.rockInstancedMesh.instanceMatrix.setUsage(THREE.DynamicDrawUsage);
        this.rockInstancedMesh.instanceColor = new THREE.InstancedBufferAttribute(colors, 3);
        this.scene.add(this.rockInstancedMesh);
    }

    createImpactFlashSystem() {
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

        this.flashUniforms = {
            uTrigger: { value: 0.0 },
            uOrigin:  { value: new THREE.Vector3(0, 3.0, 0) },
            uTime:    { value: 0.0 },
            uPR:      { value: this.renderer.getPixelRatio() },
        };

        const mat = new THREE.ShaderMaterial({
            uniforms: this.flashUniforms,
            transparent: true,
            depthWrite: false,
            blending: THREE.AdditiveBlending,
            vertexShader: `
attribute float aSeg;
attribute float aLifeSeed;
uniform float uTrigger;
uniform vec3  uOrigin;
uniform float uTime;
uniform float uPR;
varying float vLife;

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

    vec4 mv = modelViewMatrix * vec4(pos, 1.0);
    gl_Position = projectionMatrix * mv;
    float vSize = (8.0 + speed) * uPR * vLife;
    gl_PointSize = clamp(vSize / max(-mv.z * 0.01, 0.2), 1.0, 80.0);
}
            `,
            fragmentShader: `
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

        this.impactFlashPoints = new THREE.Points(geo, mat);
        this.impactFlashPoints.frustumCulled = false;
        this.scene.add(this.impactFlashPoints);
    }

    triggerImpactFlash(worldX, worldZ) {
        if (!this.flashUniforms) return;
        this.flashUniforms.uOrigin.value.set(worldX, 3.0, worldZ);
        this.flashUniforms.uTrigger.value = 1.0;

        if (this.rockUniforms) {
            this.rockUniforms.uImpactPos.value.set(worldX, 3.0, worldZ);
            this.rockUniforms.uImpact.value = 1.0;
        }

        const start = performance.now();
        const duration = 900;
        const fade = () => {
            const t = (performance.now() - start) / duration;
            if (t >= 1) return;
            if (this.flashUniforms) this.flashUniforms.uTrigger.value = 1.0 - t;
            if (this.rockUniforms)   this.rockUniforms.uImpact.value = (1.0 - t) * 0.7;
            requestAnimationFrame(fade);
        };
        requestAnimationFrame(fade);
    }

    setDeformationField(field, isStress = false) {
        if (isStress) {
            this.state.stressField = field;
        } else {
            this.state.deformationField = field;
        }
        this.applyDeformationToRoof();
    }

    applyDeformationToRoof() {
        if (!this.roofMesh) return;
        const geo = this.roofMesh.geometry;
        const origPos = this.roofOriginalGeometry.attributes.position;
        const pos = geo.attributes.position;
        const colors = geo.attributes.color.array;

        const field = this.state.showStress ? this.state.stressField : this.state.deformationField;

        let maxVal = 0;
        field.forEach(v => { if (v > maxVal) maxVal = v; });
        if (maxVal <= 0) maxVal = 1;

        for (let gi = 0; gi < GRID_N; gi++) {
            for (let gj = 0; gj < GRID_N; gj++) {
                const idx = gi * GRID_N + gj;
                const vIdx = idx;
                const val = field[idx];
                const normalized = val / maxVal;

                if (!this.state.showStress && this.state.showCloud) {
                    const defMm = val;
                    const origY = origPos.getY(vIdx);
                    pos.setY(vIdx, origY - defMm / 1000 * 3);
                }

                const c = this.colormap(this.state.showCloud || this.state.showStress ? normalized : 0);
                colors[vIdx * 3] = c.r;
                colors[vIdx * 3 + 1] = c.g;
                colors[vIdx * 3 + 2] = c.b;
            }
        }

        pos.needsUpdate = true;
        geo.attributes.color.needsUpdate = true;
        geo.computeVertexNormals();

        if (this.roofMesh.material) {
            this.roofMesh.material.wireframe = this.state.showWireframe;
        }
    }

    onResize() {
        if (!this.container) return;
        const w = this.container.clientWidth, h = this.container.clientHeight;
        this.camera.aspect = w / h;
        this.camera.updateProjectionMatrix();
        this.renderer.setSize(w, h);
        if (this.rockUniforms) {
            this.rockUniforms.uPixelRatio.value = this.renderer.getPixelRatio();
        }
        if (this.flashUniforms) {
            this.flashUniforms.uPR.value = this.renderer.getPixelRatio();
        }
        this._emit('resize', { w, h });
    }

    getFps() { return this.state.fpsSmoothed; }

    setShowRocks(v) { this.state.showRocks = v; }
    setShowCloud(v) { this.state.showCloud = v; this.applyDeformationToRoof(); }
    setShowStress(v) { this.state.showStress = v; this.applyDeformationToRoof(); }
    setShowWireframe(v) { this.state.showWireframe = v; this.applyDeformationToRoof(); }
    setAutoRotate(v) { this.state.autoRotate = v; }

    setView(viewName) {
        const views = {
            front: [0, 3, 12],
            side:  [12, 3, 0],
            top:   [0.1, 15, 0.1],
            iso:   [10, 7, 10],
        };
        const v = views[viewName];
        if (v) {
            this.camera.position.set(v[0], v[1], v[2]);
            this.controls.target.set(0, 1.5, 0);
            this.controls.update();
        }
    }

    resetView() {
        this.camera.position.set(10, 7, 10);
        this.controls.target.set(0, 1.5, 0);
        this.controls.update();
        if (this.vehicle) this.vehicle.rotation.set(0, 0, 0);
    }

    start() {
        if (this._animating) return;
        this._animating = true;
        this.state.clock.start();
        this._animate();
    }

    stop() {
        this._animating = false;
        if (this._animId) {
            cancelAnimationFrame(this._animId);
            this._animId = null;
        }
    }

    _updateInstancedRocks(dt) {
        if (!this.rockInstancedMesh) return;
        this.rockInstancedMesh.visible = this.state.showRocks;
        if (!this.state.showRocks) return;

        const K = this.rockInstancedMesh.count;
        const cam = this.camera.position;
        const t = this.state.clock.getElapsedTime();

        for (let i = 0; i < K; i++) {
            const s = (i + 1) / K;
            const period = 2.0 + s * 5.0;
            const tt = (t + s * 7.3) % period;

            const startX = (Math.sin(i * 27.3) * 0.5) * ROOF_LENGTH * 0.9;
            const startZ = (Math.cos(i * 13.7) * 0.5) * ROOF_WIDTH * 0.9;
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
                this.rockInstDummy.position.set(0, -1000, 0);
            } else {
                this.rockInstDummy.position.set(x, y, z);
            }

            const scale = 0.15 + ((i * 37) % 100) / 250.0;
            this.rockInstDummy.scale.set(scale, scale, scale);
            this.rockInstDummy.rotation.set(i * 0.7 + t, i * 1.3 + t * 0.5, i * 2.1);
            this.rockInstDummy.updateMatrix();
            this.rockInstancedMesh.setMatrixAt(i, this.rockInstDummy.matrix);
        }
        this.rockInstancedMesh.instanceMatrix.needsUpdate = true;
    }

    _animate() {
        if (!this._animating) return;
        this._animId = requestAnimationFrame(() => this._animate());

        const dt = Math.min(this.state.clock.getDelta(), 0.05);
        const t = this.state.clock.getElapsedTime();

        if (this.rockUniforms) {
            this.rockUniforms.uTime.value = t;
            this.rockUniforms.uVisible.value = this.state.showRocks ? 1.0 : 0.0;
        }
        if (this.rockGPUPoints) {
            this.rockGPUPoints.visible = this.state.showRocks;
        }
        if (this.flashUniforms) {
            this.flashUniforms.uTime.value = t;
        }

        this._updateInstancedRocks(dt);

        if (this.state.autoRotate && this.vehicle) {
            this.vehicle.rotation.y += dt * 0.2;
        }

        if (t - this.state.lastSimTick > 1.5) {
            this.state.lastSimTick = t;
            const fx = (Math.random() - 0.5) * ROOF_LENGTH * 0.7;
            const fz = (Math.random() - 0.5) * ROOF_WIDTH * 0.7;
            this.triggerImpactFlash(fx, fz);
            this._emit('autoImpact', { x: fx, z: fz, t });
        }

        this.controls.update();
        this.renderer.render(this.scene, this.camera);

        const fps = 1.0 / Math.max(dt, 1e-4);
        this.state.fpsSmoothed = this.state.fpsSmoothed * 0.9 + fps * 0.1;
        this._emit('fps', this.state.fpsSmoothed);
    }

    static get DAMAGE_LEVELS() { return DAMAGE_LEVELS; }
    static get GRID_N() { return GRID_N; }
    static get ROOF_LENGTH() { return ROOF_LENGTH; }
    static get ROOF_WIDTH() { return ROOF_WIDTH; }
}

export default AssaultCart3D;
