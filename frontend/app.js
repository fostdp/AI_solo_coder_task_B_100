import { AssaultCart3D } from './assault_cart_3d.js';
import { ProtectionPanel } from './protection_panel.js';
import { VehicleComparisonView } from './vehicle_comparison_view.js';
import { FormationOptimizerView } from './formation_optimizer_view.js';
import { VirtualDrivingView } from './virtual_driving_view.js';

const API_BASE = 'http://127.0.0.1:8080';

window.addEventListener('DOMContentLoaded', () => {
    const cart3D = new AssaultCart3D('#canvas-container');
    const panel = new ProtectionPanel(cart3D, API_BASE);
    cart3D.start();
    panel.init();

    const featureContainer = document.getElementById('feature-container');
    const appEl = document.getElementById('app');
    const tabBtns = document.querySelectorAll('.tab-btn');

    const views = {};
    let currentTab = 'simulation';

    function switchTab(tabName) {
        if (currentTab === tabName) return;
        currentTab = tabName;

        tabBtns.forEach(btn => {
            btn.classList.toggle('active', btn.dataset.tab === tabName);
        });

        if (tabName === 'simulation') {
            appEl.classList.remove('feature-mode');
            featureContainer.innerHTML = '';
            return;
        }

        appEl.classList.add('feature-mode');
        featureContainer.innerHTML = '';

        if (tabName === 'comparison') {
            const wrap = document.createElement('div');
            wrap.id = 'compare-view';
            wrap.style.maxWidth = '1400px';
            wrap.style.margin = '0 auto';
            featureContainer.appendChild(wrap);
            if (!views.comparison) {
                views.comparison = new VehicleComparisonView('#compare-view', API_BASE);
                views.comparison.init();
            } else {
                views.comparison.init();
            }
        } else if (tabName === 'cross-era') {
            const wrap = document.createElement('div');
            wrap.id = 'crossera-view';
            wrap.style.maxWidth = '1400px';
            wrap.style.margin = '0 auto';
            featureContainer.appendChild(wrap);
            if (!views.crossEra) {
                views.crossEra = new VehicleComparisonView('#crossera-view', API_BASE);
                views.crossEra.init(true);
            } else {
                views.crossEra.init(true);
            }
        } else if (tabName === 'formation') {
            const wrap = document.createElement('div');
            wrap.id = 'formation-view';
            wrap.style.maxWidth = '1400px';
            wrap.style.margin = '0 auto';
            featureContainer.appendChild(wrap);
            if (!views.formation) {
                views.formation = new FormationOptimizerView('#formation-view', API_BASE);
                views.formation.init();
            } else {
                views.formation.init();
            }
        } else if (tabName === 'driving') {
            const wrap = document.createElement('div');
            wrap.id = 'driving-view';
            wrap.style.maxWidth = '1400px';
            wrap.style.margin = '0 auto';
            featureContainer.appendChild(wrap);
            if (!views.driving) {
                views.driving = new VirtualDrivingView('#driving-view', API_BASE);
                views.driving.init();
            } else {
                views.driving.init();
            }
        }
    }

    tabBtns.forEach(btn => {
        btn.addEventListener('click', () => switchTab(btn.dataset.tab));
    });

    window.__app = {
        cart3D,
        panel,
        views,
        switchTab,
        API_BASE
    };

    window.addEventListener('resize', () => {
        if (currentTab === 'simulation') {
            cart3D.onResize();
        }
    });

    function updateClock() {
        const el = document.getElementById('clock');
        if (el) {
            const d = new Date();
            const pad = n => String(n).padStart(2, '0');
            el.textContent = `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
        }
    }
    setInterval(updateClock, 1000);
    updateClock();
});
