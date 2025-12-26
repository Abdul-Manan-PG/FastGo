const API_URL = "http://localhost:8080/api";
let canvas, ctx;
let nodes = [];
let edges = [];
let highlightedPath = [];

window.onload = () => {
    canvas = document.getElementById('mapCanvas');
    ctx = canvas.getContext('2d');
    resizeCanvas();
    window.addEventListener('resize', () => { resizeCanvas(); drawMap(); });
    // Initial Load
    setTimeout(loginCheck, 100); 
};

function resizeCanvas() {
    if (canvas && canvas.parentElement) {
        canvas.width = canvas.parentElement.clientWidth;
        canvas.height = canvas.parentElement.clientHeight;
    }
}

// Check if we are already 'logged in' visually or need to show login
function loginCheck() {
    // Just a helper to keep flow simple
}

// --- CORE: SCALE & DRAW ---
function fitMapToScreen() {
    if (nodes.length === 0) return;

    // 1. Find Bounding Box of C++ Coordinates
    let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
    nodes.forEach(n => {
        if(n.x < minX) minX = n.x;
        if(n.x > maxX) maxX = n.x;
        if(n.y < minY) minY = n.y;
        if(n.y > maxY) maxY = n.y;
    });

    // 2. Calculate Scale Factors to fit in Canvas
    const padding = 50;
    const mapW = maxX - minX || 1;
    const mapH = maxY - minY || 1;
    const canvasW = canvas.width - (padding * 2);
    const canvasH = canvas.height - (padding * 2);

    const scale = Math.min(canvasW / mapW, canvasH / mapH);

    // 3. Apply Scale & Center
    nodes.forEach(n => {
        n.displayX = (n.x - minX) * scale + padding + (canvasW - mapW * scale) / 2;
        n.displayY = (n.y - minY) * scale + padding + (canvasH - mapH * scale) / 2;
    });
}

// --- API CALLS ---
async function loadMap(showLoading = false) {
    if(showLoading) document.getElementById('loading-overlay').style.display = 'flex';

    try {
        const res = await fetch(`${API_URL}/map`);
        const data = await res.json();
        
        if (data.nodes) nodes = data.nodes;
        if (data.edges) edges = data.edges;

        // FIT MAP INSTANTLY
        fitMapToScreen();
        renderRouteManager();
        drawMap();

    } catch (e) {
        console.error("Load Error", e);
    } finally {
        if(showLoading) document.getElementById('loading-overlay').style.display = 'none';
    }
}

// --- RENDERER (No Loop, Just Draw Once) ---
function drawMap() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    fitMapToScreen(); // Re-fit in case of resize

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