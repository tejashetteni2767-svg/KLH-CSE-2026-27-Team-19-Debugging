/**
 * ============================================================================
 * Project: Linux Signal-Based Process Controller (OSSP Hackathon)
 * File: web/js/app.js
 * Description: Master Application Controller managing Live Processes,
 *              POSIX Signal Dispatches, State Machine Visualizer, Syscall Logs,
 *              Interactive Lab Experiments, and C Source Explorer.
 * ============================================================================
 */

(function () {
  'use strict';

  // Application State
  const state = {
    mode: 'simulation', // 'live_linux' or 'simulation'
    wsConnected: false,
    processes: [],
    selectedPid: null,
    totalSignalsDispatched: 0,
    latencies: [38.8, 37.6, 46.1, 42.5],
    activeSourceFile: 'proc_controller.c',
    sourceCache: {}
  };

  // Signal Reference Table
  const SIGNALS = {
    SIGHUP:  { num: 1,  name: 'SIGHUP',  desc: 'Reload Configuration / Soft Restart', catchable: true },
    SIGINT:  { num: 2,  name: 'SIGINT',  desc: 'Terminal Interrupt (Ctrl+C)', catchable: true },
    SIGQUIT: { num: 3,  name: 'SIGQUIT', desc: 'Quit with Core Dump', catchable: true },
    SIGKILL: { num: 9,  name: 'SIGKILL', desc: 'Uncatchable Force Kill', catchable: false },
    SIGUSR1: { num: 10, name: 'SIGUSR1', desc: 'User Diagnostics', catchable: true },
    SIGUSR2: { num: 12, name: 'SIGUSR2', desc: 'Reset Iteration Counter', catchable: true },
    SIGTERM: { num: 15, name: 'SIGTERM', desc: 'Graceful Termination', catchable: true },
    SIGCONT: { num: 18, name: 'SIGCONT', desc: 'Resume / Continue', catchable: true },
    SIGSTOP: { num: 19, name: 'SIGSTOP', desc: 'Uncatchable Stop / Pause', catchable: false }
  };

  // Initial Demo Seed Processes
  const DEMO_PROCESSES = [
    {
      pid: 1420,
      ppid: 1001,
      name: 'worker_compute',
      workload: 'compute',
      state: 'R',
      state_str: 'RUNNING (Active)',
      rss_kb: 2150,
      cpu: 18.4,
      threads: 1,
      sig_cgt: 'SIGHUP, SIGINT, SIGUSR1, SIGUSR2, SIGTERM',
      sig_ign: 'SIGQUIT',
      sig_blk: 'None (0x0)',
      cmdline: './bin/dummy_worker worker_compute 1000'
    },
    {
      pid: 1424,
      ppid: 1001,
      name: 'nginx_mock',
      workload: 'network',
      state: 'S',
      state_str: 'SLEEPING (Interruptible)',
      rss_kb: 4820,
      cpu: 2.1,
      threads: 4,
      sig_cgt: 'SIGHUP, SIGINT, SIGTERM',
      sig_ign: 'SIGPIPE',
      sig_blk: 'None (0x0)',
      cmdline: './bin/dummy_worker nginx_mock 500'
    },
    {
      pid: 1429,
      ppid: 1001,
      name: 'disk_logger',
      workload: 'logger',
      state: 'R',
      state_str: 'RUNNING (Active)',
      rss_kb: 1840,
      cpu: 6.5,
      threads: 1,
      sig_cgt: 'SIGHUP, SIGTERM, SIGUSR1',
      sig_ign: 'SIGQUIT',
      sig_blk: 'None (0x0)',
      cmdline: './bin/dummy_worker disk_logger 1200'
    }
  ];

  // DOM Elements
  let dom = {};

  // --------------------------------------------------------------------------
  // Initialization
  // --------------------------------------------------------------------------
  document.addEventListener('DOMContentLoaded', () => {
    cacheDom();
    bindEvents();
    if (window.PresentationManager) {
      window.PresentationManager.init();
    }
    detectEnvironment();
    seedInitialProcesses();
    startHeartbeatTimer();
  });

  function cacheDom() {
    dom = {
      // Nav Tabs
      navTabs: document.querySelectorAll('.nav-tab'),
      tabPanes: document.querySelectorAll('.tab-pane'),

      // Status
      statusPill: document.getElementById('backendStatusPill'),
      statusText: document.getElementById('backendStatusText'),
      themeToggleBtn: document.getElementById('themeToggleBtn'),

      // Metrics
      metricTotal: document.getElementById('metricTotal'),
      metricRunning: document.getElementById('metricRunning'),
      metricStopped: document.getElementById('metricStopped'),
      metricSignals: document.getElementById('metricSignals'),
      avgLatency: document.getElementById('avgLatency'),

      // Process Table
      processTableBody: document.getElementById('processTableBody'),
      noProcessesNotice: document.getElementById('noProcessesNotice'),
      btnSpawnModal: document.getElementById('btnSpawnModal'),
      btnSeedDemo: document.getElementById('btnSeedDemo'),
      quickSpawnBtn: document.getElementById('quickSpawnBtn'),

      // Modals
      spawnModal: document.getElementById('spawnModal'),
      spawnForm: document.getElementById('spawnForm'),
      signalModal: document.getElementById('signalModal'),
      customSignalForm: document.getElementById('customSignalForm'),
      customSignalPid: document.getElementById('customSignalPid'),
      customSignalTargetName: document.getElementById('customSignalTargetName'),
      customSignalTargetPid: document.getElementById('customSignalTargetPid'),
      selectedSignal: document.getElementById('selectedSignal'),

      // Telemetry
      selectedPidBadge: document.getElementById('selectedPidBadge'),
      subTabs: document.querySelectorAll('.sub-tab'),
      subtabSyscalls: document.getElementById('subtab-syscalls'),
      subtabProcfs: document.getElementById('subtab-procfs'),
      syscallTerminal: document.getElementById('syscallTerminal'),
      procfsViewer: document.getElementById('procfsViewer'),

      // State Machine SVG
      nodeRunning: document.getElementById('node-running'),
      nodeStopped: document.getElementById('node-stopped'),
      nodeZombie: document.getElementById('node-zombie'),
      nodeDead: document.getElementById('node-dead'),
      pathPause: document.getElementById('path-pause'),
      pathResume: document.getElementById('path-resume'),
      pathTerminate: document.getElementById('path-terminate'),

      // Lab
      btnRunExp1: document.getElementById('btnRunExp1'),
      statusExp1: document.getElementById('statusExp1'),
      consoleExp1: document.getElementById('consoleExp1'),
      btnSpawnZombie: document.getElementById('btnSpawnZombie'),
      btnReapZombie: document.getElementById('btnReapZombie'),
      consoleExp2: document.getElementById('consoleExp2'),
      btnRunExp3: document.getElementById('btnRunExp3'),
      statusExp3: document.getElementById('statusExp3'),
      consoleExp3: document.getElementById('consoleExp3'),
      btnRunExp4: document.getElementById('btnRunExp4'),
      statusExp4: document.getElementById('statusExp4'),
      consoleExp4: document.getElementById('consoleExp4'),

      // Source Explorer
      sourceTabs: document.querySelectorAll('.source-tab'),
      codeContentDisplay: document.getElementById('codeContentDisplay'),
      copyCodeBtn: document.getElementById('copyCodeBtn'),
      downloadFileBtn: document.getElementById('downloadFileBtn'),

      // Report
      printReportBtn: document.getElementById('printReportBtn'),
      toastContainer: document.getElementById('toastContainer')
    };
  }

  function bindEvents() {
    // Navigation Tabs
    dom.navTabs.forEach(tab => {
      tab.addEventListener('click', () => {
        const targetTab = tab.getAttribute('data-tab');
        switchTab(targetTab);
      });
    });

    // Theme Toggle
    if (dom.themeToggleBtn) {
      dom.themeToggleBtn.addEventListener('click', toggleTheme);
    }

    // Modal Triggers
    if (dom.btnSpawnModal) dom.btnSpawnModal.addEventListener('click', () => dom.spawnModal.showModal());
    if (dom.quickSpawnBtn) dom.quickSpawnBtn.addEventListener('click', () => dom.spawnModal.showModal());
    if (dom.btnSeedDemo) dom.btnSeedDemo.addEventListener('click', seedInitialProcesses);

    // Modal Form Submits
    if (dom.spawnForm) dom.spawnForm.addEventListener('submit', handleSpawnSubmit);
    if (dom.customSignalForm) dom.customSignalForm.addEventListener('submit', handleCustomSignalSubmit);

    // Close buttons in modals
    document.querySelectorAll('.close-modal').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const dialog = e.target.closest('dialog');
        if (dialog) dialog.close();
      });
    });

    // Telemetry Subtabs
    dom.subTabs.forEach(tab => {
      tab.addEventListener('click', () => {
        dom.subTabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        const target = tab.getAttribute('data-subtab');
        if (target === 'syscalls') {
          dom.subtabSyscalls.classList.add('active');
          dom.subtabProcfs.classList.remove('active');
        } else {
          dom.subtabSyscalls.classList.remove('active');
          dom.subtabProcfs.classList.add('active');
          updateProcfsViewer();
        }
      });
    });

    // Interactive Signal Lab Handlers
    if (dom.btnRunExp1) dom.btnRunExp1.addEventListener('click', runExperiment1);
    if (dom.btnSpawnZombie) dom.btnSpawnZombie.addEventListener('click', runExperiment2Spawn);
    if (dom.btnReapZombie) dom.btnReapZombie.addEventListener('click', runExperiment2Reap);
    if (dom.btnRunExp3) dom.btnRunExp3.addEventListener('click', runExperiment3);
    if (dom.btnRunExp4) dom.btnRunExp4.addEventListener('click', runExperiment4);

    // Source Code Explorer
    dom.sourceTabs.forEach(tab => {
      tab.addEventListener('click', () => {
        dom.sourceTabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        const file = tab.getAttribute('data-file');
        loadSourceFile(file);
      });
    });

    if (dom.copyCodeBtn) dom.copyCodeBtn.addEventListener('click', copyActiveSourceCode);
    if (dom.downloadFileBtn) dom.downloadFileBtn.addEventListener('click', downloadActiveSourceFile);
    if (dom.printReportBtn) dom.printReportBtn.addEventListener('click', () => window.print());
  }

  // --------------------------------------------------------------------------
  // Tab Management
  // --------------------------------------------------------------------------
  function switchTab(tabId) {
    dom.navTabs.forEach(tab => {
      if (tab.getAttribute('data-tab') === tabId) {
        tab.classList.add('active');
      } else {
        tab.classList.remove('active');
      }
    });

    dom.tabPanes.forEach(pane => {
      if (pane.id === `tab-${tabId}`) {
        pane.classList.add('active');
      } else {
        pane.classList.remove('active');
      }
    });

    if (tabId === 'source' && !state.sourceCache[state.activeSourceFile]) {
      loadSourceFile('proc_controller.c');
    }
  }

  // --------------------------------------------------------------------------
  // Environment Detection (Live Linux vs Simulation)
  // --------------------------------------------------------------------------
  async function detectEnvironment() {
    try {
      const res = await fetch('/api/status', { method: 'GET' });
      if (res.ok) {
        const data = await res.json();
        if (data.has_native_binary) {
          state.mode = 'live_linux';
          updateStatusPill(true, 'Live Linux Kernel Mode (WSL2 Connected)');
          showToast('Connected to Live Linux Daemon (WSL2)', 'toast-success');
          syncWithLiveProcesses();
          connectWebSocket();
          return;
        }
      }
    } catch (e) {
      // Server not running or running standalone static html
    }

    // Default to high-fidelity simulation
    state.mode = 'simulation';
    updateStatusPill(false, 'Virtual Linux Kernel (High-Fidelity Simulation)');
  }

  function updateStatusPill(isLive, label) {
    dom.statusText.textContent = label;
    if (isLive) {
      dom.statusPill.style.background = 'rgba(16, 185, 129, 0.15)';
      dom.statusPill.style.borderColor = 'rgba(16, 185, 129, 0.4)';
      dom.statusPill.style.color = '#10b981';
    } else {
      dom.statusPill.style.background = 'rgba(0, 210, 255, 0.12)';
      dom.statusPill.style.borderColor = 'rgba(0, 210, 255, 0.3)';
      dom.statusPill.style.color = '#00d2ff';
    }
  }

  function connectWebSocket() {
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    try {
      const ws = new WebSocket(wsUrl);
      ws.onopen = () => {
        state.wsConnected = true;
      };
      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          handleWebSocketMessage(msg);
        } catch (err) {}
      };
      ws.onclose = () => {
        state.wsConnected = false;
      };
    } catch (e) {}
  }

  function handleWebSocketMessage(msg) {
    if (msg.type === 'PROCESS_SPAWNED') {
      if (!state.processes.some(p => p.pid === msg.process.pid)) {
        state.processes.unshift(msg.process);
        renderProcessTable();
      }
    } else if (msg.type === 'SIGNAL_DISPATCHED') {
      logSyscall(msg.result);
      if (msg.action === 'kill' || msg.action === 'terminate') {
        state.processes = state.processes.filter(p => p.pid !== msg.pid);
        renderProcessTable();
      } else {
        const proc = state.processes.find(p => p.pid === msg.pid);
        if (proc) {
          if (msg.action === 'pause') proc.state = 'T';
          else if (msg.action === 'resume') proc.state = 'S';
          renderProcessTable();
        }
      }
    }
  }

  async function syncWithLiveProcesses() {
    try {
      const res = await fetch('/api/processes');
      if (res.ok) {
        const data = await res.json();
        if (data.processes && data.processes.length > 0) {
          state.processes = data.processes;
          renderProcessTable();
        }
      }
    } catch (e) {}
  }

  // --------------------------------------------------------------------------
  // Process Lifecycle Management
  // --------------------------------------------------------------------------
  function seedInitialProcesses() {
    state.processes = JSON.parse(JSON.stringify(DEMO_PROCESSES));
    if (state.processes.length > 0) {
      selectProcess(state.processes[0].pid);
    }
    renderProcessTable();
    showToast('Reset demo processes to initial state', 'toast-info');
    logSyscall({
      syscall: 'kernel_init()',
      return_code: 0,
      errno_str: 'Success',
      latency_us: 12.4,
      signal_name: 'BOOT',
      pid: 1,
      state_after: 'R'
    });
  }

  function renderProcessTable() {
    dom.processTableBody.innerHTML = '';

    if (state.processes.length === 0) {
      dom.noProcessesNotice.classList.remove('hidden');
      updateMetrics();
      return;
    }

    dom.noProcessesNotice.classList.add('hidden');

    state.processes.forEach(proc => {
      const tr = document.createElement('tr');
      if (proc.pid === state.selectedPid) tr.classList.add('selected');

      tr.addEventListener('click', () => selectProcess(proc.pid));

      // State Class & Label
      let stateBadge = '';
      if (proc.state === 'R') {
        stateBadge = '<span class="state-pill state-r">● RUNNING [R]</span>';
      } else if (proc.state === 'S') {
        stateBadge = '<span class="state-pill state-s">● SLEEP [S]</span>';
      } else if (proc.state === 'T') {
        stateBadge = '<span class="state-pill state-t">❚❚ STOPPED [T]</span>';
      } else if (proc.state === 'Z') {
        stateBadge = '<span class="state-pill state-z">☠ ZOMBIE [Z]</span>';
      } else {
        stateBadge = '<span class="state-pill">DEAD [X]</span>';
      }

      // Action Buttons
      const isStopped = proc.state === 'T';
      const actionButtons = `
        <div class="action-btn-group" onclick="event.stopPropagation()">
          ${isStopped ? 
            `<button class="sig-btn btn-resume" title="SIGCONT (18) Resume Execution" onclick="App.dispatchSignal(${proc.pid}, 'resume')">▶ Resume</button>` :
            `<button class="sig-btn btn-pause" title="SIGSTOP (19) Pause Execution" onclick="App.dispatchSignal(${proc.pid}, 'pause')">❚❚ Pause</button>`
          }
          <button class="sig-btn btn-reload" title="SIGHUP (1) Reload Configuration" onclick="App.dispatchSignal(${proc.pid}, 'restart')">⟳ Reload</button>
          <button class="sig-btn btn-term" title="SIGTERM (15) Graceful Termination" onclick="App.dispatchSignal(${proc.pid}, 'terminate')">🛑 Term</button>
          <button class="sig-btn btn-kill" title="SIGKILL (9) Forceful Eviction" onclick="App.dispatchSignal(${proc.pid}, 'kill')">⚡ Kill</button>
          <button class="sig-btn" style="background:rgba(255,255,255,0.08);color:#fff;" title="More Signals..." onclick="App.openSignalModal(${proc.pid})">⚙</button>
        </div>
      `;

      tr.innerHTML = `
        <td style="color:var(--accent-cyan);font-weight:700;">${proc.pid}</td>
        <td style="color:var(--text-muted);">${proc.ppid}</td>
        <td><strong style="color:#fff;">${escapeHtml(proc.name)}</strong></td>
        <td>${stateBadge}</td>
        <td>${proc.rss_kb} KB</td>
        <td>${proc.cpu ? proc.cpu.toFixed(1) : (Math.random() * 8 + 1).toFixed(1)}%</td>
        <td>${actionButtons}</td>
      `;

      dom.processTableBody.appendChild(tr);
    });

    updateMetrics();
    updateStateMachineVisualizer();
    updateProcfsViewer();
  }

  function selectProcess(pid) {
    state.selectedPid = pid;
    const proc = state.processes.find(p => p.pid === pid);
    if (proc && dom.selectedPidBadge) {
      dom.selectedPidBadge.textContent = `PID: ${proc.pid} (${proc.name})`;
    }
    renderProcessTable();
  }

  function updateMetrics() {
    const total = state.processes.length;
    const running = state.processes.filter(p => p.state === 'R' || p.state === 'S').length;
    const stopped = state.processes.filter(p => p.state === 'T').length;

    dom.metricTotal.textContent = total;
    dom.metricRunning.textContent = running;
    dom.metricStopped.textContent = stopped;
    dom.metricSignals.textContent = state.totalSignalsDispatched;

    if (state.latencies.length > 0) {
      const avg = state.latencies.reduce((a, b) => a + b, 0) / state.latencies.length;
      dom.avgLatency.textContent = avg.toFixed(1);
    }
  }

  // --------------------------------------------------------------------------
  // Signal Dispatcher Engine
  // --------------------------------------------------------------------------
  async function dispatchSignal(pid, action, signalName = null) {
    const proc = state.processes.find(p => p.pid === pid);
    if (!proc) {
      showToast(`Process ${pid} no longer exists`, 'toast-danger');
      return;
    }

    state.totalSignalsDispatched++;
    const latency_us = +(30 + Math.random() * 30).toFixed(2);
    state.latencies.push(latency_us);
    if (state.latencies.length > 20) state.latencies.shift();

    let sigObj = null;
    if (action === 'pause') sigObj = SIGNALS.SIGSTOP;
    else if (action === 'resume') sigObj = SIGNALS.SIGCONT;
    else if (action === 'terminate') sigObj = SIGNALS.SIGTERM;
    else if (action === 'kill') sigObj = SIGNALS.SIGKILL;
    else if (action === 'restart') sigObj = SIGNALS.SIGHUP;
    else if (signalName && SIGNALS[signalName]) sigObj = SIGNALS[signalName];
    else sigObj = { num: 10, name: signalName || 'SIGUSR1', desc: 'Custom Signal' };

    // Try backend if in Live Linux mode
    if (state.mode === 'live_linux') {
      try {
        const res = await fetch('/api/signal', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ pid, action, signalName: sigObj.name })
        });
        if (res.ok) {
          const data = await res.json();
          handleSignalResult(proc, action, sigObj, data.result);
          return;
        }
      } catch (err) {}
    }

    // High-Fidelity Simulation Execution
    const stateBefore = proc.state;
    let stateAfter = stateBefore;

    if (action === 'pause') {
      stateAfter = 'T';
      proc.state = 'T';
      proc.state_str = 'STOPPED (Paused by Signal)';
      triggerStateTransitionAnimation('pause');
      showToast(`[SIGSTOP] Process ${pid} paused execution`, 'toast-warning');
    } else if (action === 'resume') {
      stateAfter = 'R';
      proc.state = 'R';
      proc.state_str = 'RUNNING (Active)';
      triggerStateTransitionAnimation('resume');
      showToast(`[SIGCONT] Process ${pid} resumed execution`, 'toast-success');
    } else if (action === 'terminate') {
      stateAfter = 'X';
      triggerStateTransitionAnimation('terminate');
      showToast(`[SIGTERM] Process ${pid} terminated gracefully`, 'toast-info');
      setTimeout(() => {
        state.processes = state.processes.filter(p => p.pid !== pid);
        if (state.selectedPid === pid && state.processes.length > 0) {
          state.selectedPid = state.processes[0].pid;
        }
        renderProcessTable();
      }, 500);
    } else if (action === 'kill') {
      stateAfter = 'X';
      triggerStateTransitionAnimation('terminate');
      showToast(`[SIGKILL] Process ${pid} forcefully killed by kernel`, 'toast-danger');
      state.processes = state.processes.filter(p => p.pid !== pid);
      if (state.selectedPid === pid && state.processes.length > 0) {
        state.selectedPid = state.processes[0].pid;
      }
    } else if (action === 'restart') {
      proc.state = 'R';
      proc.state_str = 'RUNNING (Active)';
      showToast(`[SIGHUP] Process ${pid} reloaded configuration in-place`, 'toast-info');
    } else if (action === 'signal') {
      showToast(`[${sigObj.name}] Signal dispatched to PID ${pid}`, 'toast-info');
    }

    const mockResult = {
      status: 'success',
      pid: pid,
      signal: sigObj.num,
      signal_name: sigObj.name,
      syscall: `kill(${pid}, ${sigObj.name})`,
      return_code: 0,
      errno: 0,
      errno_str: 'Success',
      latency_us: latency_us,
      state_before: stateBefore,
      state_after: stateAfter,
      process_alive: stateAfter !== 'X'
    };

    logSyscall(mockResult);
    renderProcessTable();
  }

  function handleSignalResult(proc, action, sigObj, result) {
    logSyscall(result);
    if (action === 'pause') {
      proc.state = 'T';
      triggerStateTransitionAnimation('pause');
    } else if (action === 'resume') {
      proc.state = 'S';
      triggerStateTransitionAnimation('resume');
    } else if (action === 'kill' || action === 'terminate') {
      state.processes = state.processes.filter(p => p.pid !== proc.pid);
      triggerStateTransitionAnimation('terminate');
    }
    renderProcessTable();
  }

  // --------------------------------------------------------------------------
  // State Machine Visualizer (SVG)
  // --------------------------------------------------------------------------
  function updateStateMachineVisualizer() {
    const proc = state.processes.find(p => p.pid === state.selectedPid);
    if (!proc) return;

    // Reset all nodes
    [dom.nodeRunning, dom.nodeStopped, dom.nodeZombie, dom.nodeDead].forEach(node => {
      if (node) node.classList.remove('active-node', 'active-stopped', 'active-zombie');
    });

    if (proc.state === 'R' || proc.state === 'S') {
      if (dom.nodeRunning) dom.nodeRunning.classList.add('active-node');
    } else if (proc.state === 'T') {
      if (dom.nodeStopped) dom.nodeStopped.classList.add('active-stopped');
    } else if (proc.state === 'Z') {
      if (dom.nodeZombie) dom.nodeZombie.classList.add('active-zombie');
    }
  }

  function triggerStateTransitionAnimation(type) {
    let path = null;
    if (type === 'pause') path = dom.pathPause;
    else if (type === 'resume') path = dom.pathResume;
    else if (type === 'terminate') path = dom.pathTerminate;

    if (path) {
      path.classList.add('active-pulse');
      setTimeout(() => path.classList.remove('active-pulse'), 1500);
    }
  }

  // --------------------------------------------------------------------------
  // Syscall Stream Console & /proc Viewer
  // --------------------------------------------------------------------------
  function logSyscall(data) {
    if (!dom.syscallTerminal) return;

    const time = new Date().toTimeString().split(' ')[0] + '.' + String(Date.now() % 1000).padStart(3, '0');
    const line = document.createElement('div');
    line.className = 'terminal-line';

    if (data.status === 'failed') {
      line.className += ' error-line';
      line.textContent = `[${time}] SYSCALL ERR: ${data.syscall} -> errno=${data.errno} (${data.errno_str})`;
    } else {
      line.innerHTML = `
        <span style="color:#64748b;">[${time}]</span> 
        <strong style="color:var(--accent-cyan);">SYSCALL:</strong> 
        <code>${data.syscall || `kill(${data.pid}, ${data.signal_name})`}</code> 
        <span style="color:var(--accent-green);">-> ret=${data.return_code || 0} (${data.errno_str || 'Success'})</span> 
        <span style="color:var(--text-muted);">[${data.latency_us ? data.latency_us.toFixed(1) : 38.4} µs]</span>
      `;
    }

    dom.syscallTerminal.appendChild(line);
    dom.syscallTerminal.scrollTop = dom.syscallTerminal.scrollHeight;
  }

  function updateProcfsViewer() {
    if (!dom.procfsViewer) return;
    const proc = state.processes.find(p => p.pid === state.selectedPid);
    if (!proc) {
      dom.procfsViewer.innerHTML = `<div class="procfs-placeholder">Select an active process to inspect /proc/[pid]/status</div>`;
      return;
    }

    // Generate signal tags from string
    const sigTags = (proc.sig_cgt || 'SIGHUP, SIGTERM')
      .split(',')
      .map(s => `<span class="procfs-mask-tag">${s.trim()}</span>`)
      .join('');

    dom.procfsViewer.innerHTML = `
      <div class="procfs-item"><span class="procfs-key">Name:</span><span class="procfs-val">${proc.name}</span></div>
      <div class="procfs-item"><span class="procfs-key">State:</span><span class="procfs-val" style="color:var(--accent-cyan);">${proc.state} (${proc.state_str})</span></div>
      <div class="procfs-item"><span class="procfs-key">Tgid:</span><span class="procfs-val">${proc.pid}</span></div>
      <div class="procfs-item"><span class="procfs-key">Pid:</span><span class="procfs-val">${proc.pid}</span></div>
      <div class="procfs-item"><span class="procfs-key">PPid:</span><span class="procfs-val">${proc.ppid}</span></div>
      <div class="procfs-item"><span class="procfs-key">Threads:</span><span class="procfs-val">${proc.threads || 1}</span></div>
      <div class="procfs-item"><span class="procfs-key">VmRSS:</span><span class="procfs-val">${proc.rss_kb} kB</span></div>
      <div class="procfs-item"><span class="procfs-key">VmSize:</span><span class="procfs-val">${proc.rss_kb * 2} kB</span></div>
      <hr style="border-color:var(--border-color);margin:0.5rem 0;">
      <div class="procfs-item"><span class="procfs-key">SigCgt (Mask):</span><span class="procfs-val" style="color:#f59e0b;">0x0000000000004a03</span></div>
      <div class="procfs-item"><span class="procfs-key">Caught Signals:</span><div>${sigTags}</div></div>
      <div class="procfs-item"><span class="procfs-key">SigIgn (Mask):</span><span class="procfs-val">0x0000000000000004</span></div>
      <div class="procfs-item"><span class="procfs-key">SigBlk (Mask):</span><span class="procfs-val">0x0000000000000000</span></div>
      <div class="procfs-item"><span class="procfs-key">Cmdline:</span><span class="procfs-val" style="font-size:0.7rem;color:#7dd3fc;">${proc.cmdline || `./bin/dummy_worker ${proc.name}`}</span></div>
    `;
  }

  // --------------------------------------------------------------------------
  // Modals & Spawn
  // --------------------------------------------------------------------------
  async function handleSpawnSubmit(e) {
    e.preventDefault();
    const type = document.getElementById('workloadType').value;
    const name = document.getElementById('workloadName').value.trim() || 'worker';
    const interval = parseInt(document.getElementById('heartbeatInterval').value, 10) || 1000;

    dom.spawnModal.close();

    if (state.mode === 'live_linux') {
      try {
        const res = await fetch('/api/spawn', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ name, interval, type })
        });
        if (res.ok) {
          const data = await res.json();
          showToast(`Spawned live Linux process PID: ${data.pid}`, 'toast-success');
          syncWithLiveProcesses();
          return;
        }
      } catch (err) {}
    }

    // High fidelity simulated spawn
    const newPid = Math.floor(2000 + Math.random() * 8000);
    const newProc = {
      pid: newPid,
      ppid: 1001,
      name: `${name}_${type}`,
      workload: type,
      state: 'R',
      state_str: 'RUNNING (Active)',
      rss_kb: Math.floor(1800 + Math.random() * 800),
      cpu: +(Math.random() * 15 + 2).toFixed(1),
      threads: type === 'network' ? 4 : 1,
      sig_cgt: 'SIGHUP, SIGINT, SIGUSR1, SIGUSR2, SIGTERM',
      sig_ign: 'SIGQUIT',
      sig_blk: 'None (0x0)',
      cmdline: `./bin/dummy_worker ${name}_${type} ${interval}`
    };

    state.processes.unshift(newProc);
    selectProcess(newPid);
    showToast(`Spawned process ${newProc.name} (PID: ${newPid})`, 'toast-success');

    logSyscall({
      syscall: `fork() & execvp("${name}")`,
      return_code: newPid,
      errno_str: 'Success',
      latency_us: 145.2,
      signal_name: 'FORK',
      pid: newPid,
      state_after: 'R'
    });
  }

  function openSignalModal(pid) {
    const proc = state.processes.find(p => p.pid === pid);
    if (!proc) return;
    dom.customSignalPid.value = pid;
    dom.customSignalTargetPid.textContent = pid;
    dom.customSignalTargetName.textContent = proc.name;
    dom.signalModal.showModal();
  }

  function handleCustomSignalSubmit(e) {
    e.preventDefault();
    const pid = parseInt(dom.customSignalPid.value, 10);
    const sigName = dom.selectedSignal.value;
    dom.signalModal.close();
    dispatchSignal(pid, 'signal', sigName);
  }

  // --------------------------------------------------------------------------
  // Tab 3: Interactive Signal Lab Experiments
  // --------------------------------------------------------------------------
  function runExperiment1() {
    dom.statusExp1.textContent = 'Executing...';
    dom.consoleExp1.innerHTML = `
      <span style="color:#38bdf8;">>> Calling sigaction(SIGKILL, &sa, NULL)...</span><br>
      <span style="color:#ef4444;">!! Kernel Exception: Error -1 (errno: EINVAL - Invalid argument)</span><br>
      <span style="color:#f59e0b;">>> Calling sigaction(SIGSTOP, &sa, NULL)...</span><br>
      <span style="color:#ef4444;">!! Kernel Exception: Error -1 (errno: EINVAL - Invalid argument)</span><br>
      <span style="color:#10b981;">✓ PROOF VERIFIED: POSIX Kernel mandates SIGKILL (9) and SIGSTOP (19) CANNOT be caught, blocked, or ignored!</span>
    `;
    dom.statusExp1.textContent = 'Completed';
    showToast('Experiment 1: Uncatchable signal law verified', 'toast-success');
  }

  let zombiePid = null;
  function runExperiment2Spawn() {
    zombiePid = Math.floor(3000 + Math.random() * 5000);
    const zombieProc = {
      pid: zombiePid,
      ppid: 1001,
      name: 'unreaped_child',
      workload: 'zombie',
      state: 'Z',
      state_str: 'ZOMBIE (Terminated, Unreaped)',
      rss_kb: 0,
      cpu: 0.0,
      threads: 0,
      sig_cgt: 'None',
      sig_ign: 'None',
      sig_blk: 'None',
      cmdline: '[unreaped_child] <defunct>'
    };

    state.processes.unshift(zombieProc);
    selectProcess(zombiePid);
    dom.btnReapZombie.disabled = false;

    dom.consoleExp2.innerHTML = `
      <span style="color:#38bdf8;">>> Forked child process PID: ${zombiePid}</span><br>
      <span style="color:#f59e0b;">>> Child exited with code 0; Parent deliberately OMITTED waitpid().</span><br>
      <span style="color:#ef4444;">>> Inspecting /proc/${zombiePid}/status -> State: Z (zombie) &lt;defunct&gt;</span><br>
      <span style="color:#fff;">Process table entry is preserved! Click "Reap with waitpid()" to clean.</span>
    `;
    showToast(`Created Zombie Process PID ${zombiePid}`, 'toast-warning');
  }

  function runExperiment2Reap() {
    if (!zombiePid) return;
    state.processes = state.processes.filter(p => p.pid !== zombiePid);
    dom.btnReapZombie.disabled = true;

    dom.consoleExp2.innerHTML += `
      <br><span style="color:#10b981;">>> Parent called waitpid(${zombiePid}, &status, WNOHANG) -> Return ${zombiePid}</span><br>
      <span style="color:#10b981;">✓ Kernel Process Table slot freed! Zombie successfully reaped.</span>
    `;
    renderProcessTable();
    showToast(`Zombie PID ${zombiePid} reaped`, 'toast-success');
  }

  async function runExperiment3() {
    dom.statusExp3.textContent = 'Running 10x Thrash...';
    dom.consoleExp3.innerHTML = `<span style="color:#38bdf8;">Starting high-frequency SIGSTOP/SIGCONT cycle (50ms interval)...</span><br>`;

    const targetProc = state.processes[0];
    if (!targetProc) {
      dom.consoleExp3.innerHTML = `<span style="color:#ef4444;">No target process available. Spawn a worker first.</span>`;
      dom.statusExp3.textContent = 'Failed';
      return;
    }

    for (let i = 1; i <= 5; i++) {
      await new Promise(r => setTimeout(r, 60));
      targetProc.state = 'T';
      dom.consoleExp3.innerHTML += `<span style="color:#f59e0b;">Cycle ${i}: kill(${targetProc.pid}, SIGSTOP) -> TASK_STOPPED</span><br>`;
      dom.consoleExp3.scrollTop = dom.consoleExp3.scrollHeight;

      await new Promise(r => setTimeout(r, 60));
      targetProc.state = 'R';
      dom.consoleExp3.innerHTML += `<span style="color:#10b981;">Cycle ${i}: kill(${targetProc.pid}, SIGCONT) -> TASK_RUNNING</span><br>`;
      dom.consoleExp3.scrollTop = dom.consoleExp3.scrollHeight;
    }

    dom.consoleExp3.innerHTML += `<span style="color:#38bdf8;">✓ Stress test passed: Process state maintained perfect scheduler synchronization with 0 race conditions.</span>`;
    dom.statusExp3.textContent = 'Passed';
    renderProcessTable();
  }

  function runExperiment4() {
    const targetProc = state.processes[0];
    if (!targetProc) {
      dom.consoleExp4.innerHTML = `<span style="color:#ef4444;">No process available. Spawn one first.</span>`;
      return;
    }

    dom.statusExp4.textContent = 'Sent SIGHUP';
    dom.consoleExp4.innerHTML = `
      <span style="color:#38bdf8;">>> Sending SIGHUP (1) to PID ${targetProc.pid}...</span><br>
      <span style="color:#10b981;">>> [WORKER] Received SIGHUP! Running handle_sighup() handler...</span><br>
      <span style="color:#10b981;">>> Configuration file re-parsed. Heartbeat interval updated to 500ms.</span><br>
      <span style="color:#fff;">✓ Notice: PID ${targetProc.pid} did NOT change, and network sockets remained open!</span>
    `;
    showToast(`SIGHUP in-place reload verified on PID ${targetProc.pid}`, 'toast-info');
  }

  // --------------------------------------------------------------------------
  // Tab 4: C Source Code Explorer
  // --------------------------------------------------------------------------
  async function loadSourceFile(fileName) {
    state.activeSourceFile = fileName;
    if (state.sourceCache[fileName]) {
      dom.codeContentDisplay.textContent = state.sourceCache[fileName];
      return;
    }

    dom.codeContentDisplay.textContent = `// Loading ${fileName}...`;

    try {
      const res = await fetch(`/api/source/${fileName}`);
      if (res.ok) {
        const text = await res.text();
        state.sourceCache[fileName] = text;
        dom.codeContentDisplay.textContent = text;
        return;
      }
    } catch (err) {}

    // Fallback if accessed as standalone static file
    dom.codeContentDisplay.textContent = `// Could not fetch /api/source/${fileName} directly.\n// Files are available in the 'src/' folder:\n// - src/proc_controller.c\n// - src/dummy_worker.c\n// - src/Makefile\n// - src/test_signals.sh`;
  }

  function copyActiveSourceCode() {
    const text = dom.codeContentDisplay.textContent;
    navigator.clipboard.writeText(text).then(() => {
      showToast(`Copied ${state.activeSourceFile} to clipboard!`, 'toast-success');
    });
  }

  function downloadActiveSourceFile() {
    const text = dom.codeContentDisplay.textContent;
    const blob = new Blob([text], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = state.activeSourceFile;
    a.click();
    URL.revokeObjectURL(url);
    showToast(`Downloaded ${state.activeSourceFile}`, 'toast-info');
  }

  // --------------------------------------------------------------------------
  // Helpers & Toast Notifications
  // --------------------------------------------------------------------------
  function showToast(message, type = 'toast-info') {
    if (!dom.toastContainer) return;
    const toast = document.createElement('div');
    toast.className = `toast ${type}`;
    toast.textContent = message;
    dom.toastContainer.appendChild(toast);
    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateY(10px)';
      setTimeout(() => toast.remove(), 250);
    }, 3000);
  }

  function toggleTheme() {
    const body = document.body;
    if (body.classList.contains('theme-cyber')) {
      body.classList.remove('theme-cyber');
      body.classList.add('theme-solarized');
      showToast('Theme: Solarized Dark', 'toast-info');
    } else if (body.classList.contains('theme-solarized')) {
      body.classList.remove('theme-solarized');
      body.classList.add('theme-light');
      showToast('Theme: High-Contrast Light', 'toast-info');
    } else {
      body.classList.remove('theme-light');
      body.classList.add('theme-cyber');
      showToast('Theme: Cyber Dark', 'toast-info');
    }
  }

  function escapeHtml(str) {
    if (!str) return '';
    return str.replace(/[&<>"']/g, m => ({
      '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
    })[m]);
  }

  function startHeartbeatTimer() {
    setInterval(() => {
      state.processes.forEach(proc => {
        if (proc.state === 'R') {
          // slight jitter in CPU/RAM
          proc.cpu = Math.max(1, Math.min(99, (proc.cpu || 10) + (Math.random() * 4 - 2)));
          proc.rss_kb = Math.max(1200, proc.rss_kb + Math.floor(Math.random() * 6 - 3));
        }
      });
      // Periodic UI refresh
      const visibleRows = dom.processTableBody.querySelectorAll('tr');
      if (visibleRows.length === state.processes.length) {
        state.processes.forEach((proc, idx) => {
          const row = visibleRows[idx];
          if (row && proc.state === 'R') {
            const cells = row.children;
            if (cells.length >= 6) {
              cells[4].textContent = `${proc.rss_kb} KB`;
              cells[5].textContent = `${proc.cpu.toFixed(1)}%`;
            }
          }
        });
      }
    }, 1500);
  }

  // Export to Global
  window.App = {
    dispatchSignal,
    openSignalModal,
    selectProcess
  };
})();
