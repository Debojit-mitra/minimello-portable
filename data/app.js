/* =============================================================
   MiniMello WebUI — Application Logic
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
async function refreshStatus() {
    const s = await apiGet('/api/status');
    if (!s) return;

    // Header badges
    const wifiBadge = document.getElementById('wifi-badge');
    wifiBadge.textContent = s.wifi_connected ? 'WiFi ✓' : 'WiFi ✗';
    wifiBadge.className = 'badge ' + (s.wifi_connected ? '' : 'badge-off');

    const battBadge = document.getElementById('batt-badge');
    battBadge.textContent = s.battery_percent + '%';
    battBadge.className = 'badge ' + (s.battery_percent <= 10 ? 'badge-off' :
                                       s.battery_percent <= 25 ? 'badge-warn' : '');

    document.getElementById('version-badge').textContent = 'v' + s.version;
    document.getElementById('active-engine').textContent = 'Active: ' +
        s.active_engine + ' (' + (s.active_engine === 'emotion' ? s.emotion : s.clock_face) + ')';

    // System info
    document.getElementById('info-version').textContent = s.version;
    document.getElementById('info-battery').textContent =
        s.battery_percent + '% (' + s.battery_voltage.toFixed(2) + 'V)';
    document.getElementById('info-wifi').textContent =
        s.wifi_connected ? ('Connected (' + s.wifi_rssi + ' dBm)') : 'Disconnected';
    document.getElementById('info-ip').textContent = s.ip;
    document.getElementById('info-uptime').textContent = formatUptime(s.uptime_s);
    document.getElementById('info-heap').textContent = (s.free_heap / 1024).toFixed(1) + ' KB';

    // Highlight active emotion/face buttons
    document.querySelectorAll('.emotion-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.emotion == getEmotionIndex(s.emotion));
    });
    document.querySelectorAll('.face-btn').forEach(b => {
        b.classList.toggle('active', b.dataset.face == getFaceIndex(s.clock_face));
    });
}

function formatUptime(s) {
    const h = Math.floor(s / 3600);
    const m = Math.floor((s % 3600) / 60);
    return h > 0 ? h + 'h ' + m + 'm' : m + 'm ' + (s % 60) + 's';
}

function getEmotionIndex(name) {
    const map = { 'Neutral': 0, 'Happy': 1, 'Sad': 2, 'Angry': 3,
                  'Surprised': 4, 'Sleepy': 5, 'Love': 6, 'Wink': 7 };
    return map[name] ?? -1;
}

function getFaceIndex(name) {
    const map = { 'Digital': 0, 'Analog': 1, 'Minimal': 2 };
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
    document.getElementById('tz-offset').value = c.tz_offset / 3600;
    document.getElementById('night-enabled').checked = c.night_enabled;
    document.getElementById('night-start').value = c.night_start;
    document.getElementById('night-end').value = c.night_end;
    document.getElementById('weather-city').value = c.weather_city || '';
    document.getElementById('weather-key').value = c.weather_key_set ? '••••••••' : '';
    document.getElementById('debug-mode').checked = c.debug_mode;
    if (c.mood_interval !== undefined) {
        document.getElementById('mood-interval').value = c.mood_interval;
        document.getElementById('mood-interval-val').textContent = c.mood_interval + 's';
    }
}

// --- Emotion Buttons ---
document.querySelectorAll('.emotion-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
        await apiPost('/api/emotion', { emotion: parseInt(btn.dataset.emotion) });
        setTimeout(refreshStatus, 300);
    });
});

// --- Clock Face Buttons ---
document.querySelectorAll('.face-btn').forEach(btn => {
    btn.addEventListener('click', async () => {
        await apiPost('/api/clock/face', { face: parseInt(btn.dataset.face) });
        setTimeout(refreshStatus, 300);
    });
});

// --- Switch Engine ---
document.getElementById('btn-switch').addEventListener('click', async () => {
    await apiPost('/api/switch');
    setTimeout(refreshStatus, 300);
});

// --- WiFi ---
document.getElementById('btn-scan').addEventListener('click', async () => {
    const listEl = document.getElementById('wifi-networks');
    listEl.innerHTML = '<p class="muted">Scanning...</p>';

    // First call triggers scan
    await apiGet('/api/wifi/scan');

    // Wait and poll for results
    setTimeout(async () => {
        const data = await apiGet('/api/wifi/scan');
        if (!data || !data.networks) {
            listEl.innerHTML = '<p class="muted">No networks found</p>';
            return;
        }
        listEl.innerHTML = '';
        data.networks.forEach(net => {
            const item = document.createElement('div');
            item.className = 'network-item';
            item.innerHTML = '<span>' + net.ssid + (net.secure ? ' 🔒' : '') + '</span>' +
                             '<span class="signal">' + net.rssi + ' dBm</span>';
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

    const statusEl = document.getElementById('wifi-status');
    statusEl.textContent = 'Connecting to ' + ssid + '...';

    await apiPost('/api/wifi', { ssid, pass });
    statusEl.textContent = 'WiFi credentials saved. Reconnecting...';

    setTimeout(refreshStatus, 5000);
});

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
    alert('Display settings saved!');
});

// --- Settings ---
document.getElementById('btn-settings-save').addEventListener('click', async () => {
    const data = {
        tz_offset: Math.round(parseFloat(document.getElementById('tz-offset').value) * 3600),
        night_enabled: document.getElementById('night-enabled').checked,
        night_start: parseInt(document.getElementById('night-start').value),
        night_end: parseInt(document.getElementById('night-end').value),
        weather_city: document.getElementById('weather-city').value,
        debug_mode: document.getElementById('debug-mode').checked,
        mood_interval: parseInt(document.getElementById('mood-interval').value)
    };

    // Only send weather key if it was actually changed (not the masked placeholder)
    const keyInput = document.getElementById('weather-key').value;
    if (keyInput && !keyInput.startsWith('••')) {
        data.weather_key = keyInput;
    }

    await apiPost('/api/config', data);
    alert('Settings saved!');
});

// --- OTA ---
document.getElementById('btn-ota-check').addEventListener('click', async () => {
    const statusEl = document.getElementById('ota-status');
    statusEl.textContent = 'Checking for updates...';
    const result = await apiPost('/api/ota/check');
    statusEl.textContent = result ? result.msg : 'Check failed';
});

document.getElementById('btn-ota-update').addEventListener('click', async () => {
    if (!confirm('Install firmware update? Device will restart.')) return;
    const statusEl = document.getElementById('ota-status');
    statusEl.textContent = 'Updating... do not power off!';
    await apiPost('/api/ota/update');
});

// --- Init ---
refreshStatus();
refreshConfig();

// Auto-refresh every 5 seconds
setInterval(refreshStatus, 5000);
