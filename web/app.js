/**
 * Custom Memory Allocator - Web GUI JavaScript
 * Provides real-time heap visualization and management
 */

// Configuration
const CONFIG = {
    // Force the API to use the same host that served the page
    API_BASE: window.location.origin, 
    DEFAULT_REFRESH_RATE: 5000,
    ALERT_DURATION: 5000
};

// State
let state = {
    refreshInterval: null,
    autoRefresh: true,
    refreshRate: CONFIG.DEFAULT_REFRESH_RATE,
    isConnected: false,
    heapData: null,
    allocations: new Map()
};

/**
 * Initialize application
 */
function init() {
    setupEventListeners();
    const apiEndpoint = document.getElementById('apiEndpoint');
    if (apiEndpoint) {
        apiEndpoint.textContent = CONFIG.API_BASE;
    }
    updateStatus(false);
    refreshHeapData();
    startAutoRefresh();
}

/**
 * Setup event listeners
 */
function setupEventListeners() {
    // Allocation
    document.getElementById('mallocBtn').addEventListener('click', handleAlloc);
    document.getElementById('allocateSize').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') handleAlloc();
    });

    // Deallocation
    document.getElementById('freeBtn').addEventListener('click', handleFree);
    document.getElementById('freeAddress').addEventListener('keypress', (e) => {
        if (e.key === 'Enter') handleFree();
    });

    // Management
    document.getElementById('resetBtn').addEventListener('click', handleReset);
    document.getElementById('verifyBtn').addEventListener('click', handleVerify);

    // Auto-refresh
    document.getElementById('autoRefresh').addEventListener('change', (e) => {
        state.autoRefresh = e.target.checked;
        if (state.autoRefresh) {
            startAutoRefresh();
        } else if (state.refreshInterval) {
            clearInterval(state.refreshInterval);
        }
    });

    // Refresh rate
    document.getElementById('refreshRate').addEventListener('change', (e) => {
        state.refreshRate = parseInt(e.target.value);
        if (state.autoRefresh) {
            clearInterval(state.refreshInterval);
            startAutoRefresh();
        }
    });

    // Manual refresh
    document.addEventListener('keydown', (e) => {
        if (e.ctrlKey && e.key === 'r') {
            e.preventDefault();
            refreshHeapData();
        }
    });
}

/**
 * Handle memory allocation
 */
async function handleAlloc() {
    const sizeInput = document.getElementById('allocateSize');
    const size = parseInt(sizeInput.value);

    if (!size || size <= 0) {
        showAlert('Invalid size', 'error');
        return;
    }

    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/malloc`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ size })
        });

        const data = await response.json();

        if (data.success) {
            state.allocations.set(data.address, size);
            showAlert(`Allocated ${size} bytes at ${data.address}`, 'success');
            sizeInput.value = '';
            await refreshHeapData();
        } else {
            showAlert(`Allocation failed: ${data.error}`, 'error');
        }
    } catch (error) {
        console.error('Allocation error:', error);
        showAlert('Failed to allocate memory', 'error');
    }
}

/**
 * Handle memory deallocation
 */
async function handleFree() {
    const addressInput = document.getElementById('freeAddress');
    const address = addressInput.value.trim();

    if (!address) {
        showAlert('Enter a valid address', 'error');
        return;
    }

    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/free`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ address })
        });

        const data = await response.json();

        if (data.success) {
            state.allocations.delete(address);
            showAlert(`Freed memory at ${address}`, 'success');
            addressInput.value = '';
            await refreshHeapData();
        } else {
            showAlert('Failed to free memory', 'error');
        }
    } catch (error) {
        console.error('Free error:', error);
        showAlert('Failed to free memory', 'error');
    }
}

/**
 * Handle heap reset
 */
async function handleReset() {
    if (!confirm('Are you sure you want to reset the entire heap?')) {
        return;
    }

    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/reset`, {
            method: 'POST'
        });

        const data = await response.json();

        if (data.success) {
            state.allocations.clear();
            showAlert('Heap reset successfully', 'success');
            await refreshHeapData();
        } else {
            showAlert('Failed to reset heap', 'error');
        }
    } catch (error) {
        console.error('Reset error:', error);
        showAlert('Failed to reset heap', 'error');
    }
}

/**
 * Handle heap verification
 */
async function handleVerify() {
    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/verify`, {
            method: 'POST'
        });

        const data = await response.json();

        if (data.valid) {
            showAlert('✓ Heap integrity verified', 'success');
        } else {
            showAlert('✗ Heap integrity check failed', 'error');
        }
    } catch (error) {
        console.error('Verify error:', error);
        showAlert('Failed to verify heap', 'error');
    }
}

/**
 * Refresh heap data from API
 */
async function refreshHeapData() {
    try {
        const response = await fetch(`${CONFIG.API_BASE}/api/status`);
        const data = await response.json();

        state.heapData = data;
        updateStatus(true);
        updateDashboard(data);
        updateVisualization(data);
        updateBlocksTable(data);

    } catch (error) {
        console.error('Fetch error:', error);
        updateStatus(false);
        showAlert('Failed to connect to server', 'error');
    }
}

/**
 * Start auto-refresh interval
 */
function startAutoRefresh() {
    if (state.refreshInterval) {
        clearInterval(state.refreshInterval);
    }

    state.refreshInterval = setInterval(() => {
        refreshHeapData();
    }, state.refreshRate);
}

/**
 * Update connection status
 */
function updateStatus(connected) {
    state.isConnected = connected;
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');

    if (connected) {
        statusDot.classList.add('connected');
        statusText.textContent = 'Connected';
    } else {
        statusDot.classList.remove('connected');
        statusText.textContent = 'Disconnected';
    }
}

/**
 * Update dashboard statistics
 */
function updateDashboard(data) {
    const heap = data.heap_status;

    // Local helper for byte formatting
    const formatBytes = (bytes) => {
        if (bytes === 0) return '0 B';
        const k = 1024;
        const sizes = ['B', 'KB', 'MB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    };

    // Update basic stats
    document.getElementById('totalSize').textContent = formatBytes(heap.total_size);
    document.getElementById('allocatedSize').textContent = formatBytes(heap.allocated);
    document.getElementById('freeSize').textContent = formatBytes(heap.free);

    // Calculate Utilization
    const utilization = ((heap.allocated / heap.total_size) * 100).toFixed(1);
    document.getElementById('utilization').textContent = utilization + '%';

    document.getElementById('numAllocations').textContent = heap.num_allocations;
    document.getElementById('numBlocks').textContent = heap.num_blocks;
    document.getElementById('failedAllocations').textContent = heap.failed_allocations;

    const fragValue = heap.fragmentation.toString().replace('%', '');
    document.getElementById('fragmentation').textContent = fragValue + '%';
}

/**
 * Update heap visualization
 */
function updateVisualization(data) {
    const container = document.getElementById('heapVisualization');
    container.innerHTML = '';

    const heap = data.heap_status;
    const totalSize = heap.total_size;

    data.blocks.forEach(block => {
        const percentage = ((block.size / totalSize) * 100).toFixed(2);

        const blockEl = document.createElement('div');
        blockEl.className = `memory-block ${block.is_free ? 'free' : 'allocated'}`;

        const barWidth = Math.max(percentage * 2, 1); // Scale for visibility
        blockEl.innerHTML = `
            <div class="memory-block-bar" style="width: ${barWidth}px;"></div>
            <div class="memory-block-info">
                <div class="memory-block-address">${block.address}</div>
                <div class="memory-block-size">${block.size} bytes (${percentage}%)</div>
            </div>
        `;

        container.appendChild(blockEl);
    });
}

/**
 * Update blocks table
 */
function updateBlocksTable(data) {
    const tbody = document.getElementById('blocksList');
    if (!tbody) return; // Safety check
    
    tbody.innerHTML = '';

    // Log the data to see what's actually arriving in the browser
    console.log("Updating table with blocks:", data.blocks);

    if (!data.blocks || !Array.isArray(data.blocks)) {
        console.error("Invalid blocks data received");
        return;
    }

    data.blocks.forEach((block) => {
        // Fallback for missing data to prevent loop crashes
        const addr = block.address || '0x0';
        const size = block.size || 0;
        const isFree = block.is_free;
        const totalSize = data.heap_status.total_size || 1048576;
        
        const percentage = ((size / totalSize) * 100).toFixed(2);

        const row = document.createElement('tr');
        row.innerHTML = `
            <td><code>${addr}</code></td>
            <td>${size} bytes</td>
            <td>
                <span class="block-status ${isFree ? 'free' : 'allocated'}">
                    ${isFree ? 'FREE' : 'ALLOCATED'}
                </span>
            </td>
            <td>${percentage}%</td>
        `;

        tbody.appendChild(row);
    });
}

/**
 * Show alert message
 */
function showAlert(message, type = 'info') {
    const container = document.getElementById('alertContainer');

    const alert = document.createElement('div');
    alert.className = `alert alert-${type}`;
    alert.innerHTML = `
        <span>${message}</span>
        <span class="alert-close">&times;</span>
    `;

    alert.querySelector('.alert-close').addEventListener('click', () => {
        alert.remove();
    });

    container.appendChild(alert);

    // Auto-remove after duration
    setTimeout(() => {
        alert.remove();
    }, CONFIG.ALERT_DURATION);
}

/**
 * Utility: Format bytes
 */
function formatBytes(bytes) {
    if (bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

/**
 * Start application when DOM is ready
 */
document.addEventListener('DOMContentLoaded', init);

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    // Ctrl+R = Refresh
    if (e.ctrlKey && e.key === 'r') {
        e.preventDefault();
        refreshHeapData();
    }
    // Ctrl+A = Focus allocate input
    if (e.ctrlKey && e.key === 'a') {
        e.preventDefault();
        document.getElementById('allocateSize').focus();
    }
});
