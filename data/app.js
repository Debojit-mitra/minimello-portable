/* =============================================================
   MiniMello WebUI — Application Logic (Redesigned)
   ============================================================= */

const API = '';  // Same origin

// --- Tab Navigation ---
document.querySelectorAll('.tab').forEach(tab => {
    tab.addEventListener('click', () => {
        document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
        document.querySelectorAll('.tab-content').forEach(c => c.classList.remove('active'));
        tab.classList.add('active');
        document.getElementById('tab-' + tab.dataset.tab).classList.add('active');
    });
});

// --- UI Helpers ---
let toastTimeout;
function showToast(message, type = 'success') {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.className = `toast show ${type}`;
    clearTimeout(toastTimeout);
    toastTimeout = setTimeout(() => {
        toast.classList.remove('show');
    }, 3000);
}

function showConfirm(title, message, onConfirm) {
    document.getElementById('modal-title').textContent = title;
    document.getElementById('modal-message').textContent = message;
    const overlay = document.getElementById('modal-overlay');
    const btnCancel = document.getElementById('modal-btn-cancel');
    const btnConfirm = document.getElementById('modal-btn-confirm');
    
    // Clean up previous listeners
    const newCancel = btnCancel.cloneNode(true);
    const newConfirm = btnConfirm.cloneNode(true);
    btnCancel.parentNode.replaceChild(newCancel, btnCancel);
    btnConfirm.parentNode.replaceChild(newConfirm, btnConfirm);
    
    newCancel.addEventListener('click', () => overlay.classList.add('hidden'));
    newConfirm.addEventListener('click', () => {
        overlay.classList.add('hidden');
        onConfirm();
    });
    
    overlay.classList.remove('hidden');
}

// --- API Helpers ---
async function apiGet(path) {
    try {
        const res = await fetch(API + path);
        return await res.json();
    } catch (e) {
        console.error('API GET error:', path, e);
        return null;
    }
}

async function apiPost(path, data = null) {
    try {
        const opts = { method: 'POST' };
        if (data) {
            opts.headers = { 'Content-Type': 'application/json' };
            opts.body = JSON.stringify(data);
        }
        const res = await fetch(API + path, opts);
        return await res.json();
    } catch (e) {
        console.error('API POST error:', path, e);
        return null;
    }
}

// --- Status Polling ---
let updateNotified = false;

async function refreshStatus() {
    const s = await apiGet('/api/status');
    if (!s) return;

    // Header badges
    const wifiBadge = document.getElementById('wifi-badge');
    wifiBadge.textContent = s.wifi_connected ? 'WiFi ✓' : 'WiFi ✗';
    wifiBadge.className = 'badge ' + (s.wifi_connected ? '' : 'badge-off');

    document.getElementById('version-badge').textContent = 'v' + s.version;

    // Active face
    const activeEl = document.getElementById('active-engine');
    if (s.active_engine === 'emotion') {
        activeEl.textContent = 'Emotion · ' + s.emotion;
    } else {
        activeEl.textContent = 'Clock · ' + s.clock_face;
    }

    // WiFi tab status
    const wifiDot = document.getElementById('wifi-dot');
    const wifiDetail = document.getElementById('wifi-status-detail');
    if (s.wifi_connected) {
        wifiDot.classList.add('connected');
        wifiDetail.innerHTML = '<strong>Connected</strong> · ' + s.ip + ' · ' + s.wifi_rssi + ' dBm';
    } else {
        wifiDot.classList.remove('connected');
        wifiDetail.textContent = 'Not connected';
    }

    // System info
    document.getElementById('info-version').textContent = s.version;
    document.getElementById('info-wifi').textContent =
        s.wifi_connected ? ('Connected (' + s.wifi_rssi + ' dBm)') : 'Disconnected';
    document.getElementById('info-ip').textContent = s.ip;
    document.getElementById('info-uptime').textContent = formatUptime(s.uptime_s);
    document.getElementById('info-heap').textContent = (s.free_heap / 1024).toFixed(1) + ' KB';

    // Battery (conditional)
    const battRow = document.getElementById('battery-row');
    if (s.battery_enabled) {
        battRow.classList.remove('hidden');
        document.getElementById('info-battery').textContent =
            s.battery_percent + '% (' + s.battery_voltage.toFixed(2) + 'V)';
    } else {
        battRow.classList.add('hidden');
    }

    // Highlight active emotion cards
    document.querySelectorAll('.emotion-card').forEach(c => {
        c.classList.toggle('active', c.dataset.emotion == getEmotionIndex(s.emotion));
    });
    document.querySelectorAll('.face-card').forEach(c => {
        c.classList.toggle('active', c.dataset.face == getFaceIndex(s.clock_face));
    });

    // OTA Automatic notification
    if (s.has_update) {
        document.getElementById('btn-ota-update').disabled = false;
        document.getElementById('ota-status').innerHTML = `<strong>Update Available: v${s.latest_version}</strong><br>Ready to install.`;
        if (!updateNotified) {
            showToast(`New Firmware Available: v${s.latest_version}`, 'success');
            updateNotified = true;
        }
    }

    // Lock tabs if wifi not connected
    document.querySelectorAll('.tab').forEach(tab => {
        const isCriticalTab = (tab.dataset.tab === 'wifi' || tab.dataset.tab === 'system');
        if (!isCriticalTab) {
            tab.disabled = !s.wifi_connected;
            if (!s.wifi_connected) {
                tab.style.display = 'none'; // Completely hide non-critical tabs
            } else {
                tab.style.display = ''; // Restore visibility
            }
        }
    });

    // Force redirect to WiFi tab if not connected and on a hidden tab
    if (!s.wifi_connected) {
        const activeTab = document.querySelector('.tab.active');
        if (activeTab && activeTab.dataset.tab !== 'wifi' && activeTab.dataset.tab !== 'system') {
            document.querySelector('.tab[data-tab="wifi"]').click();
        }
    }
}

function formatUptime(s) {
    const d = Math.floor(s / 86400);
    const h = Math.floor((s % 86400) / 3600);
    const m = Math.floor((s % 3600) / 60);
    if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
    if (h > 0) return h + 'h ' + m + 'm';
    return m + 'm ' + (s % 60) + 's';
}

function getEmotionIndex(name) {
    const map = { 'Neutral': 0, 'Happy': 1, 'Sad': 2, 'Angry': 3,
                  'Surprised': 4, 'Sleepy': 5, 'Love': 6, 'Wink': 7 };
    return map[name] ?? -1;
}

function getFaceIndex(name) {
    const map = { 'Digital': 0, 'Typographic Word': 1, 'Minimal': 2 };
    return map[name] ?? -1;
}

// --- Load Config ---
async function refreshConfig() {
    const c = await apiGet('/api/config');
    if (!c) return;

    document.getElementById('wifi-ssid').value = c.wifi_ssid || '';
    document.getElementById('auto-switch').checked = c.auto_switch;
    document.getElementById('switch-interval').value = c.switch_interval;
    document.getElementById('clock-duration').value = c.clock_duration;
    document.getElementById('default-engine').value = c.default_engine;

    const brightPct = Math.round(c.brightness * 100 / 255);
    document.getElementById('brightness').value = brightPct;
    document.getElementById('brightness-val').textContent = brightPct + '%';

    // Timezone: find matching option by value (seconds)
    const tzSelect = document.getElementById('tz-offset');
    const tzVal = String(c.tz_offset);
    let found = false;
    for (const opt of tzSelect.options) {
        if (opt.value === tzVal) {
            opt.selected = true;
            found = true;
            break;
        }
    }
    // If exact match not found, select closest
    if (!found) {
        let closest = null;
        let minDiff = Infinity;
        for (const opt of tzSelect.options) {
            const diff = Math.abs(parseInt(opt.value) - c.tz_offset);
            if (diff < minDiff) {
                minDiff = diff;
                closest = opt;
            }
        }
        if (closest) closest.selected = true;
    }

    document.getElementById('night-enabled').checked = c.night_enabled;
    document.getElementById('night-start').value = c.night_start;
    document.getElementById('night-end').value = c.night_end;
    document.getElementById('weather-city').value = c.weather_city || '';
    document.getElementById('weather-key').value = c.weather_key_set ? '••••••••' : '';

    if (c.mood_interval !== undefined) {
        document.getElementById('mood-interval').value = c.mood_interval;
        document.getElementById('mood-interval-val').textContent = c.mood_interval + 's';
    }
}

// --- Emotion Cards ---
document.querySelectorAll('.emotion-card').forEach(card => {
    card.addEventListener('click', async () => {
        await apiPost('/api/emotion', { emotion: parseInt(card.dataset.emotion) });
        setTimeout(refreshStatus, 300);
    });
});

// --- Clock Face Cards ---
document.querySelectorAll('.face-card').forEach(card => {
    card.addEventListener('click', async () => {
        await apiPost('/api/clock/face', { face: parseInt(card.dataset.face) });
        setTimeout(refreshStatus, 300);
    });
});

// --- Switch Face ---
document.getElementById('btn-switch').addEventListener('click', async () => {
    await apiPost('/api/switch');
    setTimeout(refreshStatus, 300);
});

// --- WiFi ---
document.getElementById('btn-scan').addEventListener('click', async () => {
    const listEl = document.getElementById('wifi-networks');
    listEl.innerHTML = '<p class="muted">Scanning networks...</p>';

    await apiGet('/api/wifi/scan');

    setTimeout(async () => {
        const data = await apiGet('/api/wifi/scan');
        if (!data || !data.networks || data.networks.length === 0) {
            listEl.innerHTML = '<p class="muted">No networks found</p>';
            return;
        }
        listEl.innerHTML = '';
        data.networks.forEach(net => {
            const item = document.createElement('div');
            item.className = 'network-item';
            const signal = net.rssi > -50 ? '▰▰▰▰' : net.rssi > -65 ? '▰▰▰▱' :
                           net.rssi > -75 ? '▰▰▱▱' : '▰▱▱▱';
            item.innerHTML = '<span>' + net.ssid + (net.secure ? ' 🔒' : '') + '</span>' +
                             '<span class="signal">' + signal + ' ' + net.rssi + 'dBm</span>';
            item.addEventListener('click', () => {
                document.getElementById('wifi-ssid').value = net.ssid;
                document.getElementById('wifi-pass').focus();
            });
            listEl.appendChild(item);
        });
    }, 3000);
});

document.getElementById('btn-wifi-save').addEventListener('click', async () => {
    const ssid = document.getElementById('wifi-ssid').value;
    const pass = document.getElementById('wifi-pass').value;
    if (!ssid) return;

    const wifiDetail = document.getElementById('wifi-status-detail');
    wifiDetail.innerHTML = 'Connecting to <strong>' + ssid + '</strong>...';

    await apiPost('/api/wifi', { ssid, pass });

    setTimeout(refreshStatus, 5000);
});

// --- Password Toggle ---
function setupPasswordToggle(inputId, toggleId) {
    const toggle = document.getElementById(toggleId);
    if (!toggle) return;
    toggle.addEventListener('click', () => {
        const input = document.getElementById(inputId);
        if (input.type === 'password') {
            input.type = 'text';
            toggle.textContent = '🔒';
        } else {
            input.type = 'password';
            toggle.textContent = '👁';
        }
    });
}

setupPasswordToggle('wifi-pass', 'wifi-pass-toggle');
setupPasswordToggle('weather-key', 'weather-key-toggle');

// --- Display Settings ---
document.getElementById('brightness').addEventListener('input', (e) => {
    document.getElementById('brightness-val').textContent = e.target.value + '%';
});

document.getElementById('mood-interval').addEventListener('input', (e) => {
    document.getElementById('mood-interval-val').textContent = e.target.value + 's';
});

document.getElementById('btn-display-save').addEventListener('click', async () => {
    await apiPost('/api/config', {
        auto_switch: document.getElementById('auto-switch').checked,
        switch_interval: parseInt(document.getElementById('switch-interval').value),
        clock_duration: parseInt(document.getElementById('clock-duration').value),
        default_engine: parseInt(document.getElementById('default-engine').value),
        brightness: Math.round(parseInt(document.getElementById('brightness').value) * 255 / 100)
    });
    showToast('Display settings saved!');
});

// --- Settings ---
document.getElementById('btn-settings-save').addEventListener('click', async () => {
    const data = {
        tz_offset: parseInt(document.getElementById('tz-offset').value),
        night_enabled: document.getElementById('night-enabled').checked,
        night_start: parseInt(document.getElementById('night-start').value),
        night_end: parseInt(document.getElementById('night-end').value),
        weather_city: document.getElementById('weather-city').value,
        mood_interval: parseInt(document.getElementById('mood-interval').value)
    };

    // Only send weather key if it was actually changed
    const keyInput = document.getElementById('weather-key').value;
    if (keyInput && !keyInput.startsWith('••')) {
        data.weather_key = keyInput;
    }

    await apiPost('/api/config', data);
    showToast('Settings saved!');
});

// --- OTA ---
document.getElementById('btn-ota-check').addEventListener('click', async () => {
    const statusEl = document.getElementById('ota-status');
    const updateBtn = document.getElementById('btn-ota-update');
    
    statusEl.textContent = 'Checking for updates...';
    updateBtn.disabled = true;
    
    const result = await apiPost('/api/ota/check');
    statusEl.textContent = result ? result.msg : 'Check failed';
    
    if (result && result.has_update) {
        updateBtn.disabled = false;
    }
});

document.getElementById('btn-ota-update').addEventListener('click', async () => {
    showConfirm('Confirm Update', 'Install firmware update? Device will restart.', async () => {
        const statusEl = document.getElementById('ota-status');
        const updateBtn = document.getElementById('btn-ota-update');
        const progBar = document.getElementById('ota-progress-bar');
        const progFill = document.getElementById('ota-progress-fill');
        
        updateBtn.disabled = true;
        document.getElementById('btn-ota-check').disabled = true;
        
        progBar.classList.remove('hidden');
        progFill.style.width = '0%';
        statusEl.textContent = 'Starting update... do not power off!';
        
        // Trigger update asynchronously
        await apiPost('/api/ota/update');
        
        // Poll progress
        const interval = setInterval(async () => {
            try {
                const resp = await fetch('/api/ota/progress?_t=' + Date.now(), { cache: 'no-store' });
                if (resp.ok) {
                    const data = await resp.json();
                    
                    if (data.state === 'failed') {
                        statusEl.textContent = 'Update failed! See logs.';
                        progFill.style.backgroundColor = '#ff4444';
                        clearInterval(interval);
                        setTimeout(() => location.reload(), 5000);
                    } else if (data.state === 'no_updates') {
                        statusEl.textContent = 'Already up to date.';
                        progFill.style.width = '100%';
                        progFill.style.backgroundColor = '#888';
                        clearInterval(interval);
                        setTimeout(() => location.reload(), 3000);
                    } else if (data.state === 'success' || data.progress >= 100) {
                        progFill.style.width = '100%';
                        statusEl.textContent = 'Update complete! Rebooting...';
                        clearInterval(interval);
                        setTimeout(() => location.reload(), 10000);
                    } else {
                        progFill.style.width = data.progress + '%';
                        statusEl.textContent = `Updating: ${data.progress}%`;
                    }
                }
            } catch (e) {
                // Might drop connection when device reboots
                statusEl.textContent = 'Rebooting...';
                clearInterval(interval);
                setTimeout(() => location.reload(), 10000);
            }
        }, 1000);
    });
});

// --- Restart ---
document.getElementById('btn-restart').addEventListener('click', async () => {
    showConfirm('Restart Device', 'Restart the device? It will be unavailable for a few seconds.', async () => {
        await apiPost('/api/restart');
        document.getElementById('wifi-badge').textContent = 'Restarting...';
        document.getElementById('wifi-badge').className = 'badge badge-warn';
        setTimeout(() => location.reload(), 8000);
    });
});

// --- Init ---
refreshStatus();
refreshConfig();

// Auto-refresh every 5 seconds
setInterval(refreshStatus, 5000);
