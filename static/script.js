const API_URL = "http://localhost:8080/api";
let canvas, ctx;
let nodes = [];
let edges = [];
let highlightedPath = [];

// Drag & Drop State
let isDragging = false;
let draggedNode = null;
let transform = { scale: 1, minX: 0, minY: 0, offsetX: 0, offsetY: 0 }; // Stores map scaling info

window.onload = () => {
    canvas = document.getElementById('mapCanvas');
    ctx = canvas.getContext('2d');
    
    // Add Event Listeners for Dragging
    canvas.addEventListener('mousedown', handleMouseDown);
    canvas.addEventListener('mousemove', handleMouseMove);
    canvas.addEventListener('mouseup', handleMouseUp);
    
    resizeCanvas();
    window.addEventListener('resize', () => { resizeCanvas(); drawMap(); });
    setTimeout(loginCheck, 100); 
};

function resizeCanvas() {
    if (canvas && canvas.parentElement) {
        canvas.width = canvas.parentElement.clientWidth;
        canvas.height = canvas.parentElement.clientHeight;
    }
}

// --- CORE: SCALE & DRAW ---
function fitMapToScreen() {
    if (nodes.length === 0) return;

    // 1. Calculate Bounds
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    nodes.forEach(n => {
        if(n.x < minX) minX = n.x;
        if(n.x > maxX) maxX = n.x;
        if(n.y < minY) minY = n.y;
        if(n.y > maxY) maxY = n.y;
    });

    // 2. Calculate Scale
    const padding = 50;
    const mapW = maxX - minX || 1;
    const mapH = maxY - minY || 1;
    const canvasW = canvas.width - (padding * 2);
    const canvasH = canvas.height - (padding * 2);
    const scale = Math.min(canvasW / mapW, canvasH / mapH);

    // 3. Save Transform Data (Needed for Reverse Math)
    transform = {
        scale: scale,
        minX: minX,
        minY: minY,
        offsetX: padding + (canvasW - mapW * scale) / 2,
        offsetY: padding + (canvasH - mapH * scale) / 2
    };

    // 4. Apply to Nodes (Only if NOT dragging, to prevent jitter)
    nodes.forEach(n => {
        // We only recalculate display coords if we aren't currently moving this specific node manually
        if (n !== draggedNode) {
            n.displayX = (n.x - transform.minX) * transform.scale + transform.offsetX;
            n.displayY = (n.y - transform.minY) * transform.scale + transform.offsetY;
        }
    });
}

// --- DRAG EVENTS ---

function handleMouseDown(e) {
    const mouseX = e.offsetX;
    const mouseY = e.offsetY;

    // Check if clicked inside a node (Radius approx 20px)
    nodes.forEach(n => {
        const dx = mouseX - n.displayX;
        const dy = mouseY - n.displayY;
        if (dx*dx + dy*dy < 400) { // 20^2 = 400
            isDragging = true;
            draggedNode = n;
        }
    });
}

function handleMouseMove(e) {
    if (!isDragging || !draggedNode) return;

    // 1. Update visual position immediately
    draggedNode.displayX = e.offsetX;
    draggedNode.displayY = e.offsetY;

    // 2. Reverse Math: Calculate new World X/Y
    // Formula: World = (Screen - Offset) / Scale + Min
    draggedNode.x = (draggedNode.displayX - transform.offsetX) / transform.scale + transform.minX;
    draggedNode.y = (draggedNode.displayY - transform.offsetY) / transform.scale + transform.minY;

    // 3. Redraw
    drawMap(); 
}

function handleMouseUp(e) {
    if (isDragging && draggedNode) {
        // Save final position to backend
        saveNodePosition(draggedNode);
    }
    isDragging = false;
    draggedNode = null;
}

async function saveNodePosition(node) {
    await fetch(`${API_URL}/update_node`, {
        method: 'POST',
        body: JSON.stringify({ name: node.name, x: node.x, y: node.y })
    });
}

// Check if we are already 'logged in' visually or need to show login
function loginCheck() {
    // Just a helper to keep flow simple
}

// --- API CALLS ---
async function loadMap(showLoading = false) {
    if(showLoading) document.getElementById('loading-overlay').style.display = 'flex';
    try {
        const res = await fetch(`${API_URL}/map`);
        const data = await res.json();
        if (data.nodes) nodes = data.nodes;
        if (data.edges) edges = data.edges;
        fitMapToScreen();
        renderRouteManager();
        drawMap();
    } catch (e) { console.error(e); } 
    finally { if(showLoading) document.getElementById('loading-overlay').style.display = 'none'; }
}

// --- RENDERER (No Loop, Just Draw Once) ---
function drawMap() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // Only fit if NOT dragging (prevents map from resizing while you move a node)
    if (!isDragging) fitMapToScreen();

    // Draw Edges
    edges.forEach(e => {
        const n1 = nodes.find(n => n.id === e.source);
        const n2 = nodes.find(n => n.id === e.target);
        if(!n1 || !n2) return;

        ctx.beginPath();
        ctx.moveTo(n1.displayX, n1.displayY);
        ctx.lineTo(n2.displayX, n2.displayY);
        ctx.strokeStyle = e.blocked ? '#e74c3c' : '#95a5a6';
        ctx.lineWidth = 2;
        ctx.stroke();

        // Distance Label
        const mx = (n1.displayX + n2.displayX)/2;
        const my = (n1.displayY + n2.displayY)/2;
        ctx.fillStyle = "white"; ctx.fillRect(mx-15, my-10, 30, 20);
        ctx.fillStyle = e.blocked ? '#c0392b' : '#7f8c8d';
        ctx.font = "bold 11px Arial"; ctx.textAlign = "center"; ctx.textBaseline = "middle";
        ctx.fillText(e.weight, mx, my);
    });

    // Draw Path
    if (highlightedPath.length > 1) {
        ctx.beginPath(); ctx.strokeStyle = '#3498db'; ctx.lineWidth = 5; ctx.lineCap = 'round';
        for(let i=0; i<highlightedPath.length-1; i++) {
            const n1 = nodes.find(n => n.name === highlightedPath[i]);
            const n2 = nodes.find(n => n.name === highlightedPath[i+1]);
            if(n1 && n2) { ctx.moveTo(n1.displayX, n1.displayY); ctx.lineTo(n2.displayX, n2.displayY); }
        }
        ctx.stroke();
    }

    // Draw Nodes
    nodes.forEach(n => {
        ctx.beginPath(); ctx.arc(n.displayX, n.displayY, 18, 0, 2*Math.PI);
        ctx.fillStyle = '#3498db'; ctx.fill(); 
        ctx.strokeStyle = 'white'; ctx.lineWidth = 3; ctx.stroke();
        
        ctx.fillStyle = "#2c3e50"; ctx.font = "bold 13px Segoe UI"; 
        ctx.textAlign = "center"; ctx.textBaseline = "top";
        ctx.fillText(n.name, n.displayX, n.displayY + 24);
    });
}

// --- UI HELPERS ---
function renderRouteManager() {
    const list = document.getElementById('route-list');
    if(!list) return;
    list.innerHTML = "";
    const sorted = [...edges].sort((a, b) => a.weight - b.weight);
    if (sorted.length === 0) { list.innerHTML = "<p class='hint'>No routes.</p>"; return; }

    sorted.forEach(edge => {
        const n1 = nodes.find(n => n.id === edge.source);
        const n2 = nodes.find(n => n.id === edge.target);
        if (!n1 || !n2) return;

        const div = document.createElement('div');
        div.className = 'route-item';
        div.innerHTML = `
            <div><span class="route-name">${n1.name} - ${n2.name}</span><span class="route-dist">${edge.weight} km</span></div>
            <button class="toggle-btn ${edge.blocked ? 'is-blocked' : 'is-open'}"
                onclick="toggleBlock('${n1.name}-${n2.name}', ${!edge.blocked})">${edge.blocked ? "Unblock" : "Block"}</button>`;
        list.appendChild(div);
    });
}

// --- ACTIONS ---
async function login() {
    const u = document.getElementById('username').value;
    const p = document.getElementById('password').value;
    const res = await fetch(`${API_URL}/login`, { method: 'POST', body: JSON.stringify({ username: u, password: p }) });
    const data = await res.json();
    if (data.status === 'success') {
        document.getElementById('login-screen').classList.remove('active');
        document.getElementById('dashboard-screen').classList.add('active');
        document.getElementById('role-display').innerText = `(${data.role})`;
        resizeCanvas();
        if (data.role !== 'admin') document.getElementById('admin-sidebar').style.display = 'none';
        loadMap(true);
    } else { document.getElementById('login-msg').innerText = data.message; }
}

async function addCity() {
    const n = document.getElementById('new-city-name').value;
    const p = document.getElementById('new-city-pass').value;
    await fetch(`${API_URL}/add_city`, { method: 'POST', body: JSON.stringify({ name: n, password: p }) });
    loadMap(true);
}

async function addRoute() {
    const k = document.getElementById('route-key').value;
    const d = document.getElementById('route-dist').value;
    await fetch(`${API_URL}/add_route`, { method: 'POST', body: JSON.stringify({ key: k, distance: parseInt(d) }) });
    loadMap(true);
}

async function toggleBlock(key, block) {
    const res = await fetch(`${API_URL}/toggle_block`, { method: 'POST', body: JSON.stringify({ key, block }) });
    const d = await res.json();
    if(d.message.includes("Not Found")) {
        const revKey = key.split('-').reverse().join('-');
        await fetch(`${API_URL}/toggle_block`, { method: 'POST', body: JSON.stringify({ key: revKey, block }) });
    }
    loadMap(false);
}

async function findPath() {
    const s = document.getElementById('start-city').value;
    const e = document.getElementById('end-city').value;
    const res = await fetch(`${API_URL}/path?start=${s}&end=${e}`);
    const d = await res.json();
    if(d.found) { highlightedPath = d.path; document.getElementById('path-result').innerText = `${d.distance} km`; drawMap(); }
    else { highlightedPath = []; document.getElementById('path-result').innerText = "No Path"; drawMap(); }
}

function logout() { location.reload(); }


