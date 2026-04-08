const IP_EL = document.getElementById('ip');
const PORT_EL = document.getElementById('port');
const PROTO_EL = document.getElementById('protocol');
const STATUS_EL = document.getElementById('connection-status');
const OPMODE_BADGE = document.getElementById('opmode-status');
const LOG_VIEW = document.getElementById('log-view');
const POWER_VAL = document.getElementById('power-val');
const POWER_EL = document.getElementById('power');
const DURATION_EL = document.getElementById('duration');

const STAT_SENT = document.getElementById('stat-sent');
const STAT_ACCEPTED = document.getElementById('stat-accepted');
const STAT_REJECTED = document.getElementById('stat-rejected');
const STAT_PKT = document.getElementById('stat-pkt');

const DRIVE_ERR = document.getElementById('drive-error');
const TEL_ERR = document.getElementById('telemetry-error');

const CMD_NAMES = { 0: 'DRIVE', 1: 'SLEEP', 2: 'RESPONSE' };
const DIR_NAMES = { 1: 'FORWARD', 2: 'BACKWARD', 3: 'RIGHT', 4: 'LEFT' };

// Initial state
let isConnected = false;

function showError(msg, target = null) {
    let errEl = null;
    if (target === 'drive') errEl = DRIVE_ERR;
    else if (target === 'telemetry') errEl = TEL_ERR;
    else {
        DRIVE_ERR.innerText = msg;
        DRIVE_ERR.classList.remove('hidden');
        TEL_ERR.innerText = msg;
        TEL_ERR.classList.remove('hidden');
        return;
    }
    errEl.innerText = msg;
    errEl.classList.remove('hidden');
}

function hideErrors() {
    DRIVE_ERR.classList.add('hidden');
    TEL_ERR.classList.add('hidden');
}

POWER_EL.addEventListener('input', (e) => {
    POWER_VAL.innerText = `${e.target.value}%`;
});

document.getElementById('btn-connect').addEventListener('click', async () => {
    const btn = document.getElementById('btn-connect');
    const data = {
        ip: IP_EL.value,
        port: parseInt(PORT_EL.value),
        protocol: PROTO_EL.value
    };

    if (!data.ip || isNaN(data.port)) {
        showError('Please provide a valid IP and Port.');
        return;
    }

    try {
        btn.disabled = true;
        STATUS_EL.innerText = 'Connecting...';
        STATUS_EL.className = 'status-badge disconnected';

        const res = await fetch('/connect', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        const result = await res.json();

        if (result.success) {
            isConnected = true;
            STATUS_EL.innerText = 'Connected';
            STATUS_EL.className = 'status-badge connected';
            OPMODE_BADGE.classList.remove('hidden');
            setOpMode('unknown');
            addLog('SYSTEM', result.message);
            updateStats(result.stats);
            hideErrors();
        } else {
            isConnected = false;
            STATUS_EL.innerText = 'Disconnected';
            STATUS_EL.className = 'status-badge disconnected';
            OPMODE_BADGE.classList.add('hidden');
            addLog('SYSTEM', result.message, 'error');
            showError(result.message);
        }
    } catch (err) {
        isConnected = false;
        STATUS_EL.innerText = 'Disconnected';
        OPMODE_BADGE.classList.add('hidden');
        addLog('SYSTEM', 'Connection Failed: ' + err.message, 'error');
        showError('Network Error: Could not reach server.');
    } finally {
        btn.disabled = false;
    }
});

document.querySelectorAll('.btn-ctrl').forEach(btn => {
    btn.addEventListener('click', async () => {
        const cmd = btn.getAttribute('data-cmd');
        await sendCommand(cmd);
    });
});

document.getElementById('btn-sleep').addEventListener('click', async () => {
    if (confirm('Verify: Put robot into Sleep Mode?')) {
        await sendCommand('sleep');
    }
});

document.getElementById('btn-telemetry').addEventListener('click', async () => {
    if (!isConnected) {
        showError('Please connect to the robot first.', 'telemetry');
        return;
    }
    const btn = document.getElementById('btn-telemetry');
    hideErrors();
    try {
        btn.disabled = true;
        const res = await fetch('/telementry_request/');
        const result = await res.json();

        updateTelemetry(result);
        updateStats(result.stats);
        if (result.success && result.ack) {
            addLog('RECV', result.message);
        } else {
            addLog('RECV', result.message || 'Telemetry Error', 'error');
            showError(`Telemetry Failed: ${result.message || 'No response'}`, 'telemetry');
        }
    } catch (err) {
        addLog('SYSTEM', 'Telemetry Request Failed', 'error');
        showError('Server Error during telemetry request.', 'telemetry');
    } finally {
        btn.disabled = false;
    }
});

async function sendCommand(cmdName) {
    if (!isConnected) {
        showError('Please connect to the robot first.', 'drive');
        return;
    }
    hideErrors();
    const durationVal = parseInt(DURATION_EL.value);
    const powerVal = parseInt(POWER_EL.value);

    // Protocol strictly uses 1 byte (0-255) for duration and power
    if (isNaN(durationVal) || durationVal < 0 || durationVal > 255) {
        showError('Duration must be a number between 0 and 255.', 'drive');
        return;
    }
    if (isNaN(powerVal) || powerVal < 0 || powerVal > 255) {
        showError('Power must be a number between 0 and 255.', 'drive');
        return;
    }

    const data = {
        command: cmdName,
        duration: durationVal,
        power: powerVal
    };

    try {
        const btns = document.querySelectorAll('.btn-ctrl, #btn-sleep, #btn-telemetry');
        btns.forEach(b => b.disabled = true);

        const res = await fetch('/telecommand/', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        const result = await res.json();

        updateStats(result.stats);
        if (result.success && result.ack) {
            addLog('ACK', `${cmdName.toUpperCase()} - ${result.message}`);
            
            if (cmdName === 'sleep') {
                setOpMode('sleep');
            } else if (['forward', 'backward', 'left', 'right'].includes(cmdName)) {
                setOpMode('active');
            }
        } else {
            const errMsg = result.message || 'Operation Failed';
            addLog('NACK', `${cmdName.toUpperCase()} - ${errMsg}`, 'error');
            showError(`${cmdName.toUpperCase()} Failed: ${errMsg}`, 'drive');
        }
    } catch (err) {
        addLog('SYSTEM', 'Command Failed: ' + err.message, 'error');
        showError('Server communication error.', 'drive');
    } finally {
        const btns = document.querySelectorAll('.btn-ctrl, #btn-sleep, #btn-telemetry');
        btns.forEach(b => b.disabled = false);
    }
}

function setOpMode(mode) {
    OPMODE_BADGE.className = 'status-badge'; // Reset classes
    if (mode === 'sleep') {
        OPMODE_BADGE.textContent = 'Mode: SLEEPING';
        OPMODE_BADGE.classList.add('opmode-sleep');
    } else if (mode === 'active') {
        OPMODE_BADGE.textContent = 'Mode: ACTIVE DRIVE';
        OPMODE_BADGE.classList.add('opmode-active');
    } else {
        OPMODE_BADGE.textContent = 'Mode: Unknown';
        OPMODE_BADGE.classList.add('opmode-unknown');
    }
}

function updateStats(stats) {
    if (!stats) return;
    STAT_SENT.innerText = stats.sent;
    STAT_ACCEPTED.innerText = stats.accepted;
    STAT_REJECTED.innerText = stats.rejected;
    STAT_PKT.innerText = stats.pktCount;
}

function updateTelemetry(data) {
    if (data.success && data.telemetry) {
        const t = data.telemetry;
        document.getElementById('tel-pkt').innerText = t.lastPktCounter;
        document.getElementById('tel-grade').innerText = t.currentGrade;
        document.getElementById('tel-hits').innerText = t.hitCount;
        document.getElementById('tel-heading').innerText = t.heading + '°';

        // Only telemetry requests (cmd type 2) don't update the last command
        // cmd=0 with value=0 means no drive command has been executed yet
        let cmdName;
        if (t.lastCmd === 0 && t.lastCmdValue === 0) {
            cmdName = 'N/A (No drive command sent)';
        } else if (t.lastCmd === 0) {
            const dirName = DIR_NAMES[t.lastCmdValue] || `Unknown (${t.lastCmdValue})`;
            cmdName = `DRIVE (${dirName})`;
        } else {
            cmdName = CMD_NAMES[t.lastCmd] || `Unknown (${t.lastCmd})`;
        }
        document.getElementById('last-cmd-name').innerText = cmdName;
        document.getElementById('last-cmd-val').innerText =
            t.lastCmdValue > 0 ? (DIR_NAMES[t.lastCmdValue] || t.lastCmdValue) : 'N/A';
        document.getElementById('last-cmd-power').innerText =
            t.lastCmdValue > 0 ? `${t.lastCmdPower}%` : 'N/A';
    }
}

// === Routing Table ===
const ROUTING_STATUS = document.getElementById('routing-status');

async function setRouting(enabled) {
    const ip = document.getElementById('route-ip').value.trim();
    const port = parseInt(document.getElementById('route-port').value);

    if (enabled && (!ip || isNaN(port))) {
        alert('Please enter a valid Next-Hop IP and Port.');
        return;
    }

    try {
        const body = enabled
            ? JSON.stringify({ target_ip: ip, target_port: port, enabled: true })
            : JSON.stringify({ enabled: false });

        const res = await fetch('/routing_table/', {
            method: enabled ? 'POST' : 'POST',
            headers: { 'Content-Type': 'application/json' },
            body
        });
        const result = await res.json();

        if (result.success && result.enabled) {
            ROUTING_STATUS.textContent = `Routing Active -> ${result.target_ip}:${result.target_port}`;
            ROUTING_STATUS.className = 'routing-badge enabled';
            addLog('ROUTE', `Routing enabled -> ${result.target_ip}:${result.target_port}`);
        } else if (!result.success) {
            ROUTING_STATUS.textContent = 'Routing Failed - Target Unreachable';
            ROUTING_STATUS.className = 'routing-badge disabled';
            addLog('ROUTE', `Routing failed: ${result.message}`, 'error');
        } else {
            ROUTING_STATUS.textContent = 'Routing Disabled';
            ROUTING_STATUS.className = 'routing-badge disabled';
            addLog('ROUTE', 'Routing disabled.');
        }
    } catch (err) {
        addLog('SYSTEM', 'Routing config failed: ' + err.message, 'error');
    }
}

document.getElementById('btn-route-enable').addEventListener('click', () => setRouting(true));
document.getElementById('btn-route-disable').addEventListener('click', () => setRouting(false));

function addLog(dir, content, type = '') {
    const ts = new Date().toLocaleTimeString();
    const div = document.createElement('div');
    div.className = `log-entry ${type}`;
    div.innerHTML = `<span class="log-ts">[${ts}]</span><span class="log-dir">${dir}</span><span class="log-content">${content}</span>`;
    LOG_VIEW.prepend(div);
}

document.getElementById('btn-clear-logs').addEventListener('click', () => {
    LOG_VIEW.innerHTML = '';
});
