const API_URL = "http://localhost:8080/api";

// --- GLOBAL STATE ---
let canvas, ctx;
let nodes = [];
let edges = [];
let userRole = "guest";
let userCity = "";

// Drag State
let isDragging = false, draggedNode = null;
let transform = { scale: 1, minX: 0, minY: 0, offsetX: 0, offsetY: 0 };

// TRACKING STATE
let trackingActive = false;
let trackData = null; // { history: [{city, time}], future: [city, city] }

window.onload = () => {
    canvas = document.getElementById('mapCanvas');
    ctx = canvas.getContext('2d');
    canvas.addEventListener('mousedown', handleMouseDown);
    canvas.addEventListener('mousemove', handleMouseMove);
    canvas.addEventListener('mouseup', handleMouseUp);
    window.addEventListener('resize', () => { resizeCanvas(); drawMap(); });
    resizeCanvas();
};

function resizeCanvas() {
    if (canvas && canvas.parentElement) {
        canvas.width = canvas.parentElement.clientWidth;
        canvas.height = canvas.parentElement.clientHeight;
    }
}

// --- AUTH & DASHBOARD ---
async function login() {
    const u = document.getElementById('username').value;
    const p = document.getElementById('password').value;
    const res = await fetch(`${API_URL}/login`, { method: 'POST', body: JSON.stringify({ username: u, password: p }) });
    const data = await res.json();
    if (data.status === 'success') setupDashboard(data.role, data.city);
    else document.getElementById('login-msg').innerText = data.message;
}

function loginAsGuest() { setupDashboard('guest', ''); }
function logout() { location.reload(); }

function setupDashboard(role, city) {
    userRole = role; userCity = city || "";
    document.getElementById('login-screen').classList.remove('active');
    document.getElementById('dashboard-screen').classList.add('active');
    document.getElementById('role-display').innerText = `(${userRole})`;
    document.getElementById('city-display').innerText = userCity ? `@ ${userCity}` : "";

    // Hide all sidebars
    ['admin-tools', 'manager-tools', 'guest-tools', 'admin-controls'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.style.display = 'none';
    });

    // Show relevant sidebar
    if (userRole === 'admin') {
        document.getElementById('admin-tools').style.display = 'block';
        document.getElementById('admin-controls').style.display = 'block';
        loadAdminPackages();
    } else if (userRole === 'manager') {
        document.getElementById('manager-tools').style.display = 'block';
        loadManagerPackages();
        setInterval(loadManagerPackages, 3000);
    } else {
        document.getElementById('guest-tools').style.display = 'block';
    }
    loadMap(true);
}

// --- TRACKING LOGIC ---

async function trackPackageByID() {
    const id = document.getElementById('guest-track-id').value;
    startTracking(id);
}

// Main function to visualize a package
async function startTracking(id) {
    const res = await fetch(`${API_URL}/track_package?id=${id}`);
    const data = await res.json();

    if (!data.found) { alert("Package not found"); return; }

    trackData = data;
    trackingActive = true;

    // Show tracking pane
    document.getElementById('pkg-tracking-pane').style.display = 'block';

    // Hide toolbars if not guest to give space
    if (userRole === 'manager') document.getElementById('manager-tools').style.display = 'none';
    if (userRole === 'admin') document.getElementById('admin-tools').style.display = 'none';
    if (userRole !== 'guest') document.getElementById('close-tracking-btn').style.display = 'block';

    // Populate Tracking Pane
    document.getElementById('trk-id').innerText = data.id;
    document.getElementById('trk-status').innerText = getStatusName(data.status);
    document.getElementById('trk-status').className = `status-badge st-${data.status}`;
    document.getElementById('trk-current').innerText = data.current;

    // History List
    const histDiv = document.getElementById('trk-history-list');
    histDiv.innerHTML = data.history.map(h =>
        `<div>✅ ${h.city} <span style="color:#777; font-size:10px;">${h.time.split(' ')[1] || ''}</span></div>`
    ).join('');

    // Future List
    const futDiv = document.getElementById('trk-future-list');
    if (data.future && data.future.length > 0) {
        futDiv.innerHTML = data.future.map(c => `<div>🔹 ${c}</div>`).join('');
    } else {
        futDiv.innerHTML = "<div>🏁 Arrived / No Plan</div>";
    }

    drawMap();
}

function closeTracking() {
    trackingActive = false;
    trackData = null;
    document.getElementById('pkg-tracking-pane').style.display = 'none';

    // Restore Sidebars
    if (userRole === 'manager') document.getElementById('manager-tools').style.display = 'block';
    if (userRole === 'admin') document.getElementById('admin-tools').style.display = 'block';

    drawMap();
}

// --- MAP ENGINE ---

async function loadMap() {
    const res = await fetch(`${API_URL}/map`);
    const data = await res.json();
    nodes = data.nodes || []; edges = data.edges || [];
    fitMapToScreen();
    drawMap();
    if (userRole === 'admin') renderRouteManager();
}

function fitMapToScreen() {
    if (nodes.length === 0) return;
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    nodes.forEach(n => { if (n.x < minX) minX = n.x; if (n.x > maxX) maxX = n.x; if (n.y < minY) minY = n.y; if (n.y > maxY) maxY = n.y; });
    const padding = 50, mapW = maxX - minX || 1, mapH = maxY - minY || 1;
    const cw = canvas.width - (padding * 2), ch = canvas.height - (padding * 2);
    const s = Math.min(cw / mapW, ch / mapH);
    transform = { scale: s, minX, minY, offsetX: padding + (cw - mapW * s) / 2, offsetY: padding + (ch - mapH * s) / 2 };
    nodes.forEach(n => { if (n !== draggedNode) { n.displayX = (n.x - minX) * s + transform.offsetX; n.displayY = (n.y - minY) * s + transform.offsetY; } });
}

function drawMap() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!isDragging) fitMapToScreen();

    // 1. Draw Edges
    edges.forEach(e => {
        const n1 = nodes.find(n => n.id === e.source);
        const n2 = nodes.find(n => n.id === e.target);
        if (!n1 || !n2) return;

        // Draw the line (Existing code)
        ctx.beginPath();
        ctx.moveTo(n1.displayX, n1.displayY);
        ctx.lineTo(n2.displayX, n2.displayY);

        if (e.blocked) ctx.strokeStyle = '#e74c3c';
        else if (trackingActive) ctx.strokeStyle = '#ecf0f1';
        else ctx.strokeStyle = '#95a5a6';

        ctx.lineWidth = 2; ctx.stroke();

        // --- NEW CODE STARTS HERE: Draw Distance Text ---
        // Only show distance if we aren't in "tracking mode" (to keep it clean)
        // or you can remove the condition to always show it.
        if (!trackingActive) {
            const midX = (n1.displayX + n2.displayX) / 2;
            const midY = (n1.displayY + n2.displayY) / 2;
            const distText = e.weight + " km";

            // Optional: Draw a small white background for readability
            ctx.font = "bold 11px Arial";
            const textWidth = ctx.measureText(distText).width;
            
            ctx.fillStyle = "rgba(255, 255, 255, 0.8)";
            ctx.fillRect(midX - textWidth / 2 - 2, midY - 6, textWidth + 4, 12);

            // Draw the text
            ctx.fillStyle = "#555"; // Dark grey color
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(distText, midX, midY);
        }
        // --- NEW CODE ENDS HERE ---
    });

    // 2. Draw Tracking Routes
    if (trackingActive && trackData) {

        // A. HISTORY (Solid Green)
        if (trackData.history && trackData.history.length > 1) {
            ctx.beginPath(); ctx.lineWidth = 4; ctx.strokeStyle = '#27ae60'; ctx.lineCap = 'round';
            for (let i = 0; i < trackData.history.length - 1; i++) {
                const n1 = nodes.find(n => n.name === trackData.history[i].city);
                const n2 = nodes.find(n => n.name === trackData.history[i + 1].city);
                if (n1 && n2) { ctx.moveTo(n1.displayX, n1.displayY); ctx.lineTo(n2.displayX, n2.displayY); }
            }
            ctx.stroke();
        }

        // B. FUTURE (Dashed Blue)
        // Connect Current -> Future[0] -> Future[1]...
        if (trackData.future && trackData.future.length > 0) {
            ctx.beginPath(); ctx.lineWidth = 3; ctx.strokeStyle = '#3498db'; ctx.setLineDash([10, 10]);

            // Link current to first planned
            const curr = nodes.find(n => n.name === trackData.current);
            const firstFut = nodes.find(n => n.name === trackData.future[0]);
            if (curr && firstFut) { ctx.moveTo(curr.displayX, curr.displayY); ctx.lineTo(firstFut.displayX, firstFut.displayY); }

            // Link rest
            for (let i = 0; i < trackData.future.length - 1; i++) {
                const n1 = nodes.find(n => n.name === trackData.future[i]);
                const n2 = nodes.find(n => n.name === trackData.future[i + 1]);
                if (n1 && n2) { ctx.moveTo(n1.displayX, n1.displayY); ctx.lineTo(n2.displayX, n2.displayY); }
            }
            ctx.stroke(); ctx.setLineDash([]);
        }
    }

    // 3. Draw Nodes
    nodes.forEach(n => {
        let color = '#3498db'; let radius = 18;

        if (trackingActive && trackData) {
            color = '#ecf0f1'; // Fade out irrelevant

            // Check History
            if (trackData.history.some(h => h.city === n.name)) color = '#27ae60';
            // Check Future
            if (trackData.future && trackData.future.includes(n.name)) color = '#3498db';
            // Check Ends
            if (n.name === trackData.current) { color = '#e67e22'; radius = 22; }
        }

        ctx.beginPath(); ctx.arc(n.displayX, n.displayY, radius, 0, 2 * Math.PI);
        ctx.fillStyle = color; ctx.fill();
        ctx.strokeStyle = 'white'; ctx.lineWidth = 3; ctx.stroke();

        // Label logic
        if (!trackingActive || color !== '#ecf0f1') {
            ctx.fillStyle = "#2c3e50"; ctx.font = "bold 13px Segoe UI";
            ctx.textAlign = "center"; ctx.textBaseline = "top";
            ctx.fillText(n.name, n.displayX, n.displayY + radius + 6);
        }
    });
}

// --- DATA FETCHERS ---

async function loadManagerPackages() {
    if (userRole !== 'manager') return;
    const res = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const pkgs = await res.json();

    const render = (p, btn = '') => `
        <div class="pkg-item" onclick="startTracking(${p.id})">
            <b>#${p.id}</b> -> ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
            ${btn}
        </div>`;

    document.getElementById('tab-outgoing').innerHTML = pkgs.filter(p => p.status <= 1 && p.current === userCity)
        .map(p => render(p, p.status === 0 ? `<button onclick="event.stopPropagation(); updatePkg(${p.id}, 'load')">Load</button>` : '')).join('');

    document.getElementById('tab-incoming').innerHTML = pkgs.filter(p => p.status === 3 && p.current === userCity)
        .map(p => `
            <div class="pkg-item" onclick="startTracking(${p.id})">
                <b>#${p.id}</b> from ${p.sender}
                <button onclick="event.stopPropagation(); updatePkg(${p.id}, 'deliver')">Deliver</button>
                <button onclick="event.stopPropagation(); updatePkg(${p.id}, 'return')">Return</button>
            </div>`).join('');

    document.getElementById('tab-history').innerHTML = pkgs.map(p => render(p)).join('');
}

async function loadAdminPackages() {
    if (userRole !== 'admin') return;
    const res = await fetch(`${API_URL}/admin_packages`);
    const pkgs = await res.json();
    document.getElementById('admin-pkg-list').innerHTML = pkgs.map(p => `
        <div class="pkg-item" onclick="startTracking(${p.id})">
            <b>#${p.id}</b> ${p.current} -> ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
        </div>`).join('');
}

// --- ACTIONS & HELPERS ---
async function createPackage() { const body = { sender: document.getElementById('pkg-sender').value, receiver: document.getElementById('pkg-receiver').value, address: document.getElementById('pkg-addr').value, dest: document.getElementById('pkg-dest').value, type: parseInt(document.getElementById('pkg-type').value), weight: parseFloat(document.getElementById('pkg-weight').value) }; await fetch(`${API_URL}/add_package`, { method: 'POST', body: JSON.stringify(body) }); loadManagerPackages(); }
async function updatePkg(id, action) { await fetch(`${API_URL}/update_pkg_status`, { method: 'POST', body: JSON.stringify({ id, action }) }); loadManagerPackages(); }
async function addCity() { await fetch(`${API_URL}/add_city`, { method: 'POST', body: JSON.stringify({ name: document.getElementById('new-city-name').value, password: document.getElementById('new-city-pass').value }) }); loadMap(); }
async function addRoute() { await fetch(`${API_URL}/add_route`, { method: 'POST', body: JSON.stringify({ key: document.getElementById('route-key').value, distance: parseInt(document.getElementById('route-dist').value) }) }); loadMap(); }
async function toggleBlock(key, b) { await fetch(`${API_URL}/toggle_block`, { method: 'POST', body: JSON.stringify({ key, block: b }) }); loadMap(); }
async function nextShift() { const res = await fetch(`${API_URL}/next_shift`, { method: 'POST' }); const data = await res.json(); if (data.logs) alert(data.logs.join('\n')); }

function renderRouteManager() { const list = document.getElementById('route-list'); list.innerHTML = ""; edges.forEach(e => { const n1 = nodes.find(n => n.id === e.source), n2 = nodes.find(n => n.id === e.target); if (n1 && n2) list.innerHTML += `<div>${n1.name}-${n2.name} <button onclick="toggleBlock('${n1.name}-${n2.name}',${!e.blocked})">${e.blocked ? 'Unblock' : 'Block'}</button></div>`; }); }
function getStatusName(s) { const names = ["Created", "Loaded", "In Transit", "Arrived", "Delivered", "Failed"]; return names[s] || "?"; }
function showTab(name) { document.querySelectorAll('.tab-content').forEach(d => d.classList.remove('active')); document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active')); document.getElementById(`tab-${name}`).classList.add('active'); }

async function handleMouseDown(e) { const mouseX = e.offsetX, mouseY = e.offsetY; for (let n of nodes) { const dx = mouseX - n.displayX, dy = mouseY - n.displayY; if (dx * dx + dy * dy < 400) { if (userRole === 'admin') { isDragging = true; draggedNode = n; } } } }
function handleMouseMove(e) { if (!isDragging || !draggedNode || userRole !== 'admin') return; draggedNode.displayX = e.offsetX; draggedNode.displayY = e.offsetY; draggedNode.x = (draggedNode.displayX - transform.offsetX) / transform.scale + transform.minX; draggedNode.y = (draggedNode.displayY - transform.offsetY) / transform.scale + transform.minY; drawMap(); }
function handleMouseUp() { if (isDragging && draggedNode && userRole === 'admin') { fetch(`${API_URL}/update_node`, { method: 'POST', body: JSON.stringify({ name: draggedNode.name, x: draggedNode.x, y: draggedNode.y }) }); } isDragging = false; draggedNode = null; }