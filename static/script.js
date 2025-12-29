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
let trackData = null;

// History Navigation
let historyStack = [];
let futureStack = [];

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
    document.getElementById('role-display').innerText = role;
    document.getElementById('city-display').innerText = userCity ? userCity : "";

    ['admin-tools', 'manager-tools', 'manager-courier-module', 'guest-tools', 'rider-tools', 'admin-controls'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.style.display = 'none';
    });

    if (userRole === 'admin') {
        document.getElementById('admin-tools').style.display = 'block';
        document.getElementById('admin-controls').style.display = 'block';
        loadAdminPackages();
        loadAdminStats();
    } else if (userRole === 'manager') {
        document.getElementById('manager-tools').style.display = 'block';
        if (!document.getElementById('mgr-toggle-container')) addManagerToggles();
        loadManagerPackages();
    } else if (userRole === 'rider') {
        document.getElementById('rider-tools').style.display = 'block';
        loadRiderPackages();
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

async function startTracking(id) {
    const res = await fetch(`${API_URL}/track_package?id=${id}`);
    const data = await res.json();

    if (!data.found) { alert("Package not found"); return; }

    trackData = data;
    trackingActive = true;

    historyStack = [...data.history];
    futureStack = [];

    document.getElementById('pkg-tracking-pane').style.display = 'block';

    if (userRole === 'manager') document.getElementById('manager-tools').style.display = 'none';
    if (userRole === 'admin') document.getElementById('admin-tools').style.display = 'none';
    if (userRole !== 'guest') document.getElementById('close-tracking-btn').style.display = 'block';

    document.getElementById('trk-id').innerText = data.id;
    document.getElementById('trk-to').innerText = data.sender;
    document.getElementById('trk-from').innerText = data.receiver;
    document.getElementById('trk-address').innerText = data.address;
    document.getElementById('trk-status').innerText = getStatusName(data.status);
    document.getElementById('trk-status').className = `status-badge st-${data.status}`;

    updateTrackingUI();
    drawMap();
}

function trackPrev() {
    if (historyStack.length > 1) {
        const point = historyStack.pop();
        futureStack.push(point);
        updateTrackingUI();
        drawMap();
    }
}

function trackNext() {
    if (futureStack.length > 0) {
        const point = futureStack.pop();
        historyStack.push(point);
        updateTrackingUI();
        drawMap();
    }
}

function updateTrackingUI() {
    const currentPoint = historyStack[historyStack.length - 1];
    if (currentPoint) {
        document.getElementById('trk-current').innerText = currentPoint.city;
    }

    const histDiv = document.getElementById('trk-history-list');
    histDiv.innerHTML = historyStack.map(h =>
        `<div>✅ ${h.city} <span style="color:#6b7280; font-size:10px;">${h.time.split(' ')[1] || ''}</span></div>`
    ).join('');

    const futDiv = document.getElementById('trk-future-list');
    if (trackData.future && trackData.future.length > 0) {
        futDiv.innerHTML = trackData.future.map(c => `<div>📍 ${c}</div>`).join('');
    } else {
        futDiv.innerHTML = "<div>🏁 Arrived / No Plan</div>";
    }
}

function closeTracking() {
    trackingActive = false;
    trackData = null;
    document.getElementById('pkg-tracking-pane').style.display = 'none';

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
    const padding = 60, mapW = maxX - minX || 1, mapH = maxY - minY || 1;
    const cw = canvas.width - (padding * 2), ch = canvas.height - (padding * 2);
    const s = Math.min(cw / mapW, ch / mapH);
    transform = { scale: s, minX, minY, offsetX: padding + (cw - mapW * s) / 2, offsetY: padding + (ch - mapH * s) / 2 };
    nodes.forEach(n => { if (n !== draggedNode) { n.displayX = (n.x - minX) * s + transform.offsetX; n.displayY = (n.y - minY) * s + transform.offsetY; } });
}

function drawMap() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    if (!isDragging) fitMapToScreen();

    // Draw Edges with Enhanced Styling
    edges.forEach(e => {
        const n1 = nodes.find(n => n.id === e.source);
        const n2 = nodes.find(n => n.id === e.target);
        if (!n1 || !n2) return;

        ctx.beginPath();
        ctx.moveTo(n1.displayX, n1.displayY);
        ctx.lineTo(n2.displayX, n2.displayY);

        if (e.blocked) {
            ctx.strokeStyle = '#ef4444';
            ctx.lineWidth = 3;
            ctx.setLineDash([8, 8]);
        } else if (trackingActive) {
            ctx.strokeStyle = '#e5e7eb';
            ctx.lineWidth = 2;
            ctx.setLineDash([]);
        } else {
            ctx.strokeStyle = '#cbd5e1';
            ctx.lineWidth = 3;
            ctx.setLineDash([]);
        }

        ctx.lineCap = 'round';
        ctx.stroke();
        ctx.setLineDash([]);

        // Draw distance labels with improved styling
        if (!trackingActive) {
            const midX = (n1.displayX + n2.displayX) / 2;
            const midY = (n1.displayY + n2.displayY) / 2;
            const distText = e.weight + " km";

            // Background pill
            ctx.font = "bold 12px Inter";
            const textWidth = ctx.measureText(distText).width;
            const padding = 8;

            ctx.fillStyle = "rgba(255, 255, 255, 0.95)";
            ctx.shadowColor = "rgba(0, 0, 0, 0.1)";
            ctx.shadowBlur = 8;
            ctx.shadowOffsetY = 2;

            const rectX = midX - textWidth / 2 - padding;
            const rectY = midY - 8;
            const rectWidth = textWidth + padding * 2;
            const rectHeight = 16;
            const radius = 8;

            ctx.beginPath();
            ctx.moveTo(rectX + radius, rectY);
            ctx.lineTo(rectX + rectWidth - radius, rectY);
            ctx.quadraticCurveTo(rectX + rectWidth, rectY, rectX + rectWidth, rectY + radius);
            ctx.lineTo(rectX + rectWidth, rectY + rectHeight - radius);
            ctx.quadraticCurveTo(rectX + rectWidth, rectY + rectHeight, rectX + rectWidth - radius, rectY + rectHeight);
            ctx.lineTo(rectX + radius, rectY + rectHeight);
            ctx.quadraticCurveTo(rectX, rectY + rectHeight, rectX, rectY + rectHeight - radius);
            ctx.lineTo(rectX, rectY + radius);
            ctx.quadraticCurveTo(rectX, rectY, rectX + radius, rectY);
            ctx.closePath();
            ctx.fill();

            ctx.shadowColor = "transparent";
            ctx.shadowBlur = 0;
            ctx.shadowOffsetY = 0;

            // Text
            ctx.fillStyle = "#475569";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(distText, midX, midY);
        }
    });

    // Draw Tracking Routes with Enhanced Styling
    if (trackingActive && trackData) {
        // History Route (Green Gradient)
        if (historyStack.length > 1) {
            ctx.lineWidth = 5;
            ctx.lineCap = 'round';
            ctx.lineJoin = 'round';

            for (let i = 0; i < historyStack.length - 1; i++) {
                const n1 = nodes.find(n => n.name === historyStack[i].city);
                const n2 = nodes.find(n => n.name === historyStack[i + 1].city);
                if (n1 && n2) {
                    const gradient = ctx.createLinearGradient(n1.displayX, n1.displayY, n2.displayX, n2.displayY);
                    gradient.addColorStop(0, '#10b981');
                    gradient.addColorStop(1, '#059669');

                    ctx.strokeStyle = gradient;
                    ctx.shadowColor = "rgba(16, 185, 129, 0.3)";
                    ctx.shadowBlur = 8;

                    ctx.beginPath();
                    ctx.moveTo(n1.displayX, n1.displayY);
                    ctx.lineTo(n2.displayX, n2.displayY);
                    ctx.stroke();

                    ctx.shadowColor = "transparent";
                    ctx.shadowBlur = 0;
                }
            }
        }

        // Future Route (Dashed Blue)
        if (trackData.future && trackData.future.length > 0) {
            ctx.lineWidth = 4;
            ctx.setLineDash([12, 8]);
            ctx.lineCap = 'round';

            const curr = nodes.find(n => n.name === trackData.current);
            const firstFut = nodes.find(n => n.name === trackData.future[0]);
            if (curr && firstFut) {
                const gradient = ctx.createLinearGradient(curr.displayX, curr.displayY, firstFut.displayX, firstFut.displayY);
                gradient.addColorStop(0, '#3b82f6');
                gradient.addColorStop(1, '#2563eb');
                ctx.strokeStyle = gradient;
                ctx.beginPath();
                ctx.moveTo(curr.displayX, curr.displayY);
                ctx.lineTo(firstFut.displayX, firstFut.displayY);
                ctx.stroke();
            }

            for (let i = 0; i < trackData.future.length - 1; i++) {
                const n1 = nodes.find(n => n.name === trackData.future[i]);
                const n2 = nodes.find(n => n.name === trackData.future[i + 1]);
                if (n1 && n2) {
                    ctx.beginPath();
                    ctx.moveTo(n1.displayX, n1.displayY);
                    ctx.lineTo(n2.displayX, n2.displayY);
                    ctx.stroke();
                }
            }
            ctx.setLineDash([]);
        }
    }

    // Draw Nodes with Enhanced Styling
    let currentVisCity = null;
    if (trackingActive && historyStack.length > 0) {
        currentVisCity = historyStack[historyStack.length - 1].city;
    }

    nodes.forEach(n => {
        let color = '#6366f1';
        let radius = 20;
        let glowColor = 'rgba(99, 102, 241, 0.3)';

        if (trackingActive) {
            if (historyStack.some(h => h.city === n.name)) {
                color = '#10b981';
                glowColor = 'rgba(16, 185, 129, 0.3)';
            }
            if (n.name === currentVisCity) {
                color = '#f97316';
                radius = 26;
                glowColor = 'rgba(249, 115, 22, 0.4)';
            }
        }

        // Glow effect
        ctx.shadowColor = glowColor;
        ctx.shadowBlur = 20;

        // Outer ring
        ctx.beginPath();
        ctx.arc(n.displayX, n.displayY, radius + 3, 0, 2 * Math.PI);
        ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
        ctx.fill();

        // Main circle with gradient
        const gradient = ctx.createRadialGradient(n.displayX, n.displayY - radius / 3, 0, n.displayX, n.displayY, radius);
        const lightColor = adjustBrightness(color, 20);
        gradient.addColorStop(0, lightColor);
        gradient.addColorStop(1, color);

        ctx.beginPath();
        ctx.arc(n.displayX, n.displayY, radius, 0, 2 * Math.PI);
        ctx.fillStyle = gradient;
        ctx.fill();

        ctx.shadowColor = "transparent";
        ctx.shadowBlur = 0;

        // Border
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 3;
        ctx.stroke();

        // Label with improved styling
        if (!trackingActive || color !== '#ecf0f1') {
            // Label background
            ctx.font = "bold 14px Inter";
            const textWidth = ctx.measureText(n.name).width;
            const labelPadding = 8;

            ctx.fillStyle = "rgba(255, 255, 255, 0.95)";
            ctx.shadowColor = "rgba(0, 0, 0, 0.1)";
            ctx.shadowBlur = 8;
            ctx.shadowOffsetY = 2;

            const labelY = n.displayY + radius + 12;
            const labelHeight = 22;
            const labelRadius = 6;

            ctx.beginPath();
            ctx.moveTo(n.displayX - textWidth / 2 - labelPadding + labelRadius, labelY);
            ctx.lineTo(n.displayX + textWidth / 2 + labelPadding - labelRadius, labelY);
            ctx.quadraticCurveTo(n.displayX + textWidth / 2 + labelPadding, labelY, n.displayX + textWidth / 2 + labelPadding, labelY + labelRadius);
            ctx.lineTo(n.displayX + textWidth / 2 + labelPadding, labelY + labelHeight - labelRadius);
            ctx.quadraticCurveTo(n.displayX + textWidth / 2 + labelPadding, labelY + labelHeight, n.displayX + textWidth / 2 + labelPadding - labelRadius, labelY + labelHeight);
            ctx.lineTo(n.displayX - textWidth / 2 - labelPadding + labelRadius, labelY + labelHeight);
            ctx.quadraticCurveTo(n.displayX - textWidth / 2 - labelPadding, labelY + labelHeight, n.displayX - textWidth / 2 - labelPadding, labelY + labelHeight - labelRadius);
            ctx.lineTo(n.displayX - textWidth / 2 - labelPadding, labelY + labelRadius);
            ctx.quadraticCurveTo(n.displayX - textWidth / 2 - labelPadding, labelY, n.displayX - textWidth / 2 - labelPadding + labelRadius, labelY);
            ctx.closePath();
            ctx.fill();

            ctx.shadowColor = "transparent";
            ctx.shadowBlur = 0;
            ctx.shadowOffsetY = 0;

            // Label text
            ctx.fillStyle = "#1f2937";
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText(n.name, n.displayX, labelY + labelHeight / 2);
        }
    });
}

// Helper function to adjust color brightness
function adjustBrightness(hex, percent) {
    const num = parseInt(hex.replace("#", ""), 16);
    const amt = Math.round(2.55 * percent);
    const R = (num >> 16) + amt;
    const G = (num >> 8 & 0x00FF) + amt;
    const B = (num & 0x0000FF) + amt;
    return "#" + (0x1000000 + (R < 255 ? R < 1 ? 0 : R : 255) * 0x10000 +
        (G < 255 ? G < 1 ? 0 : G : 255) * 0x100 +
        (B < 255 ? B < 1 ? 0 : B : 255))
        .toString(16).slice(1);
}

// --- DATA FETCHERS ---

async function loadManagerPackages() {
    if (userRole !== 'manager') return;
    const res = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const pkgs = await res.json();

    const render = (p, btn = '') => `
        <div class="pkg-item" onclick="startTracking(${p.id})">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
                <b style="font-size:14px;">#${p.id}</b>
                <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
            </div>
            <div style="font-size:12px; color:#6b7280; margin-bottom:4px;">📍 ${p.dest}</div>
            ${btn}
        </div>`;

    document.getElementById('tab-outgoing').innerHTML = pkgs.filter(p => p.status <= 1 && p.current === userCity)
        .map(p => render(p, p.status === 0 ? `<button onclick="event.stopPropagation(); updatePkg(${p.id}, 'load')" style="background: linear-gradient(135deg, #10b981 0%, #059669 100%);">📦 Load</button>` : '')).join('');

    document.getElementById('tab-incoming').innerHTML = pkgs.filter(p => (p.status === 3 || p.status === 5) && p.current === userCity)
        .map(p => {
            let actionBtn = '';
            if (p.status === 3) {
                actionBtn = `<div style="font-size:11px; color:#f97316; font-weight:600; margin-top:8px;">⬇ Arrived (Assign to Riders)</div>`;
            } else if (p.status === 5) {
                actionBtn = `<button onclick="event.stopPropagation(); updatePkg(${p.id}, 'return')" style="background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);">↩ Return</button>`;
            }
            return render(p, actionBtn);
        }).join('');

    document.getElementById('tab-history').innerHTML = pkgs.map(p => render(p)).join('');
}

async function loadAdminPackages() {
    if (userRole !== 'admin') return;
    const res = await fetch(`${API_URL}/admin_packages`);
    const pkgs = await res.json();
    document.getElementById('admin-pkg-list').innerHTML = pkgs.map(p => `
        <div class="pkg-item" onclick="startTracking(${p.id})">
            <div style="display:flex; justify-content:space-between; align-items:center;">
                <div>
                    <b style="font-size:14px;">#${p.id}</b>
                    <div style="font-size:12px; color:#6b7280; margin-top:4px;">${p.current} → ${p.dest}</div>
                </div>
                <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
            </div>
        </div>`).join('');
}

async function loadAdminStats() {
    if (userRole !== 'admin') return;
    const res = await fetch(`${API_URL}/admin_stats`);
    const stats = await res.json();

    document.getElementById('stat-revenue').innerText = "$" + stats.revenue.toFixed(2);
    document.getElementById('stat-delivered').innerText = stats.delivered;
    document.getElementById('stat-transit').innerText = stats.inTransit;
    document.getElementById('stat-failed').innerText = stats.failed;
}

// --- ACTIONS & HELPERS ---
async function createPackage() {
    const sender = document.getElementById('pkg-sender').value;
    const weight = parseFloat(document.getElementById('pkg-weight').value);
    const type = parseInt(document.getElementById('pkg-type').value);

    if (!weight || !sender) { alert("Please fill details"); return; }

    let base = 5;
    let weightCost = weight * 1.1;
    let priorityCost = (type === 1) ? 20 : (type === 2 ? 5 : 0);
    let estimatedPrice = base + weightCost + priorityCost;

    const confirmed = confirm(`Estimated Price: $${estimatedPrice.toFixed(2)}\n\nDo you want to create this package?`);
    if (!confirmed) return;

    const body = {
        sender: sender,
        receiver: document.getElementById('pkg-receiver').value,
        address: document.getElementById('pkg-addr').value,
        dest: document.getElementById('pkg-dest').value,
        type: type,
        weight: weight
    };

    await fetch(`${API_URL}/add_package`, { method: 'POST', body: JSON.stringify(body) });
    loadManagerPackages();
}

async function updatePkg(id, action) {
    await fetch(`${API_URL}/update_pkg_status`,
        { method: 'POST', body: JSON.stringify({ id, action }) });
    loadManagerPackages();
}

async function addCity() {
    await fetch(`${API_URL}/add_city`,
        {
            method: 'POST', body: JSON.stringify(
                { name: document.getElementById('new-city-name').value, password: document.getElementById('new-city-pass').value })
        });
    loadMap();
}

async function addRoute() {
    await fetch(`${API_URL}/add_route`, {
        method: 'POST', body: JSON.stringify(
            { key: document.getElementById('route-key').value, distance: parseInt(document.getElementById('route-dist').value) })
    });
    loadMap();
}

async function toggleBlock(key, b) {
    await fetch(`${API_URL}/toggle_block`, {
        method: 'POST', body: JSON.stringify({ key, block: b })
    });
    loadMap();
}

async function nextShift() {
    const res = await fetch(`${API_URL}/next_shift`, {
        method: 'POST'
    });
    const data = await res.json();
    if (data.logs) alert(data.logs.join('\n'));
}

function renderRouteManager() {
    const list = document.getElementById('route-list');
    list.innerHTML = "";
    edges.forEach(e => {
        const n1 = nodes.find(n => n.id === e.source), n2 = nodes.find(n => n.id === e.target);
        if (n1 && n2) list.innerHTML += `<div>
            <span style="font-weight:600;">${n1.name}-${n2.name}</span>
            <button onclick="toggleBlock('${n1.name}-${n2.name}',${!e.blocked})" 
                style="background: ${e.blocked ? 'linear-gradient(135deg, #10b981 0%, #059669 100%)' : 'linear-gradient(135deg, #ef4444 0%, #dc2626 100%)'};">
                ${e.blocked ? '✓ Unblock' : '🚫 Block'}
            </button>
        </div>`;
    });
}

function getStatusName(s) {
    const names = ["Created", "Loaded", "In Transit", "Arrived", "Delivered", "Failed"];
    return names[s] || "?";
}

function showTab(name) {
    document.querySelectorAll('.tab-content').forEach(d => d.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.getElementById(`tab-${name}`).classList.add('active');
    event.target.classList.add('active');
}

async function handleMouseDown(e) {
    const mouseX = e.offsetX, mouseY = e.offsetY;
    for (let n of nodes) {
        const dx = mouseX - n.displayX, dy = mouseY - n.displayY;
        if (dx * dx + dy * dy < 625) {
            if (userRole === 'admin') {
                isDragging = true; draggedNode = n;
            }
        }
    }
}

function handleMouseMove(e) {
    if (!isDragging || !draggedNode || userRole !== 'admin') return;
    draggedNode.displayX = e.offsetX;
    draggedNode.displayY = e.offsetY;
    draggedNode.x = (draggedNode.displayX - transform.offsetX) / transform.scale + transform.minX;
    draggedNode.y = (draggedNode.displayY - transform.offsetY) / transform.scale + transform.minY;
    drawMap();
}

function handleMouseUp() {
    if (isDragging && draggedNode && userRole === 'admin') {
        fetch(`${API_URL}/update_node`, {
            method: 'POST', body: JSON.stringify(
                { name: draggedNode.name, x: draggedNode.x, y: draggedNode.y })
        });
    }
    isDragging = false; draggedNode = null;
}

// --- RIDER/COURIER LOGIC ---

async function loadRidersAndAssignments() {
    const res = await fetch(`${API_URL}/get_riders`);
    const riders = await res.json();

    const res2 = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const allPkgs = await res2.json();

    const hubPkgs = allPkgs.filter(p => p.status === 3 || p.status === 6);

    let html = `<div style="font-size:12px; margin-bottom:12px; padding:10px; background:#f0f9ff; border-radius:8px; color:#0369a1; font-weight:600;">
        📦 Waiting for Assignment: ${hubPkgs.length}
    </div>`;

    if (riders.length === 0) {
        html += `<div style="padding:20px; color:#9ca3af; font-size:13px; text-align:center;">No Riders Available</div>`;
    }

    riders.forEach(r => {
        html += `
            <div style="background:#f9fafb; padding:12px; border-radius:10px; border:1px solid #e5e7eb; margin-bottom:8px;">
                <div style="display:flex; justify-content:space-between; align-items:center;">
                    <div>
                        <div style="font-weight:700; color:#1f2937; margin-bottom:4px;">${r.username}</div>
                        <div style="font-size:11px; color:#6b7280;">${r.vehicle === 'bike' ? '🚴 Bike' : '🚐 CarryBus'}</div>
                    </div>
                    <button class="btn-action" style="width:auto; padding:8px 16px; font-size:12px;" 
                       onclick="assignPackages(${r.id})">Assign</button>
                </div>
            </div>`;
    });
    document.getElementById('courier-list').innerHTML = html;
}

async function assignPackages(riderId) {
    const resPkgs = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const allPkgs = await resPkgs.json();

    const packagesToAssign = allPkgs.filter(p => p.status === 3 || p.status === 6);

    if (packagesToAssign.length === 0) {
        alert("No packages available to assign.");
        return;
    }

    const packageIds = packagesToAssign.map(p => p.id);

    const res = await fetch(`${API_URL}/assign_packages`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            riderId: riderId,
            packageIds: packageIds
        })
    });

    const data = await res.json();
    alert(data.message);

    loadRidersAndAssignments();
}

async function riderAction(id, action) {
    await fetch(`${API_URL}/rider_action`, { method: 'POST', body: JSON.stringify({ id, action }) });
    loadRiderPackages();
}

function switchModule(mod) {
    if (mod === 'pkg') {
        document.getElementById('manager-tools').style.display = 'block';
        document.getElementById('manager-courier-module').style.display = 'none';
        loadManagerPackages();
    } else {
        document.getElementById('manager-tools').style.display = 'none';
        document.getElementById('manager-courier-module').style.display = 'block';
        loadRidersAndAssignments();
    }
}

async function createRider() {
    const u = document.getElementById('rider-user').value;
    const p = document.getElementById('rider-pass').value;
    const v = document.getElementById('rider-veh').value;

    if (!u || !p) { alert("Please enter username and password"); return; }

    const res = await fetch(`${API_URL}/add_rider`, {
        method: 'POST',
        body: JSON.stringify({ username: u, password: p, vehicle: v })
    });

    if (res.ok) {
        alert("Rider Added Successfully");
        document.getElementById('rider-user').value = "";
        document.getElementById('rider-pass').value = "";
        loadRidersAndAssignments();
    } else {
        alert("Error adding rider");
    }
}

async function loadRiderPackages() {
    const res = await fetch(`${API_URL}/rider_packages`);
    const pkgs = await res.json();
    const list = document.getElementById('rider-pkg-list');

    if (pkgs.length === 0) {
        list.innerHTML = "<div style='padding:20px; color:#9ca3af; text-align:center; font-size:13px;'>No active deliveries</div>";
        return;
    }

    list.innerHTML = pkgs.map(p => {
        const attemptWarn = p.attempts > 0
            ? `<span style="color:#ef4444; font-size:11px; font-weight:600; margin-left:8px;">⚠ Attempt ${p.attempts + 1}/3</span>`
            : '';

        return `
        <div class="pkg-item" style="border-left: 4px solid #3b82f6; background: linear-gradient(135deg, #ffffff 0%, #f8fafc 100%);" onclick="startTracking(${p.id})">
            <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:8px;">
                <div style="font-size:14px; font-weight:700; color:#1f2937;">
                    #${p.id} ${attemptWarn}
                </div>
            </div>
            <div style="font-size:13px; margin-bottom:4px; color:#475569;">👤 ${p.receiver}</div>
            <div style="font-size:12px; margin-bottom:12px; color:#64748b;">📍 ${p.address}</div>
            
            <div style="display:flex; gap:8px;">
                <button onclick="event.stopPropagation(); riderAction(${p.id}, 'delivered')" 
                    style="flex:1; background: linear-gradient(135deg, #10b981 0%, #059669 100%); color:white; padding:8px; border:none; border-radius:8px; cursor:pointer; font-weight:600; font-size:12px;">
                    ✅ Delivered
                </button>
                <button onclick="event.stopPropagation(); riderAction(${p.id}, 'failed')" 
                    style="flex:1; background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%); color:white; padding:8px; border:none; border-radius:8px; cursor:pointer; font-weight:600; font-size:12px;">
                    ❌ Failed
                </button>
            </div>
        </div>
    `}).join('');
}

function addManagerToggles() {
    const container = document.getElementById('sidebar-container');
    const toggleDiv = document.createElement('div');
    toggleDiv.id = "mgr-toggle-container";
    toggleDiv.className = "card";
    toggleDiv.style.background = "linear-gradient(135deg, #f0f9ff 0%, #e0f2fe 100%)";
    toggleDiv.style.borderColor = "#3b82f6";
    toggleDiv.innerHTML = `
        <div style="display:flex; gap:8px;">
            <button onclick="switchModule('pkg')" style="flex:1; padding:10px; cursor:pointer; background: linear-gradient(135deg, #6366f1 0%, #4f46e5 100%); color:white; border:none; border-radius:10px; font-weight:600; font-size:13px;">📦 Packages</button>
            <button onclick="switchModule('courier')" style="flex:1; padding:10px; cursor:pointer; background:white; color:#6b7280; border:2px solid #e5e7eb; border-radius:10px; font-weight:600; font-size:13px;">🛵 Riders</button>
        </div>
    `;
    container.insertBefore(toggleDiv, container.firstChild);
}