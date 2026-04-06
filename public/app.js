const IP_EL = document.getElementById('ip');
const PORT_EL = document.getElementById('port');
const PROTO_EL = document.getElementById('protocol');
const STATUS_EL = document.getElementById('connection-status');
const LOG_VIEW = document.getElementById('log-view');
const POWER_EL = document.getElementById('power');
const POWER_VAL = document.getElementById('power-val');
const DURATION_EL = document.getElementById('duration');

const TEL_PKT = document.getElementById('tel-pkt');
const TEL_GRADE = document.getElementById('tel-grade');
const TEL_HITS = document.getElementById('tel-hits');
const TEL_HEADING = document.getElementById('tel-heading');
const CMD_NAME = document.getElementById('last-cmd-name');
const CMD_VAL = document.getElementById('last-cmd-val');
const CMD_POWER = document.getElementById('last-cmd-power');

// Initial state
let isConnected = false;

// Update power label
POWER_EL.addEventListener('input', (e) => {
    POWER_VAL.innerText = `${e.target.value}%`;
});

// Connect button handler
document.getElementById('btn-connect').addEventListener('click', async () => {
    const data = {
        ip: IP_EL.value,
        port: parseInt(PORT_EL.value),
        protocol: PROTO_EL.value
    };

    try {
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
            addLog('SYSTEM', result.message);
        } else {
            addLog('SYSTEM', result.message, 'error');
        }
    } catch (err) {
        addLog('SYSTEM', 'Connection Failed: ' + err.message, 'error');
    }
});

// Direction and Command handlers
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
    try {
        const res = await fetch('/telementry_request/');
        const result = await res.json();
        
        updateTelemetry(result);
        addLog('RECV', result.message);
    } catch (err) {
        addLog('SYSTEM', 'Telemetry Request Failed', 'error');
    }
});

document.getElementById('btn-clear-logs').addEventListener('click', () => {
    LOG_VIEW.innerHTML = '';
});

// Helper: Send command
async function sendCommand(cmdName) {
    const data = {
        command: cmdName,
        duration: parseInt(DURATION_EL.value),
        power: parseInt(POWER_EL.value)
    };

    try {
        const res = await fetch('/telecommand/', {
            method: 'PUT',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(data)
        });
        const result = await res.json();
        
        if (result.success) {
            addLog('ACK', `${cmdName.toUpperCase()} - ${result.message}`);
        } else {
            addLog('NACK', `${cmdName.toUpperCase()} - ${result.message}`, 'error');
        }
    } catch (err) {
        addLog('SYSTEM', 'Command Failed: ' + err.message, 'error');
    }
}

// Helper: Update Telemetry UI
function updateTelemetry(data) {
    if (data.success && data.telemetry) {
        const t = data.telemetry;
        TEL_PKT.innerText = t.lastPktCounter;
        TEL_GRADE.innerText = t.currentGrade;
        TEL_HITS.innerText = t.hitCount;
        TEL_HEADING.innerText = t.heading;
        CMD_NAME.innerText = getCmdName(t.lastCmd);
        CMD_VAL.innerText = t.lastCmdValue;
        CMD_POWER.innerText = `${t.lastCmdPower}%`;
    }
}

function getCmdName(id) {
    switch(id) {
        case 1: return 'FORWARD';
        case 2: return 'BACKWARD';
        case 3: return 'RIGHT';
        case 4: return 'LEFT';
        default: return 'NONE';
    }
}

// Helper: Add Log to UI
function addLog(dir, content, type = '') {
    const ts = new Date().toLocaleTimeString();
    const div = document.createElement('div');
    div.className = `log-entry ${type}`;
    div.innerHTML = `<span class="log-ts">[${ts}]</span><span class="log-dir">${dir}</span><span class="log-content">${content}</span>`;
    
    LOG_VIEW.prepend(div);
}

// Poll logs occasionally
setInterval(async () => {
    if (!isConnected) return;
    try {
        const res = await fetch('/logs');
        const data = await res.json();
        // Here we could sync logs if desired, 
        // but for now local logs are added by actions.
    } catch (err) {}
}, 5000);
