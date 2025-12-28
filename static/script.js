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

// NEW: Stacks for History Navigation
let historyStack = []; // Stores the currently visible path
let futureStack = [];  // Stores the points we moved back from

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

// 4. Update Dashboard Setup to handle 'rider' role
function setupDashboard(role, city) {
    userRole = role; userCity = city || "";
    document.getElementById('login-screen').classList.remove('active');
    document.getElementById('dashboard-screen').classList.add('active');
    document.getElementById('role-display').innerText = `(${userRole})`;
    document.getElementById('city-display').innerText = userCity ? `@ ${userCity}` : "";

    // Hide all panels initially
    ['admin-tools', 'manager-tools', 'manager-courier-module', 'guest-tools', 'rider-tools', 'admin-controls'].forEach(id => {
        const el = document.getElementById(id);
        if (el) el.style.display = 'none';
    });

    // Show specific panels based on role
    if (userRole === 'admin') {
        document.getElementById('admin-tools').style.display = 'block';
        document.getElementById('admin-controls').style.display = 'block';
        loadAdminPackages();
        loadAdminStats(); // <--- ADD THIS
    } else if (userRole === 'manager') {
        // Manager starts on the Package View
        document.getElementById('manager-tools').style.display = 'block';
        // Add Toggle Buttons dynamically if not present
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

// Main function to visualize a package
async function startTracking(id) {
    const res = await fetch(`${API_URL}/track_package?id=${id}`);
    const data = await res.json();

    if (!data.found) { alert("Package not found"); return; }

    trackData = data;
    trackingActive = true;

    // --- NEW: Initialize Stacks ---
    // Deep copy the history array into our Stack
    historyStack = [...data.history];
    futureStack = [];
    // ------------------------------

    // Show tracking pane
    document.getElementById('pkg-tracking-pane').style.display = 'block';

    // Hide toolbars logic... (Keep existing code)
    if (userRole === 'manager') document.getElementById('manager-tools').style.display = 'none';
    if (userRole === 'admin') document.getElementById('admin-tools').style.display = 'none';
    if (userRole !== 'guest') document.getElementById('close-tracking-btn').style.display = 'block';

    // Populate Tracking Pane logic... (Keep existing code)
    document.getElementById('trk-id').innerText = data.id;
    document.getElementById('trk-status').innerText = getStatusName(data.status);
    document.getElementById('trk-status').className = `status-badge st-${data.status}`;

    // Initial Render
    updateTrackingUI();
    drawMap();
}

// --- NEW TRACKING NAVIGATION FUNCTIONS ---

function trackPrev() {
    // We need at least one item to remain (the start point)
    if (historyStack.length > 1) {
        const point = historyStack.pop(); // Remove from History (LIFO)
        futureStack.push(point);          // Add to Future Stack
        updateTrackingUI();
        drawMap();
    }
}

function trackNext() {
    // If there is anything in the Future stack to "redo"
    if (futureStack.length > 0) {
        const point = futureStack.pop();  // Remove from Future (LIFO)
        historyStack.push(point);         // Add back to History
        updateTrackingUI();
        drawMap();
    }
}

function updateTrackingUI() {
    // Update the "Current View" text based on the top of the History Stack
    const currentPoint = historyStack[historyStack.length - 1];
    if (currentPoint) {
        document.getElementById('trk-current').innerText = currentPoint.city;
    }

    // Update History List based on Stack
    const histDiv = document.getElementById('trk-history-list');
    histDiv.innerHTML = historyStack.map(h =>
        `<div>✅ ${h.city} <span style="color:#777; font-size:10px;">${h.time.split(' ')[1] || ''}</span></div>`
    ).join('');

    // Future list remains static (the originally planned route)
    const futDiv = document.getElementById('trk-future-list');
    if (trackData.future && trackData.future.length > 0) {
        futDiv.innerHTML = trackData.future.map(c => `<div>🔹 ${c}</div>`).join('');
    } else {
        futDiv.innerHTML = "<div>🏁 Arrived / No Plan</div>";
    }
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
        if (historyStack.length > 1) {
            ctx.beginPath(); ctx.lineWidth = 4; ctx.strokeStyle = '#27ae60'; ctx.lineCap = 'round';
            for (let i = 0; i < historyStack.length - 1; i++) {
                const n1 = nodes.find(n => n.name === historyStack[i].city);
                const n2 = nodes.find(n => n.name === historyStack[i + 1].city);
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
    // 3. Draw Nodes

    // --- FIX STARTS HERE ---
    let currentVisCity = null;
    // Only calculate this if we are actively tracking and have history
    if (trackingActive && historyStack.length > 0) {
        currentVisCity = historyStack[historyStack.length - 1].city;
    }
    // --- FIX ENDS HERE ---

    nodes.forEach(n => {
        let color = '#3498db'; // Default Blue
        let radius = 18;

        if (trackingActive) {
            // Check History Stack
            if (historyStack.some(h => h.city === n.name)) color = '#27ae60';
            // Check Current Stack Top
            if (n.name === currentVisCity) { color = '#e67e22'; radius = 22; }
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

// 2. Manager Packages (With 'Return' button for Failed items)
async function loadManagerPackages() {
    if (userRole !== 'manager') return;
    const res = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const pkgs = await res.json();

    const render = (p, btn = '') => `
        <div class="pkg-item" onclick="startTracking(${p.id})">
            <b>#${p.id}</b> -> ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
            ${btn}
        </div>`;

    // Outgoing
    document.getElementById('tab-outgoing').innerHTML = pkgs.filter(p => p.status <= 1 && p.current === userCity)
        .map(p => render(p, p.status === 0 ? `<button onclick="event.stopPropagation(); updatePkg(${p.id}, 'load')">Load</button>` : '')).join('');

    // Incoming: Show Arrived (3) AND Failed (5) packages here
    document.getElementById('tab-incoming').innerHTML = pkgs.filter(p => (p.status === 3 || p.status === 5) && p.current === userCity)
        .map(p => {
            let actionBtn = '';
            if (p.status === 3) {
                actionBtn = `<div style="font-size:11px; color:#e67e22;">⬇ Arrived (Go to Riders)</div>`;
            } else if (p.status === 5) {
                // FAILED STATUS -> Show Return Button
                actionBtn = `<button onclick="event.stopPropagation(); updatePkg(${p.id}, 'return')" style="background:#c0392b;">↩ Return to Sender</button>`;
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
            <b>#${p.id}</b> ${p.current} -> ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
        </div>`).join('');
}

// 3. Admin Stats Loader
async function loadAdminStats() {
    if (userRole !== 'admin') return;
    const res = await fetch(`${API_URL}/admin_stats`);
    const stats = await res.json();

    // Inject into HTML (Make sure elements exist, see step 5)
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

    // --- CLIENT SIDE PRICE ESTIMATION ---
    let base = 10;
    let weightCost = weight * 2;
    let priorityCost = (type === 1) ? 50 : (type === 2 ? 20 : 0);
    let estimatedPrice = base + weightCost + priorityCost;

    // --- CONFIRMATION MODAL ---
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

// [New Functions for Rider/Courier Logic]

async function loadRidersAndAssignments() {
    // Load Riders
    const res = await fetch(`${API_URL}/get_riders`);
    const riders = await res.json();

    // Load Packages at Hub
    const res2 = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const allPkgs = await res2.json();

    // UPDATED: Look for Status 3 (Arrived) OR 6 (At Hub) so they appear in the list
    const hubPkgs = allPkgs.filter(p => p.status === 3 || p.status === 6);

    let html = `<div style="font-size:11px; margin-bottom:5px;">Waiting for Assignment: ${hubPkgs.length}</div>`;

    // Only show riders if we have them
    if (riders.length === 0) {
        html += `<div style="padding:10px; color:#777; font-size:12px;">No Riders Found</div>`;
    }

    riders.forEach(r => {
        html += `
            <div style="background:#f9f9f9; padding:8px; border-bottom:1px solid #ddd;">
                <b>${r.username}</b> (${r.vehicle})
                <button class="blue-btn" style="float:right; width:auto; padding:2px 8px;" 
                   onclick="assignPackages(${r.id})">Assign</button>
            </div>`;
    });
    document.getElementById('courier-list').innerHTML = html;
}

async function assignPackages(riderId) {
    // 1. Find the packages that need assignment (Status 3 or 6)
    const resPkgs = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const allPkgs = await resPkgs.json();

    // Filter for packages that are 'Arrived' (3) or 'At Hub' (6)
    const packagesToAssign = allPkgs.filter(p => p.status === 3 || p.status === 6);

    if (packagesToAssign.length === 0) {
        alert("No packages available to assign.");
        return;
    }

    // 2. Get the list of IDs (e.g., [101, 102])
    const packageIds = packagesToAssign.map(p => p.id);

    console.log(`Assigning packages ${packageIds.join(',')} to Rider ${riderId}`);

    // 3. Send Rider ID AND Package IDs to the backend
    const res = await fetch(`${API_URL}/assign_packages`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
            riderId: riderId,
            packageIds: packageIds // sending the specific list
        })
    });

    const data = await res.json();
    alert(data.message);

    // 4. Refresh the view
    loadRidersAndAssignments();
}

async function riderAction(id, action) {
    await fetch(`${API_URL}/rider_action`, { method: 'POST', body: JSON.stringify({ id, action }) });
    loadRiderPackages();
}


// --- MISSING FUNCTIONS & FIXES ---

// 1. Switch between Package View and Courier View (Fixing ID mismatch)
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

// 2. Create a new Rider (Connects to /api/add_rider)
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

// 3. Load Packages for the logged-in Rider (Connects to /api/rider_packages)
// 3. Load Packages for the logged-in Rider (Connects to /api/rider_packages)
async function loadRiderPackages() {
    const res = await fetch(`${API_URL}/rider_packages`);
    const pkgs = await res.json();
    const list = document.getElementById('rider-pkg-list');

    if (pkgs.length === 0) {
        list.innerHTML = "<div style='padding:10px; color:#777;'>No active deliveries</div>";
        return;
    }

    list.innerHTML = pkgs.map(p => {
        // Warning for previous failed attempts
        const attemptWarn = p.attempts > 0
            ? `<span style="color:#c0392b; font-size:11px; margin-left:5px;">(Attempt ${p.attempts + 1}/3)</span>`
            : '';

        return `
        <div class="pkg-item" style="border-left: 4px solid #3498db;" onclick="startTracking(${p.id})">
            <div style="font-size:14px; font-weight:bold;">
                #${p.id} - ${p.receiver} 
                ${attemptWarn}
            </div>
            <div style="font-size:12px; margin-bottom:5px;">📍 ${p.address}</div>
            
            <div style="display:flex; gap:5px;">
                <button onclick="event.stopPropagation(); riderAction(${p.id}, 'delivered')" 
                    style="background:#27ae60; color:white; padding:4px 8px; border:none; border-radius:3px; cursor:pointer;">
                    ✅ Delivered
                </button>
                <button onclick="event.stopPropagation(); riderAction(${p.id}, 'failed')" 
                    style="background:#c0392b; color:white; padding:4px 8px; border:none; border-radius:3px; cursor:pointer;">
                    ❌ Failed
                </button>
            </div>
        </div>
    `}).join('');
}

// Helper to inject toggle buttons for Manager
function addManagerToggles() {
    const container = document.getElementById('sidebar-container');
    const toggleDiv = document.createElement('div');
    toggleDiv.id = "mgr-toggle-container";
    toggleDiv.className = "card";
    toggleDiv.style.background = "#ecf0f1";
    toggleDiv.innerHTML = `
        <div style="display:flex; gap:5px;">
            <button onclick="switchModule('pkg')" style="flex:1; padding:8px; cursor:pointer;">📦 Packages</button>
            <button onclick="switchModule('courier')" style="flex:1; padding:8px; cursor:pointer;">🛵 Riders</button>
        </div>
    `;
    // Insert at the top of the sidebar
    container.insertBefore(toggleDiv, container.firstChild);
}