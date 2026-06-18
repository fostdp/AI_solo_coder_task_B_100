import { AssaultCart3D } from './assault_cart_3d.js';
import { ProtectionPanel } from './protection_panel.js';

const API_BASE = 'http://127.0.0.1:8080';

window.addEventListener('DOMContentLoaded', () => {
    const cart3D = new AssaultCart3D('#canvas-container');
    const panel = new ProtectionPanel(cart3D, API_BASE);

    cart3D.start();
    panel.init();

    window.__app = { cart3D, panel };

    window.addEventListener('resize', () => cart3D.onResize());
});
