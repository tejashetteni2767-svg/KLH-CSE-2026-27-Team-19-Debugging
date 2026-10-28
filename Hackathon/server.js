/**
 * ============================================================================
 * Project: Linux Signal-Based Process Controller (OSSP Hackathon)
 * File: server.js
 * Description: Express + WebSocket Server bridging the Web Dashboard with
 *              native Linux/WSL system calls and procfs introspection.
 * ============================================================================
 */

const express = require('express');
const http = require('http');
const path = require('path');
const fs = require('fs');
const { exec, spawn } = require('child_process');
const WebSocket = require('ws');

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server, path: '/ws' });

const PORT = process.env.PORT || 3000;
const IS_WINDOWS = process.platform === 'win32';

app.use(express.json());
app.use(express.static(path.join(__dirname, 'web')));
app.use('/src', express.static(path.join(__dirname, 'src')));

// In-memory tracked processes for simulation / live coordination
let trackedProcesses = [];
let hasWslBinary = false;

// Check if WSL and Linux binary exist
function checkEnvironment() {
  if (IS_WINDOWS) {
    exec('wsl test -f bin/proc_ctrl && echo "EXISTS"', { cwd: __dirname }, (err, stdout) => {
      hasWslBinary = !err && stdout.includes('EXISTS');
      console.log(`[SYSTEM] WSL Binary Check: ${hasWslBinary ? 'Found (/bin/proc_ctrl)' : 'Not found (will use high-fidelity simulation)'}`);
    });
  } else {
    hasWslBinary = fs.existsSync(path.join(__dirname, 'bin', 'proc_ctrl'));
  }
}
checkEnvironment();

// Helper to run proc_ctrl command in WSL or native Linux
function runProcCtrl(args, callback) {
  const binary = './bin/proc_ctrl';
  const fullCommand = IS_WINDOWS 
    ? `wsl ${binary} ${args}`
    : `${binary} ${args}`;

  exec(fullCommand, { cwd: __dirname, timeout: 5000 }, (error, stdout, stderr) => {
    if (error) {
      return callback(error, stderr || stdout);
    }
    callback(null, stdout);
  });
}

// ----------------- REST API Endpoints -----------------

// 1. System Status
app.get('/api/status', (req, res) => {
  res.json({
    status: 'online',
    platform: process.platform,
    arch: process.arch,
    node_version: process.version,
    wsl_available: IS_WINDOWS,
    has_native_binary: hasWslBinary,
    active_connections: wss.clients.size,
    timestamp: new Date().toISOString()
  });
});

// 2. List Processes
app.get('/api/processes', (req, res) => {
  if (hasWslBinary) {
    runProcCtrl('list --json', (err, output) => {
      if (!err && output) {
        try {
          const list = JSON.parse(output.trim());
          // Filter to our spawned workers or interesting processes
          const relevant = list.filter(p => 
            p.name.includes('dummy_worker') || 
            trackedProcesses.some(tp => tp.pid === p.pid)
          );
          return res.json({ mode: 'live_linux', processes: relevant.length > 0 ? relevant : trackedProcesses });
        } catch (e) {
          // fallback
        }
      }
      res.json({ mode: 'simulation', processes: trackedProcesses });
    });
  } else {
    res.json({ mode: 'simulation', processes: trackedProcesses });
  }
});

// 3. Spawn New Process
app.post('/api/spawn', (req, res) => {
  const { name = 'worker', interval = 1000, type = 'compute' } = req.body;

  if (hasWslBinary) {
    const cmd = IS_WINDOWS 
      ? `wsl bash -c "./bin/dummy_worker ${name}_${type} ${interval} > /dev/null 2>&1 & echo \\$!"`
      : `./bin/dummy_worker ${name}_${type} ${interval} > /dev/null 2>&1 & echo $!`;
    exec(cmd, { cwd: __dirname }, (err, stdout) => {
      if (err) {
        return res.status(500).json({ error: 'Failed to spawn process in Linux' });
      }
      const pid = parseInt(stdout.trim(), 10);
      const newProc = {
        pid,
        ppid: 1,
        name: `dummy_worker [${type}]`,
        workload: type,
        state: 'S',
        state_str: 'SLEEPING (Interruptible)',
        rss_kb: 2150,
        threads: 1,
        sig_cgt: 'SIGHUP, SIGINT, SIGUSR1, SIGUSR2, SIGTERM',
        created_at: new Date().toISOString()
      };
      trackedProcesses.push(newProc);
      broadcast({ type: 'PROCESS_SPAWNED', process: newProc });
      res.json({ status: 'success', pid, process: newProc });
    });
  } else {
    // High fidelity simulation spawner
    const mockPid = Math.floor(1000 + Math.random() * 9000);
    const newProc = {
      pid: mockPid,
      ppid: 1042,
      name: `${name}_${type}`,
      workload: type,
      state: 'R',
      state_str: 'RUNNING (Active)',
      rss_kb: Math.floor(1800 + Math.random() * 800),
      threads: 1,
      sig_cgt: 'SIGHUP, SIGINT, SIGUSR1, SIGUSR2, SIGTERM',
      created_at: new Date().toISOString()
    };
    trackedProcesses.push(newProc);
    broadcast({ type: 'PROCESS_SPAWNED', process: newProc });
    res.json({ status: 'success', pid: mockPid, process: newProc });
  }
});

// 4. Dispatch Signal (Pause, Resume, Terminate, Kill, Restart, Custom)
app.post('/api/signal', (req, res) => {
  const { pid, action, signalName } = req.body;
  const numPid = parseInt(pid, 10);

  if (!numPid) {
    return res.status(400).json({ error: 'Invalid PID provided' });
  }

  let command = '';
  switch (action) {
    case 'pause': command = `pause ${numPid} --json`; break;
    case 'resume': command = `resume ${numPid} --json`; break;
    case 'terminate': command = `terminate ${numPid} 3 --json`; break;
    case 'kill': command = `kill ${numPid} --json`; break;
    case 'restart': command = `restart ${numPid} --json`; break;
    case 'signal': command = `signal ${numPid} ${signalName || 'SIGUSR1'} --json`; break;
    default:
      return res.status(400).json({ error: `Unknown action ${action}` });
  }

  if (hasWslBinary) {
    runProcCtrl(command, (err, output) => {
      let result = null;
      try {
        result = JSON.parse(output.trim());
      } catch (e) {
        result = { status: 'executed', raw: output };
      }

      // Update in-memory state
      const proc = trackedProcesses.find(p => p.pid === numPid);
      if (proc) {
        if (action === 'pause') {
          proc.state = 'T';
          proc.state_str = 'STOPPED (Paused by Signal)';
        } else if (action === 'resume') {
          proc.state = 'S';
          proc.state_str = 'SLEEPING (Interruptible)';
        } else if (action === 'kill' || action === 'terminate') {
          proc.state = 'X';
          proc.state_str = 'TERMINATED';
          trackedProcesses = trackedProcesses.filter(p => p.pid !== numPid);
        }
      }

      broadcast({ type: 'SIGNAL_DISPATCHED', action, pid: numPid, result });
      res.json({ status: 'success', action, pid: numPid, result });
    });
  } else {
    // Simulated signal execution
    const proc = trackedProcesses.find(p => p.pid === numPid);
    const latency_us = (25 + Math.random() * 35).toFixed(2);
    let stateBefore = proc ? proc.state : 'R';
    let stateAfter = stateBefore;

    if (action === 'pause') {
      stateAfter = 'T';
      if (proc) { proc.state = 'T'; proc.state_str = 'STOPPED (Paused by Signal)'; }
    } else if (action === 'resume') {
      stateAfter = 'R';
      if (proc) { proc.state = 'R'; proc.state_str = 'RUNNING (Active)'; }
    } else if (action === 'terminate') {
      stateAfter = 'X';
      if (proc) { proc.state = 'X'; proc.state_str = 'TERMINATED'; }
      trackedProcesses = trackedProcesses.filter(p => p.pid !== numPid);
    } else if (action === 'kill') {
      stateAfter = 'X';
      if (proc) { proc.state = 'X'; proc.state_str = 'TERMINATED'; }
      trackedProcesses = trackedProcesses.filter(p => p.pid !== numPid);
    } else if (action === 'restart') {
      if (proc) {
        proc.state = 'R';
        proc.state_str = 'RUNNING (Active)';
      }
    }

    const mockResult = {
      status: 'success',
      pid: numPid,
      signal_name: signalName || (action === 'pause' ? 'SIGSTOP' : action === 'resume' ? 'SIGCONT' : action === 'kill' ? 'SIGKILL' : 'SIGTERM'),
      syscall: `kill(${numPid}, ${action.toUpperCase()})`,
      return_code: 0,
      errno: 0,
      errno_str: 'Success',
      latency_us: parseFloat(latency_us),
      state_before: stateBefore,
      state_after: stateAfter,
      process_alive: stateAfter !== 'X'
    };

    broadcast({ type: 'SIGNAL_DISPATCHED', action, pid: numPid, result: mockResult });
    res.json({ status: 'success', action, pid: numPid, result: mockResult });
  }
});

// 5. Inspect /proc/[pid]
app.get('/api/inspect/:pid', (req, res) => {
  const pid = parseInt(req.params.pid, 10);
  if (hasWslBinary) {
    runProcCtrl(`status ${pid} --json`, (err, output) => {
      if (!err && output) {
        try {
          return res.json(JSON.parse(output.trim()));
        } catch (e) {}
      }
      res.status(404).json({ error: 'Process inspection failed' });
    });
  } else {
    const proc = trackedProcesses.find(p => p.pid === pid);
    if (!proc) {
      return res.status(404).json({ error: 'Process not found' });
    }
    res.json({
      pid: proc.pid,
      ppid: proc.ppid,
      name: proc.name,
      state: proc.state,
      state_desc: proc.state_str,
      vm_size_kb: proc.rss_kb * 2,
      vm_rss_kb: proc.rss_kb,
      threads: proc.threads,
      sig_cgt_hex: '0x0000000000004a03',
      sig_cgt_names: proc.sig_cgt,
      sig_ign_hex: '0x0000000000000004',
      sig_ign_names: 'SIGQUIT',
      sig_blk_hex: '0x0000000000000000',
      sig_blk_names: 'None (0x0)',
      cmdline: `./bin/dummy_worker ${proc.name} 1000`
    });
  }
});

// 6. Source Files for in-browser viewer
app.get('/api/source/:file', (req, res) => {
  const allowed = ['proc_controller.c', 'dummy_worker.c', 'Makefile', 'test_signals.sh'];
  const fileName = req.params.file;
  if (!allowed.includes(fileName)) {
    return res.status(403).json({ error: 'Access denied' });
  }
  const filePath = path.join(__dirname, 'src', fileName);
  if (fs.existsSync(filePath)) {
    res.type('text/plain').sendFile(filePath);
  } else {
    res.status(404).json({ error: 'File not found' });
  }
});

// Broadcast helper for WebSocket
function broadcast(data) {
  const payload = JSON.stringify(data);
  wss.clients.forEach(client => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(payload);
    }
  });
}

// WebSocket Connection Handler
wss.on('connection', ws => {
  ws.send(JSON.stringify({ 
    type: 'CONNECTED', 
    message: 'Connected to Linux Signal Process Controller Daemon',
    has_native_binary: hasWslBinary 
  }));
});

server.listen(PORT, () => {
  console.log(`================================================================`);
  console.log(`  Linux Signal Process Controller Web Server running on port ${PORT}`);
  console.log(`  Access Dashboard: http://localhost:${PORT}`);
  console.log(`  OS / Backend Mode: ${hasWslBinary ? 'Live Linux (WSL Native)' : 'High-Fidelity Virtual Kernel'}`);
  console.log(`================================================================`);
});
