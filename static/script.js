const API_URL = "http://localhost:8080/api";
let canvas, ctx, nodes = [], edges = [];
let userRole = "guest";
let userCity = "";
let isDragging = false, draggedNode = null;
let transform = { scale: 1, minX: 0, minY: 0, offsetX: 0, offsetY: 0 };

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

// --- Login & Role ---
async function login() {
    const u = document.getElementById('username').value;
    const p = document.getElementById('password').value;
    const res = await fetch(`${API_URL}/login`, { method: 'POST', body: JSON.stringify({ username: u, password: p }) });
    const data = await res.json();
    
    if (data.status === 'success') {
        userRole = data.role;
        userCity = data.city || "";
        
        document.getElementById('login-screen').classList.remove('active');
        document.getElementById('dashboard-screen').classList.add('active');
        document.getElementById('role-display').innerText = `(${userRole})`;
        if(userCity) document.getElementById('city-display').innerText = `@ ${userCity}`;

        if (userRole === 'admin') {
            document.getElementById('admin-tools').style.display = 'block';
            document.getElementById('admin-controls').style.display = 'block';
        } else {
            document.getElementById('admin-tools').style.display = 'none';
            document.getElementById('manager-tools').style.display = 'block';
            loadManagerPackages(); // Initial Load
            setInterval(loadManagerPackages, 3000); // Auto-refresh for updates
        }
        loadMap(true);
    } else { document.getElementById('login-msg').innerText = data.message; }
}

// --- Manager Functions ---
async function createPackage() {
    const body = {
        sender: document.getElementById('pkg-sender').value,
        receiver: document.getElementById('pkg-receiver').value,
        address: document.getElementById('pkg-addr').value,
        dest: document.getElementById('pkg-dest').value,
        type: parseInt(document.getElementById('pkg-type').value),
        weight: parseFloat(document.getElementById('pkg-weight').value)
    };
    await fetch(`${API_URL}/add_package`, { method: 'POST', body: JSON.stringify(body) });
    loadManagerPackages();
}

async function loadManagerPackages() {
    if(userRole !== 'manager') return;
    const res = await fetch(`${API_URL}/manager_packages?city=${userCity}`);
    const pkgs = await res.json();
    
    // Render Outgoing (Created/Loaded at source)
    const outDiv = document.getElementById('tab-outgoing');
    outDiv.innerHTML = pkgs.filter(p => p.status <= 1 && p.current === userCity).map(p => `
        <div class="pkg-item">
            <b>#${p.id}</b> -> ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span>
            ${p.status === 0 ? `<button onclick="updatePkg(${p.id}, 'load')">Load</button>` : ''}
        </div>`).join('');

    // Render Incoming (Arrived at destination = userCity)
    const inDiv = document.getElementById('tab-incoming');
    inDiv.innerHTML = pkgs.filter(p => p.status === 3 && p.current === userCity).map(p => `
        <div class="pkg-item">
            <b>#${p.id}</b> from ${p.sender}
            <button onclick="updatePkg(${p.id}, 'deliver')">Deliver</button>
            <button onclick="updatePkg(${p.id}, 'return')">Return</button>
        </div>`).join('');

    // Render History (All others)
    const histDiv = document.getElementById('tab-history');
    histDiv.innerHTML = pkgs.map(p => `
        <div class="pkg-item">#${p.id} ${p.dest} <span class="status-badge st-${p.status}">${getStatusName(p.status)}</span></div>
    `).join('');
}

async function updatePkg(id, action) {
    await fetch(`${API_URL}/update_pkg_status`, { method: 'POST', body: JSON.stringify({id, action}) });
    loadManagerPackages();
}

function getStatusName(s) {
    const names = ["Created", "Loaded", "Transit", "Arrived", "Delivered", "Failed"];
    return names[s] || "?";
}

function showTab(name) {
    document.querySelectorAll('.tab-content').forEach(d => d.classList.remove('active'));
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    document.getElementById(`tab-${name}`).classList.add('active');
    // Highlight button (simple logic omitted for brevity)
}

// --- Admin Simulation ---
async function nextShift() {
    const res = await fetch(`${API_URL}/next_shift`, { method: 'POST' });
    const data = await res.json();
    if(data.logs) alert(data.logs.join('\n'));
}

// --- Map Interaction ---
async function handleMouseDown(e) {
    const mouseX = e.offsetX; const mouseY = e.offsetY;
    
    // Check if clicking a node
    for(let n of nodes) {
        const dx = mouseX - n.displayX; const dy = mouseY - n.displayY;
        if (dx*dx + dy*dy < 400) {
            // ADMIN: Check packages at city
            if (userRole === 'admin') {
                checkCityPackages(n.name);
                // Also enable drag
                isDragging = true; draggedNode = n;
                return;
            }
        }
    }
}

async function checkCityPackages(city) {
    const res = await fetch(`${API_URL}/city_packages?city=${city}`);
    const data = await res.json();
    let msg = `Packages in ${city}:\n`;
    if(!data || data.length === 0) msg += "None.";
    else data.forEach(p => msg += `#${p.id} (to ${p.dest}) [${getStatusName(p.status)}]\n`);
    alert(msg);
}

function handleMouseMove(e) {
    if (!isDragging || !draggedNode || userRole !== 'admin') return; // Strict Check
    draggedNode.displayX = e.offsetX; draggedNode.displayY = e.offsetY;
    draggedNode.x = (draggedNode.displayX - transform.offsetX) / transform.scale + transform.minX;
    draggedNode.y = (draggedNode.displayY - transform.offsetY) / transform.scale + transform.minY;
    drawMap();
}

function handleMouseUp() {
    if (isDragging && draggedNode && userRole === 'admin') {
        saveNodePosition(draggedNode);
    }
    isDragging = false; draggedNode = null;
}

// ... (Keep existing loadMap, fitMapToScreen, drawMap, saveNodePosition, helper functions) ...
// Copy previous implementations for: loadMap, fitMapToScreen, drawMap, renderRouteManager, toggleBlock, addCity, addRoute, resetLayout
// Ensure drawMap() uses 'nodes' and 'edges' globals.
// ...
async function loadMap(force=false) {
    const res = await fetch(`${API_URL}/map`);
    const data = await res.json();
    nodes = data.nodes || []; edges = data.edges || [];
    fitMapToScreen();
    drawMap();
    if(userRole === 'admin') renderRouteManager(); // Only admin sees route list
}

function fitMapToScreen() { /* Same as before */ 
    if (nodes.length === 0) return;
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    nodes.forEach(n => { if(n.x<minX)minX=n.x; if(n.x>maxX)maxX=n.x; if(n.y<minY)minY=n.y; if(n.y>maxY)maxY=n.y; });
    const padding = 50, mapW = maxX-minX||1, mapH = maxY-minY||1;
    const cw = canvas.width-(padding*2), ch = canvas.height-(padding*2);
    const s = Math.min(cw/mapW, ch/mapH);
    transform = { scale: s, minX, minY, offsetX: padding+(cw-mapW*s)/2, offsetY: padding+(ch-mapH*s)/2 };
    nodes.forEach(n => { if(n!==draggedNode){ n.displayX=(n.x-minX)*s+transform.offsetX; n.displayY=(n.y-minY)*s+transform.offsetY; }});
}

function drawMap() { /* Same as before */ 
    ctx.clearRect(0,0,canvas.width,canvas.height);
    if(!isDragging) fitMapToScreen();
    edges.forEach(e => {
        const n1=nodes.find(n=>n.id===e.source), n2=nodes.find(n=>n.id===e.target);
        if(!n1||!n2)return;
        ctx.beginPath(); ctx.moveTo(n1.displayX,n1.displayY); ctx.lineTo(n2.displayX,n2.displayY);
        ctx.strokeStyle=e.blocked?'#e74c3c':'#95a5a6'; ctx.stroke();
    });
    nodes.forEach(n => {
        ctx.beginPath(); ctx.arc(n.displayX,n.displayY,18,0,2*Math.PI);
        ctx.fillStyle='#3498db'; ctx.fill(); ctx.stroke();
        ctx.fillStyle="#2c3e50"; ctx.fillText(n.name, n.displayX, n.displayY+24);
    });
}
function renderRouteManager() {
    const list = document.getElementById('route-list'); list.innerHTML = "";
    edges.forEach(e => {
       const n1=nodes.find(n=>n.id===e.source), n2=nodes.find(n=>n.id===e.target);
       if(n1&&n2) list.innerHTML+=`<div>${n1.name}-${n2.name} <button onclick="toggleBlock('${n1.name}-${n2.name}',${!e.blocked})">${e.blocked?'Unblock':'Block'}</button></div>`;
    });
}
async function toggleBlock(key, b) { await fetch(`${API_URL}/toggle_block`, {method:'POST', body:JSON.stringify({key, block:b})}); loadMap(); }
async function addCity() { await fetch(`${API_URL}/add_city`, {method:'POST', body:JSON.stringify({name:document.getElementById('new-city-name').value, password:document.getElementById('new-city-pass').value})}); loadMap(); }
async function addRoute() { await fetch(`${API_URL}/add_route`, {method:'POST', body:JSON.stringify({key:document.getElementById('route-key').value, distance:parseInt(document.getElementById('route-dist').value)})}); loadMap(); }
async function saveNodePosition(n) { await fetch(`${API_URL}/update_node`, {method:'POST', body:JSON.stringify({name:n.name, x:n.x, y:n.y})}); }
async function resetLayout() { await fetch(`${API_URL}/reset_layout`,{method:'POST'}); loadMap(); }
function logout() { location.reload(); }