const API_URL = "http://localhost:8080/api";
let canvas, ctx;
let nodes = [];
let edges = [];
let highlightedPath = [];

// --- INITIALIZATION ---
window.onload = () => {
    canvas = document.getElementById('mapCanvas');
    ctx = canvas.getContext('2d');
    
    // Initial resize (might be 0 if hidden, but good practice)
    resizeCanvas();
    window.addEventListener('resize', resizeCanvas);
    
    // Start drawing loop
    requestAnimationFrame(drawLoop);
};

function resizeCanvas() {
    if (canvas && canvas.parentElement) {
        // Make canvas pixel count equal to its display size
        canvas.width = canvas.parentElement.clientWidth;
        canvas.height = canvas.parentElement.clientHeight;
    }
}

// --- LOGIC: RECALCULATE POSITIONS ---
function calculateNodePositions() {
    // Safety check: if canvas has no size, don't try to calculate positions
    if (canvas.width === 0 || canvas.height === 0) return;

    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;
    
    // Dynamic Radius: Fits the map inside the window
    const radius = Math.min(canvas.width, canvas.height) * 0.35;
    
    if (nodes.length === 0) return;

    const angleStep = (2 * Math.PI) / nodes.length;

    nodes.forEach((node, index) => {
        node.displayX = centerX + radius * Math.cos(index * angleStep);
        node.displayY = centerY + radius * Math.sin(index * angleStep);
    });
}

// --- API CALLS ---
async function login() {
    const user = document.getElementById('username').value;
    const pass = document.getElementById('password').value;

    const res = await fetch(`${API_URL}/login`, {
        method: 'POST', body: JSON.stringify({ username: user, password: pass })
    });
    const data = await res.json();

    if (data.status === 'success') {
        document.getElementById('login-screen').classList.remove('active');
        document.getElementById('dashboard-screen').classList.add('active');
        document.getElementById('role-display').innerText = `(${data.role})`;
        
        // --- CRITICAL FIX: Resize Map NOW that it is visible ---
        resizeCanvas(); 
        // ------------------------------------------------------

        if (data.role !== 'admin') {
            document.getElementById('admin-sidebar').style.display = 'none';
        }
        loadMap();
    } else {
        document.getElementById('login-msg').innerText = data.message;
    }
}

async function toggleBlock(shouldBlock) {
    const key = document.getElementById('block-key').value;
    if (!key) { alert("Please enter Route Key"); return; }

    const res = await fetch(`${API_URL}/toggle_block`, {
        method: 'POST', body: JSON.stringify({ key: key, block: shouldBlock })
    });
    const data = await res.json();
    loadMap();
    alert(data.message);
}

async function loadMap() {
    const res = await fetch(`${API_URL}/map`);
    const data = await res.json();
    if (data.nodes) nodes = data.nodes;
    if (data.edges) edges = data.edges;
}

async function addCity() {
    const name = document.getElementById('new-city-name').value;
    const pass = document.getElementById('new-city-pass').value;
    await fetch(`${API_URL}/add_city`, {
        method: 'POST', body: JSON.stringify({ name, password: pass })
    });
    loadMap();
    alert("City Added!");
}

async function addRoute() {
    const key = document.getElementById('route-key').value;
    const dist = parseInt(document.getElementById('route-dist').value);
    await fetch(`${API_URL}/add_route`, {
        method: 'POST', body: JSON.stringify({ key, distance: dist })
    });
    loadMap();
    alert("Route Added!");
}

async function findPath() {
    const start = document.getElementById('start-city').value;
    const end = document.getElementById('end-city').value;

    const res = await fetch(`${API_URL}/path?start=${start}&end=${end}`);
    const data = await res.json();

    if (data.found) {
        highlightedPath = data.path;
        document.getElementById('path-result').innerText = `Distance: ${data.distance} km`;
    } else {
        highlightedPath = [];
        document.getElementById('path-result').innerText = "No Path Found";
    }
}

function logout() { location.reload(); }

// --- DRAWING LOGIC ---
function drawLoop() {
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    
    // 1. Recalculate positions based on CURRENT window size
    calculateNodePositions();

    // 2. Draw Edges
    edges.forEach(edge => {
        const n1 = nodes.find(n => n.id === edge.source);
        const n2 = nodes.find(n => n.id === edge.target);
        if (!n1 || !n2 || n1.displayX === undefined) return;

        ctx.beginPath();
        ctx.moveTo(n1.displayX, n1.displayY);
        ctx.lineTo(n2.displayX, n2.displayY);
        
        ctx.strokeStyle = edge.blocked ? '#e74c3c' : '#bdc3c7'; // Red if blocked, Grey if open
        ctx.lineWidth = 2;
        ctx.stroke();

        // Draw Weight Text
        const midX = (n1.displayX + n2.displayX) / 2;
        const midY = (n1.displayY + n2.displayY) / 2;
        ctx.fillStyle = "white";
        ctx.fillRect(midX - 10, midY - 8, 20, 16); // Text Background
        ctx.fillStyle = edge.blocked ? '#c0392b' : '#7f8c8d';
        ctx.font = "bold 12px Arial";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText(edge.weight, midX, midY);
    });

    // 3. Draw Shortest Path (Blue)
    if (highlightedPath.length > 1) {
        ctx.beginPath();
        ctx.strokeStyle = '#3498db'; // BLUE
        ctx.lineWidth = 4;
        ctx.lineCap = "round";
        
        for (let i = 0; i < highlightedPath.length - 1; i++) {
            const n1 = nodes.find(n => n.name === highlightedPath[i]);
            const n2 = nodes.find(n => n.name === highlightedPath[i+1]);
            if (n1 && n2) {
                ctx.moveTo(n1.displayX, n1.displayY);
                ctx.lineTo(n2.displayX, n2.displayY);
            }
        }
        ctx.stroke();
    }

    // 4. Draw Nodes
    nodes.forEach(node => {
        if (node.displayX === undefined) return;

        ctx.beginPath();
        ctx.arc(node.displayX, node.displayY, 20, 0, 2 * Math.PI);
        ctx.fillStyle = '#3498db';
        ctx.fill();
        ctx.strokeStyle = 'white';
        ctx.lineWidth = 3;
        ctx.stroke();

        ctx.fillStyle = "#2c3e50";
        ctx.font = "bold 14px Segoe UI";
        ctx.textAlign = "center";
        ctx.textBaseline = "top";
        ctx.fillText(node.name, node.displayX, node.displayY + 25);
    });

    requestAnimationFrame(drawLoop);
}