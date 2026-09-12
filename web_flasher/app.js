// Clicker ESP32 Web Flasher Controller

const DEFAULT_RELEASES = [
  {
    tag: "v0.1.0",
    version: "0.1.0",
    name: "Clicker Device Firmware",
    manifest: "releases/v0.1.0/manifest.json",
    bin: "releases/v0.1.0/firmware.bin",
    factory_bin: "releases/v0.1.0/factory_firmware.bin",
    size: 567984,
    factory_size: 633520,
    is_latest: true
  },
  {
    tag: "v0.0.1",
    version: "0.0.1",
    name: "Clicker Device Firmware",
    manifest: "releases/v0.0.1/manifest.json",
    bin: "releases/v0.0.1/firmware.bin",
    factory_bin: "releases/v0.0.1/factory_firmware.bin",
    size: 373216,
    factory_size: 438752,
    is_latest: false
  }
];

// Turbo High-Speed Flashing Hook (PlatformIO 460,800 baud upload rate):
// When ESP Web Tools instantiates ESPLoader for writing flash, we upgrade baudrate
// to 460,800 baud so stub flasher runs at 4x speed.
let _esploaderInstance = null;
try {
  Object.defineProperty(window, 'esploader', {
    configurable: true,
    enumerable: true,
    get() {
      return _esploaderInstance;
    },
    set(inst) {
      _esploaderInstance = inst;
      if (inst) {
        console.info('[Web Flasher] Activating turbo upload speed: 460,800 baud (PlatformIO speed)');
        inst.baudrate = 460800;
      }
    }
  });
} catch (e) {
  console.warn('[Web Flasher] Could not hook window.esploader setter:', e);
}

let sessionAttempts = parseInt(sessionStorage.getItem('flasher_session_attempts') || '0', 10);
let ch340Reassured = sessionStorage.getItem('flasher_ch340_reassured') === 'true';

document.addEventListener('DOMContentLoaded', async () => {
  initUnsupportedModal();
  checkBrowserCompatibility();
  initInstallButtonWrapper();
  setupErrorInterceptors();
  await detectWorkingReleasePrefix();
  await loadVersionRegistry();
  initVersionSelector();
  renderDownloadsTable();
  initClipboardButtons();
});

function checkBrowserCompatibility() {
  const statusEl = document.getElementById('browser-status');
  const activateBtn = document.getElementById('activate-install-btn');
  const espInstallBtn = document.getElementById('esp-install-btn');
  const isSerialSupported = 'serial' in navigator;
  const isHttpsOrLocal = window.isSecureContext || window.location.protocol === 'https:' || window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1';

  if (!isSerialSupported) {
    if (statusEl) {
      statusEl.className = 'browser-status unsupported';
      statusEl.innerHTML = `
        <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="flex-shrink:0;">
          <circle cx="12" cy="12" r="10"></circle>
          <line x1="12" y1="8" x2="12" y2="12"></line>
          <line x1="12" y1="16" x2="12.01" y2="16"></line>
        </svg>
        <span style="font-size:0.92rem;line-height:1.45;">Web flashing requires a Chromium-based browser (Chrome, Edge, Brave, or Opera) — Firefox and Safari don't support this. Please switch browsers, or use the Desktop CLI / flash_device.bat option instead.</span>
      `;
    }
    if (activateBtn) {
      activateBtn.disabled = true;
      activateBtn.style.opacity = '0.5';
      activateBtn.style.cursor = 'not-allowed';
      activateBtn.setAttribute('aria-disabled', 'true');
      activateBtn.title = "Web Serial unsupported in this browser";
      activateBtn.addEventListener('click', (e) => {
        e.preventDefault();
        e.stopPropagation();
        openUnsupportedModal();
      }, true);
    }
    if (espInstallBtn) {
      espInstallBtn.style.pointerEvents = 'none';
    }
    openUnsupportedModal();
    return false;
  } else if (!isHttpsOrLocal) {
    if (statusEl) {
      statusEl.className = 'browser-status unsupported';
      statusEl.innerHTML = `
        <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="flex-shrink:0;">
          <rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect>
          <path d="M7 11V7a5 5 0 0 1 10 0v4"></path>
        </svg>
        <span><strong>HTTPS Required:</strong> Web Serial requires HTTPS or localhost to communicate with USB devices.</span>
      `;
    }
    if (activateBtn) {
      activateBtn.disabled = true;
      activateBtn.style.opacity = '0.5';
      activateBtn.style.cursor = 'not-allowed';
    }
    return false;
  } else {
    if (statusEl) {
      statusEl.className = 'browser-status supported';
      statusEl.innerHTML = `
        <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="flex-shrink:0;">
          <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path>
          <polyline points="22 4 12 14.01 9 11.01"></polyline>
        </svg>
        <span><strong>Web Serial Ready:</strong> Connect the device via USB-C and click below to flash.</span>
      `;
    }
    return true;
  }
}

function initUnsupportedModal() {
  const modal = document.getElementById('unsupported-modal');
  const closeBtn = document.getElementById('btn-modal-close');
  const manualBtn = document.getElementById('btn-modal-manual');

  if (closeBtn && modal) {
    closeBtn.addEventListener('click', () => {
      modal.style.display = 'none';
    });
  }

  if (manualBtn && modal) {
    manualBtn.addEventListener('click', () => {
      modal.style.display = 'none';
      const manualSection = document.getElementById('manual-section');
      if (manualSection) {
        manualSection.scrollIntoView({ behavior: 'smooth' });
      }
    });
  }

  if (modal) {
    modal.addEventListener('click', (e) => {
      if (e.target === modal) {
        modal.style.display = 'none';
      }
    });
  }
}

function openUnsupportedModal() {
  const modal = document.getElementById('unsupported-modal');
  if (modal) {
    modal.style.display = 'flex';
  }
}

function showCH340ReassuranceBanner() {
  const banner = document.getElementById('ch340-reassurance-banner');
  if (banner) {
    banner.style.display = 'flex';
    banner.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
}

function hideCH340ReassuranceBanner() {
  const banner = document.getElementById('ch340-reassurance-banner');
  if (banner) {
    banner.style.display = 'none';
  }
}

function initInstallButtonWrapper() {
  const activateBtn = document.getElementById('activate-install-btn');
  if (activateBtn) {
    activateBtn.addEventListener('click', () => {
      sessionAttempts++;
      sessionStorage.setItem('flasher_session_attempts', sessionAttempts.toString());
      hideCH340ReassuranceBanner();
      console.log(`[ESP Web Flasher] Session install attempt #${sessionAttempts}`);
    });
  }
}

function setupErrorInterceptors() {
  // 1. Intercept unhandled promise rejections
  window.addEventListener('unhandledrejection', (event) => {
    const reason = event.reason;
    const errorMsg = (reason && (reason.message || reason.toString())) || '';

    // Suppress Improv-related errors post-flash
    if (/improv/i.test(errorMsg)) {
      console.info('[ESP Web Flasher] Suppressed expected cosmetic Improv Wi-Fi serial notice:', errorMsg);
      event.preventDefault();
      return;
    }

    // Intercept CH340 first-attempt failure
    if (/already open|timeout|device has been lost|failed to initialize|failed to connect/i.test(errorMsg)) {
      if (sessionAttempts <= 1 && !ch340Reassured) {
        ch340Reassured = true;
        sessionStorage.setItem('flasher_ch340_reassured', 'true');
        showCH340ReassuranceBanner();
        console.warn('[ESP Web Flasher] Surfacing reassuring CH340 driver notice on initial attempt failure.');
        event.preventDefault();
      }
    }
  });

  // 2. Observe DOM for esp-web-tools dialog creation and errors
  const observer = new MutationObserver(() => {
    const dialog = document.querySelector('ewt-install-dialog');
    if (dialog) {
      document.body.classList.add('dialog-blur-active');
      injectDialogStyles(dialog);

      if (!dialog._outsideClickHooked) {
        dialog._outsideClickHooked = true;
        dialog.addEventListener('click', (e) => {
          const container = dialog.shadowRoot?.querySelector('ew-dialog')?.shadowRoot?.querySelector('.container');
          const path = e.composedPath();
          if (container && (e.target === container || path.includes(container))) {
            return;
          }
          if (e.target === dialog && !dialog._hasStartedFlashing) {
            dialog._closeDialog();
          }
        });
      }

      if (dialog.shadowRoot) {
        const text = dialog.shadowRoot.textContent || '';

        // Suppress Improv Wi-Fi Serial not detected from breaking user experience
        if (/improv/i.test(text)) {
          console.info('[ESP Web Flasher] Improv check in dialog bypassed.');
        }

        // Check for first attempt CH340 driver quirk
        if (/already open|timeout|device has been lost|failed to initialize|failed to connect/i.test(text)) {
          if (sessionAttempts <= 1 && !ch340Reassured) {
            ch340Reassured = true;
            sessionStorage.setItem('flasher_ch340_reassured', 'true');
            showCH340ReassuranceBanner();
          }
        }
      }
    } else {
      document.body.classList.remove('dialog-blur-active');
    }
  });

  observer.observe(document.body, { childList: true, subtree: true });
}

/**
 * Apply Dark Theme and Emerald Border to the inner ew-dialog component
 */
function applyInnerDialogTheme(ewDialog) {
  if (!ewDialog || !ewDialog.shadowRoot) return;
  if (ewDialog.shadowRoot.querySelector('#clicker-inner-dialog-theme')) return;

  const innerStyle = document.createElement('style');
  innerStyle.id = 'clicker-inner-dialog-theme';
  innerStyle.textContent = `
    dialog {
      background: transparent !important;
      border: none !important;
      outline: none !important;
      overflow: visible !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: center !important;
      justify-content: center !important;
      margin: auto !important;
      padding: 0 !important;
      width: auto !important;
      height: auto !important;
      max-width: min(520px, 92vw) !important;
      max-height: 90vh !important;
      z-index: 9999 !important;
      pointer-events: auto !important;
    }
    .container {
      background-color: #09090b !important;
      background-image:
        radial-gradient(rgba(255, 255, 255, 0.12) 1px, transparent 1px),
        linear-gradient(180deg, #111115 0%, #09090b 100%) !important;
      background-size: 24px 24px, 100% 100% !important;
      border: 1.5px solid rgba(255, 255, 255, 0.22) !important;
      border-radius: 20px !important;
      box-shadow: 0 30px 80px rgba(0, 0, 0, 0.95), 0 0 40px rgba(255, 255, 255, 0.08), inset 0 1px 0 rgba(255, 255, 255, 0.22) !important;
      width: min(500px, 90vw) !important;
      max-width: 500px !important;
      height: auto !important;
      min-height: auto !important;
      max-height: 85vh !important;
      flex-grow: 0 !important;
      flex-shrink: 0 !important;
      padding: 1.75rem 2rem 1.6rem !important;
      box-sizing: border-box !important;
      overflow: hidden !important;
      position: relative !important;
      z-index: 3 !important;
      pointer-events: auto !important;
      cursor: default !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: stretch !important;
      justify-content: flex-start !important;
    }
    .container::before {
      display: none !important;
    }
    .scrim {
      position: fixed !important;
      inset: 0 !important;
      background: rgba(0, 0, 0, 0.5) !important;
      backdrop-filter: blur(12px) !important;
      -webkit-backdrop-filter: blur(12px) !important;
      pointer-events: auto !important;
      cursor: pointer !important;
      z-index: 1 !important;
    }
    .scroller {
      overflow: visible !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: center !important;
      width: 100% !important;
    }
    .headline {
      padding: 0 !important;
      margin: 0 0 1.1rem 0 !important;
      display: flex !important;
      align-items: center !important;
      justify-content: center !important;
      position: relative !important;
      width: 100% !important;
      text-align: center !important;
    }
    .headline .text, h2, h2#headline, slot[name="headline"]::slotted(*) {
      width: 100% !important;
      text-align: center !important;
      justify-content: center !important;
      align-items: center !important;
      display: flex !important;
      font-family: 'Rajdhani', sans-serif !important;
      font-size: 1.55rem !important;
      font-weight: 700 !important;
      letter-spacing: 0.08em !important;
      color: #ffffff !important;
      text-transform: uppercase !important;
      padding: 0 !important;
      margin: 0 auto !important;
      box-sizing: border-box !important;
    }
    slot[name="headline"]::slotted(div) {
      width: 100% !important;
      text-align: center !important;
      justify-content: center !important;
      align-items: center !important;
      display: flex !important;
      padding: 0 !important;
      padding-right: 0 !important;
      padding-left: 0 !important;
      margin: 0 auto !important;
      box-sizing: border-box !important;
    }
    .headline ew-icon-button, slot[name="headline"]::slotted(ew-icon-button) {
      position: absolute !important;
      right: 0 !important;
      top: 50% !important;
      transform: translateY(-50%) !important;
      color: #94a3b8 !important;
    }
    .content {
      padding: 0 !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: center !important;
      text-align: center !important;
      width: 100% !important;
      color: #cbd5e1 !important;
    }
    slot[name="content"]::slotted(*) {
      width: 100% !important;
      display: flex !important;
      flex-direction: column !important;
      align-items: center !important;
      text-align: center !important;
      box-sizing: border-box !important;
    }
    .actions, slot[name="actions"]::slotted(*) {
      display: flex !important;
      justify-content: center !important;
      align-items: center !important;
      gap: 1.25rem !important;
      padding: 1.25rem 0 0.15rem 0 !important;
      margin: 0 !important;
      width: 100% !important;
    }
  `;
  ewDialog.shadowRoot.appendChild(innerStyle);

  // Handle clicking outside the popup window to close and return home
  const closePopup = () => {
    const ewt = ewDialog.closest('ewt-install-dialog') || document.querySelector('ewt-install-dialog');
    if (ewt && ewt._hasStartedFlashing) {
      return;
    }
    if (ewt && ewt._closeDialog) {
      ewt._closeDialog();
    } else {
      document.body.classList.remove('dialog-blur-active');
      if (ewDialog.close) ewDialog.close();
      const parent = ewDialog.getRootNode();
      if (parent && parent.host) parent.host.remove();
    }
  };

  const scrimEl = ewDialog.shadowRoot.querySelector('.scrim');
  if (scrimEl && !scrimEl._closeHooked) {
    scrimEl._closeHooked = true;
    scrimEl.addEventListener('click', (e) => {
      e.stopPropagation();
      e.preventDefault();
      closePopup();
    });
  }

  const dialogEl = ewDialog.shadowRoot.querySelector('dialog');
  if (dialogEl && !dialogEl._closeHooked) {
    dialogEl._closeHooked = true;
    dialogEl.addEventListener('click', (e) => {
      const container = ewDialog.shadowRoot.querySelector('.container');
      const path = e.composedPath();
      // If the click originated inside the card (or on slotted buttons like INSTALL), DO NOTHING!
      if (container && (e.target === container || path.includes(container))) {
        return;
      }
      e.stopPropagation();
      e.preventDefault();
      closePopup();
    });
  }
}

/**
 * Custom Dark Cyberpunk Theme Injection for ESP Web Tools Dialog
 */
function injectDialogStyles(dialog) {
  if (!dialog || !dialog.shadowRoot) return;

  if (!dialog.shadowRoot.querySelector('#clicker-dialog-theme')) {
    const style = document.createElement('style');
    style.id = 'clicker-dialog-theme';
    style.textContent = `
      :host {
        --md-dialog-container-color: #09090b !important;
        --md-dialog-headline-color: #fafafa !important;
        --md-dialog-supporting-text-color: #a1a1aa !important;
        --md-sys-color-surface-container-high: #09090b !important;
        --md-sys-color-surface: #09090b !important;
        --md-sys-color-on-surface: #fafafa !important;
        --md-sys-color-on-surface-variant: #a1a1aa !important;
        --md-sys-color-primary: #10b981 !important;
        --md-sys-color-outline: rgba(255, 255, 255, 0.22) !important;
        --md-circular-progress-active-indicator-color: #10b981 !important;
        --text-color: #fafafa !important;
        font-family: 'Rajdhani', 'Plus Jakarta Sans', 'Inter', -apple-system, sans-serif !important;
        pointer-events: auto !important;
      }
      ew-dialog {
        --md-dialog-container-color: #09090b !important;
        --md-dialog-container-shape: 22px !important;
        pointer-events: auto !important;
      }
      div[slot="headline"] {
        font-family: 'Rajdhani', sans-serif !important;
        font-size: 1.55rem !important;
        font-weight: 700 !important;
        color: #ffffff !important;
        letter-spacing: 0.08em !important;
        text-transform: uppercase !important;
        text-align: center !important;
        display: flex !important;
        align-items: center !important;
        justify-content: center !important;
        width: 100% !important;
        padding: 0 !important;
        padding-right: 0 !important;
        padding-left: 0 !important;
        margin: 0 auto !important;
        box-sizing: border-box !important;
      }
      ew-icon-button[slot="headline"] {
        position: absolute !important;
        right: 1.25rem !important;
        top: 1.25rem !important;
      }
      ew-icon-button[slot="headline"] svg {
        color: #94a3b8 !important;
        transition: color 0.2s ease !important;
      }
      ew-icon-button[slot="headline"]:hover svg {
        color: #10b981 !important;
      }
      ewt-page-progress {
        color: #a1a1aa !important;
        font-family: 'Rajdhani', 'JetBrains Mono', monospace !important;
        font-size: 1.25rem !important;
        display: flex !important;
        flex-direction: column !important;
        align-items: center !important;
        justify-content: center !important;
        padding: 2.4rem 1.5rem !important;
        text-align: center !important;
        gap: 1.25rem !important;
      }
      ew-circular-progress {
        --md-circular-progress-active-indicator-color: #10b981 !important;
        --md-circular-progress-size: 64px !important;
      }
      ewt-page-message {
        color: #fafafa !important;
        font-family: 'Rajdhani', sans-serif !important;
        font-size: 1.2rem !important;
        padding: 2.2rem 1.5rem !important;
        text-align: center !important;
      }
      ewt-page-message .icon {
        color: #10b981 !important;
        font-size: 2rem !important;
      }

      /* Centered Actions Buttons */
      div[slot="actions"] {
        display: flex !important;
        justify-content: center !important;
        align-items: center !important;
        gap: 1.25rem !important;
        width: 100% !important;
        padding-top: 0.5rem !important;
      }
      ew-text-button {
        --md-text-button-label-text-color: #ffffff !important;
        display: inline-flex !important;
        align-items: center !important;
        justify-content: center !important;
        font-family: 'Rajdhani', sans-serif !important;
        font-weight: 700 !important;
        font-size: 1.05rem !important;
        letter-spacing: 0.05em !important;
        text-transform: uppercase !important;
        pointer-events: auto !important;
        cursor: pointer !important;
        transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1) !important;
      }
      ew-text-button:first-child {
        --md-text-button-label-text-color: #94a3b8 !important;
        background: rgba(255, 255, 255, 0.05) !important;
        border: 1.5px solid rgba(255, 255, 255, 0.14) !important;
        border-radius: 12px !important;
        padding: 0.45rem 1.5rem !important;
        min-width: 110px !important;
      }
      ew-text-button:first-child:hover {
        --md-text-button-label-text-color: #ffffff !important;
        background: rgba(255, 255, 255, 0.1) !important;
        border-color: rgba(255, 255, 255, 0.3) !important;
      }
      ew-text-button:last-child {
        --md-text-button-label-text-color: #09090b !important;
        color: #09090b !important;
        background: #ffffff !important;
        border: 1.5px solid #ffffff !important;
        border-radius: 12px !important;
        padding: 0.45rem 2.2rem !important;
        min-width: 170px !important;
        box-shadow: 0 0 24px rgba(255, 255, 255, 0.18), inset 0 1px 0 rgba(255, 255, 255, 0.6) !important;
      }
      ew-text-button:last-child:hover {
        background: #f4f4f5 !important;
        border-color: #ffffff !important;
        filter: brightness(1.02) !important;
        box-shadow: 0 0 35px rgba(255, 255, 255, 0.35) !important;
        transform: translateY(-1px) !important;
      }

      /* Clean Minimal Version Pill Badge & Centered Content */
      .modal-cyber-badge {
        display: inline-flex;
        align-items: center;
        justify-content: center;
        gap: 0.5rem;
        background: rgba(16, 185, 129, 0.08);
        border: 1px solid rgba(16, 185, 129, 0.35);
        border-radius: 9999px;
        padding: 0.35rem 1.1rem;
        font-family: 'Rajdhani', sans-serif;
        font-size: 0.92rem;
        font-weight: 700;
        letter-spacing: 0.06em;
        color: #10b981;
        margin-bottom: 1.15rem;
      }
      .pulse-emerald-dot {
        width: 7px;
        height: 7px;
        border-radius: 50%;
        background: #10b981;
        box-shadow: 0 0 8px #10b981;
        animation: pulse-glow 2s infinite ease-in-out;
      }
      @keyframes pulse-glow {
        0%, 100% { opacity: 1; transform: scale(1); }
        50% { opacity: 0.4; transform: scale(0.85); }
      }
      .badge-ver {
        color: #ffffff;
      }

      .modal-instruction-text {
        font-family: 'Inter', -apple-system, sans-serif;
        font-size: 0.92rem;
        color: #cbd5e1;
        margin-bottom: 1.35rem;
        text-align: center;
        line-height: 1.5;
      }

      /* Spacious Mode Selector Cards */
      .modal-mode-wrapper {
        display: flex;
        flex-direction: column;
        gap: 0.85rem;
        width: 100%;
        margin-bottom: 1.35rem;
        text-align: left;
      }
      .modal-mode-option {
        display: flex;
        align-items: flex-start;
        gap: 1rem;
        padding: 1.1rem 1.25rem;
        background: rgba(255, 255, 255, 0.03);
        border: 1.5px solid rgba(255, 255, 255, 0.1);
        border-radius: 14px;
        cursor: pointer;
        transition: all 0.2s cubic-bezier(0.16, 1, 0.3, 1);
        user-select: none;
        pointer-events: auto !important;
        box-sizing: border-box;
      }
      .modal-mode-option:hover {
        background: rgba(255, 255, 255, 0.06);
        border-color: rgba(255, 255, 255, 0.25);
        transform: translateY(-1px);
      }
      .modal-mode-option.active {
        background: rgba(16, 185, 129, 0.09) !important;
        border-color: rgba(16, 185, 129, 0.6) !important;
        box-shadow: 0 0 25px rgba(16, 185, 129, 0.18), inset 0 0 15px rgba(16, 185, 129, 0.05) !important;
      }
      #modal-mode-factory.active {
        background: rgba(245, 158, 11, 0.09) !important;
        border-color: rgba(245, 158, 11, 0.6) !important;
        box-shadow: 0 0 25px rgba(245, 158, 11, 0.18), inset 0 0 15px rgba(245, 158, 11, 0.05) !important;
      }
      .modal-radio-dot {
        width: 22px !important;
        height: 22px !important;
        border-radius: 50% !important;
        border: 2px solid #52525b !important;
        margin-top: 2px !important;
        flex-shrink: 0 !important;
        display: flex !important;
        align-items: center !important;
        justify-content: center !important;
        box-sizing: border-box !important;
        transition: all 0.2s ease !important;
        background: transparent !important;
      }
      .modal-mode-option.active .modal-radio-dot {
        border-color: #10b981 !important;
      }
      #modal-mode-factory.active .modal-radio-dot {
        border-color: #f59e0b !important;
      }
      .modal-radio-dot::after {
        content: '' !important;
        display: none !important;
        width: 10px !important;
        height: 10px !important;
        border-radius: 50% !important;
        margin: 0 !important;
        padding: 0 !important;
        transition: all 0.2s ease !important;
      }
      .modal-mode-option.active .modal-radio-dot::after {
        display: block !important;
        background: #10b981 !important;
        box-shadow: 0 0 8px #10b981 !important;
      }
      #modal-mode-factory.active .modal-radio-dot::after {
        display: block !important;
        background: #f59e0b !important;
        box-shadow: 0 0 8px #f59e0b !important;
      }
      .modal-option-body {
        flex: 1;
      }
      .modal-mode-header {
        display: flex;
        align-items: center;
        justify-content: space-between;
        gap: 0.5rem;
        margin-bottom: 0.35rem;
      }
      .modal-mode-title {
        font-family: 'Rajdhani', sans-serif;
        font-size: 1.15rem;
        font-weight: 700;
        color: #ffffff;
        letter-spacing: 0.02em;
      }
      .modal-badge-rec {
        font-size: 0.7rem;
        font-weight: 700;
        font-family: 'Rajdhani', sans-serif;
        letter-spacing: 0.04em;
        text-transform: uppercase;
        background: rgba(16, 185, 129, 0.2);
        border: 1px solid rgba(16, 185, 129, 0.45);
        color: #10b981;
        padding: 3px 8px;
        border-radius: 9999px;
      }
      .modal-badge-warn {
        font-size: 0.7rem;
        font-weight: 700;
        font-family: 'Rajdhani', sans-serif;
        letter-spacing: 0.04em;
        text-transform: uppercase;
        background: rgba(245, 158, 11, 0.2);
        border: 1px solid rgba(245, 158, 11, 0.45);
        color: #f59e0b;
        padding: 3px 8px;
        border-radius: 9999px;
      }
      .modal-mode-desc {
        font-size: 0.84rem;
        color: #94a3b8;
        line-height: 1.45;
        font-family: 'Inter', -apple-system, sans-serif;
      }
      .modal-mode-desc code {
        font-family: 'JetBrains Mono', monospace;
        font-size: 0.8rem;
        color: #00f0ff;
        background: rgba(0, 240, 255, 0.1);
        padding: 1px 4px;
        border-radius: 4px;
      }
      .modal-footer-note {
        display: flex;
        align-items: center;
        justify-content: center;
        gap: 0.5rem;
        font-size: 0.8rem;
        color: #64748b;
        font-family: 'Inter', -apple-system, sans-serif;
      }
      .modal-footer-note svg {
        color: #10b981;
        flex-shrink: 0;
      }
    `;
    dialog.shadowRoot.appendChild(style);
  }

  const innerDialog = dialog.shadowRoot.querySelector('ew-dialog');
  if (innerDialog) {
    applyInnerDialogTheme(innerDialog);
  }
}

// Hook custom elements definitions so connecting window and dialogs get themed from frame 0
if (typeof customElements !== 'undefined') {
  customElements.whenDefined('ew-dialog').then(() => {
    const EwDialog = customElements.get('ew-dialog');
    if (EwDialog && EwDialog.prototype) {
      const origConnected = EwDialog.prototype.connectedCallback;
      EwDialog.prototype.connectedCallback = function () {
        if (origConnected) origConnected.call(this);
        applyInnerDialogTheme(this);
      };
      const origUpdate = EwDialog.prototype.update;
      if (origUpdate) {
        EwDialog.prototype.update = function (...args) {
          origUpdate.call(this, ...args);
          applyInnerDialogTheme(this);
        };
      }
    }
  });

  // Hook ewt-page-progress to completely eliminate 'This will take 2 minutes' misinformation
  customElements.whenDefined('ewt-page-progress').then(() => {
    const EwtPageProgress = customElements.get('ewt-page-progress');
    if (EwtPageProgress && EwtPageProgress.prototype) {
      const origUpdated = EwtPageProgress.prototype.updated;
      EwtPageProgress.prototype.updated = function (changedProps) {
        if (origUpdated) origUpdated.call(this, changedProps);
        if (this.shadowRoot) {
          const walker = document.createTreeWalker(this.shadowRoot, NodeFilter.SHOW_TEXT);
          let n;
          while ((n = walker.nextNode())) {
            if (/This will take|2 minutes|a minute|Keep this page visible|slow down/i.test(n.nodeValue)) {
              n.nodeValue = '';
            }
          }
        }
      };
      const origWillUpdate = EwtPageProgress.prototype.willUpdate;
      EwtPageProgress.prototype.willUpdate = function (changedProps) {
        if (this.label && typeof this.label === 'object') {
          this.label = '';
        } else if (typeof this.label === 'string' && /This will take|2 minutes|a minute|Keep this page visible|slow down/i.test(this.label)) {
          this.label = '';
        }
        if (origWillUpdate) origWillUpdate.call(this, changedProps);
      };
    }
  });

  customElements.whenDefined('ewt-install-dialog').then(() => {
    const EwtDialog = customElements.get('ewt-install-dialog');
    if (EwtDialog && EwtDialog.prototype) {
      // 0. Hook _renderProgress to completely remove the 2-minute misinformation
      const origRenderProgress = EwtDialog.prototype._renderProgress;
      if (origRenderProgress) {
        EwtDialog.prototype._renderProgress = function (label, progress) {
          let cleanLabel = (typeof label === 'string') ? label : '';
          if (/This will take|2 minutes|a minute|Keep this page visible|slow down/i.test(cleanLabel)) {
            cleanLabel = '';
          }
          return origRenderProgress.call(this, cleanLabel, progress);
        };
      }

      // 1. Hook willUpdate: automatically transition any initial DASHBOARD or ASK_ERASE to INSTALL (Confirm Installation)
      // BUT if flashing has completed, close dialog and return to main page
      const origWillUpdate = EwtDialog.prototype.willUpdate;
      EwtDialog.prototype.willUpdate = function (changedProps) {
        if (this._hasStartedFlashing && (this._state === 'DASHBOARD' || this._state === 'PROVISION' || this._flashDone)) {
          this._closeDialog();
          return;
        }

        if (!this._hasStartedFlashing && this._manifest && (this._state === 'DASHBOARD' || this._state === 'ASK_ERASE')) {
          this._state = 'INSTALL';
          this._installErase = false;
          this._installConfirmed = false;
        }
        if (origWillUpdate) origWillUpdate.call(this, changedProps);
      };

      // 2. Failsafe: Override dashboard renders
      EwtDialog.prototype._renderDashboardNoImprov = function () {
        if (this._hasStartedFlashing || this._flashDone) {
          this._closeDialog();
          return '';
        }
        this._state = 'INSTALL';
        this._installErase = false;
        this._installConfirmed = false;
        return this._renderInstall();
      };
      EwtDialog.prototype._renderDashboard = function () {
        if (this._hasStartedFlashing || this._flashDone) {
          this._closeDialog();
          return '';
        }
        this._state = 'INSTALL';
        this._installErase = false;
        this._installConfirmed = false;
        return this._renderInstall();
      };
      EwtDialog.prototype._renderAskErase = function () {
        if (this._hasStartedFlashing || this._flashDone) {
          this._closeDialog();
          return '';
        }
        this._state = 'INSTALL';
        this._installErase = false;
        this._installConfirmed = false;
        return this._renderInstall();
      };

      // Ensure turbo 460,800 baud is guaranteed on _confirmInstall call
      const origConfirmInstall = EwtDialog.prototype._confirmInstall;
      EwtDialog.prototype._confirmInstall = async function () {
        this._hasStartedFlashing = true;
        this._flashDone = false;
        this._finishHandled = false;
        if (window.esploader) {
          window.esploader.baudrate = 460800;
        }
        return origConfirmInstall.apply(this, arguments);
      };

      // Hook _closeDialog to guarantee blur removal and clean return to main page
      const origCloseDialog = EwtDialog.prototype._closeDialog;
      EwtDialog.prototype._closeDialog = function () {
        document.body.classList.remove('dialog-blur-active');
        if (origCloseDialog) {
          origCloseDialog.call(this);
        } else if (this.shadowRoot) {
          const ewDlg = this.shadowRoot.querySelector('ew-dialog');
          if (ewDlg) ewDlg.close();
        }
        this.remove();
      };

      EwtDialog.prototype._preventDefault = function (e) {
        if (e && e.preventDefault) e.preventDefault();
      };

      // 3. Hook updated: inject the interactive mode selector into Confirm Installation & handle completion
      const origUpdated = EwtDialog.prototype.updated;
      EwtDialog.prototype.updated = function(changedProps) {
        if (origUpdated) origUpdated.call(this, changedProps);
        injectDialogStyles(this);

        // Sanitize 'This will take 2 minutes' text inside ewt-page-progress
        if (this.shadowRoot) {
          const progressEl = this.shadowRoot.querySelector('ewt-page-progress');
          if (progressEl) {
            if (progressEl.label && typeof progressEl.label === 'object') {
              progressEl.label = '';
            }
            if (progressEl.shadowRoot) {
              const walker = document.createTreeWalker(progressEl.shadowRoot, NodeFilter.SHOW_TEXT);
              let n;
              while ((n = walker.nextNode())) {
                if (/This will take|2 minutes|a minute|Keep this page visible|slow down/i.test(n.nodeValue)) {
                  n.nodeValue = '';
                }
              }
            }
          }
        }

        // Auto-close dialog after flash completes and return to main flasher page
        if (this._hasStartedFlashing && this._installState && this._installState.state === 'finished') {
          this._flashDone = true;
          if (!this._finishHandled) {
            this._finishHandled = true;
            console.info('[Web Flasher] Installation complete! Returning to main page...');
            setTimeout(() => {
              this._closeDialog();
            }, 1200);
          }

          // In case user clicks the button before timeout
          const finishBtn = this.shadowRoot.querySelector('ew-text-button');
          if (finishBtn && !finishBtn._finishHooked) {
            finishBtn._finishHooked = true;
            finishBtn.textContent = 'Finish';
            finishBtn.addEventListener('click', (e) => {
              e.stopPropagation();
              e.preventDefault();
              this._closeDialog();
            }, true);
          }
          return;
        }

        if (!this._hasStartedFlashing && this._state === 'INSTALL' && !this._installConfirmed && this.shadowRoot) {
          const contentDiv = this.shadowRoot.querySelector('div[slot="content"]');
          if (contentDiv && !contentDiv.querySelector('#modal-mode-menu')) {
            const ver = (this._manifest && this._manifest.version) ? `v${this._manifest.version}` : 'v0.1.0';
            contentDiv.innerHTML = `
              <div class="modal-cyber-badge">
                <span class="pulse-emerald-dot"></span>
                <span class="badge-ver">${ver}</span>
              </div>

              <p class="modal-instruction-text">
                Select your preferred installation mode to begin flashing:
              </p>

              <div class="modal-mode-wrapper" id="modal-mode-menu">
                <div class="modal-mode-option ${!this._installErase ? 'active' : ''}" id="modal-mode-update">
                  <div class="modal-radio-dot"></div>
                  <div class="modal-option-body">
                    <div class="modal-mode-header">
                      <span class="modal-mode-title">Standard Update</span>
                      <span class="modal-badge-rec">Keep Stats</span>
                    </div>
                    <div class="modal-mode-desc">
                      Writes firmware directly to partition <code>0x10000</code>. Preserves your stored click counts, unlocked milestones, and local preferences.
                    </div>
                  </div>
                </div>

                <div class="modal-mode-option ${this._installErase ? 'active' : ''}" id="modal-mode-factory">
                  <div class="modal-radio-dot"></div>
                  <div class="modal-option-body">
                    <div class="modal-mode-header">
                      <span class="modal-mode-title">Factory Reset</span>
                      <span class="modal-badge-warn">Wipe Flash</span>
                    </div>
                    <div class="modal-mode-desc">
                      Performs complete chip wipe and re-flashes clean partition tables. Clears all stored counters and returns device to factory state.
                    </div>
                  </div>
                </div>
              </div>

              <div class="modal-footer-note">
                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <circle cx="12" cy="12" r="10"></circle>
                  <line x1="12" y1="16" x2="12" y2="12"></line>
                  <line x1="12" y1="8" x2="12.01" y2="8"></line>
                </svg>
                <span>Turbo upload speed active &bull; ~12 seconds completion</span>
              </div>
            `;

            const optUpdate = contentDiv.querySelector('#modal-mode-update');
            const optFactory = contentDiv.querySelector('#modal-mode-factory');

            if (optUpdate && optFactory) {
              optUpdate.addEventListener('click', (e) => {
                e.stopPropagation();
                this._installErase = false;
                optUpdate.classList.add('active');
                optFactory.classList.remove('active');
              });

              optFactory.addEventListener('click', (e) => {
                e.stopPropagation();
                this._installErase = true;
                optFactory.classList.add('active');
                optUpdate.classList.remove('active');
              });
            }

            const backBtn = this.shadowRoot.querySelector('ew-text-button:first-child');
            if (backBtn && !backBtn._backHooked) {
              backBtn._backHooked = true;
              backBtn.addEventListener('click', (e) => {
                e.stopPropagation();
                this._closeDialog();
              });
            }
          }
        }
      };

      // 4. Hook connectedCallback and disconnectedCallback for background blur
      const origConnected = EwtDialog.prototype.connectedCallback;
      EwtDialog.prototype.connectedCallback = function() {
        if (origConnected) origConnected.call(this);
        document.body.classList.add('dialog-blur-active');
        injectDialogStyles(this);
      };

      const origDisconnected = EwtDialog.prototype.disconnectedCallback;
      EwtDialog.prototype.disconnectedCallback = function() {
        if (origDisconnected) origDisconnected.call(this);
        document.body.classList.remove('dialog-blur-active');
      };
    }
  });
}

/**
 * Automatically probe candidate manifest locations to ensure the URL always resolves
 * regardless of whether served from project root or inside web_flasher/.
 */
async function detectWorkingReleasePrefix() {
  const candidates = [
    'releases/v0.1.0/manifest.json',
    '../releases/v0.1.0/manifest.json',
    './releases/v0.1.0/manifest.json',
    '/releases/v0.1.0/manifest.json',
    'manifest.json'
  ];

  for (const candidate of candidates) {
    try {
      const res = await fetch(candidate, { method: 'HEAD' });
      if (res.ok) {
        if (candidate.startsWith('../releases/')) {
          detectedPathPrefix = '../releases/';
        } else if (candidate.startsWith('releases/')) {
          detectedPathPrefix = 'releases/';
        } else if (candidate.startsWith('./releases/')) {
          detectedPathPrefix = './releases/';
        } else if (candidate.startsWith('/releases/')) {
          detectedPathPrefix = '/releases/';
        } else {
          detectedPathPrefix = '';
        }
        console.log(`Resolved releases path prefix: "${detectedPathPrefix}" using probe: ${candidate}`);
        return;
      }
    } catch (e) {
      // Continue checking next candidate
    }
  }
  detectedPathPrefix = '';
}

async function loadVersionRegistry() {
  const candidateUrls = [
    'versions.json',
    `${detectedPathPrefix}versions.json`,
    'releases/versions.json',
    '../releases/versions.json',
    './releases/versions.json'
  ];

  for (const url of candidateUrls) {
    try {
      const res = await fetch(url);
      if (res.ok) {
        const data = await res.json();
        if (data.releases && data.releases.length > 0) {
          availableReleases = data.releases.map(rel => {
            const tag = rel.tag || `v${rel.version}`;
            let manifestPath = rel.manifest || `releases/${tag}/manifest.json`;
            let binPath = rel.bin || `releases/${tag}/firmware.bin`;
            let factoryBinPath = rel.factory_bin || `releases/${tag}/factory_firmware.bin`;

            if (detectedPathPrefix && detectedPathPrefix !== 'releases/') {
              if (manifestPath.startsWith('releases/')) {
                manifestPath = detectedPathPrefix + manifestPath.substring('releases/'.length);
              }
              if (binPath.startsWith('releases/')) {
                binPath = detectedPathPrefix + binPath.substring('releases/'.length);
              }
              if (factoryBinPath.startsWith('releases/')) {
                factoryBinPath = detectedPathPrefix + factoryBinPath.substring('releases/'.length);
              }
            }

            return {
              ...rel,
              tag: tag,
              manifest: manifestPath,
              bin: binPath,
              factory_bin: factoryBinPath
            };
          });
          return;
        }
      }
    } catch (err) {
      // Try next
    }
  }

  availableReleases = DEFAULT_RELEASES;
}

function initVersionSelector() {
  const select = document.getElementById('version-select');
  if (!select) return;
  select.innerHTML = '';

  availableReleases.forEach(rel => {
    const opt = document.createElement('option');
    opt.value = rel.tag;
    opt.textContent = `${rel.tag} ${rel.is_latest ? '(Latest Stable)' : ''}`;
    select.appendChild(opt);
  });

  select.addEventListener('change', (e) => {
    updateSelectedVersion(e.target.value);
  });

  if (availableReleases.length > 0) {
    updateSelectedVersion(availableReleases[0].tag);
  }
}

function updateSelectedVersion(tag) {
  const rel = availableReleases.find(r => r.tag === tag) || availableReleases[0];

  // Update Meta labels
  const metaVer = document.getElementById('meta-version');
  const metaOffset = document.getElementById('meta-offset');
  const metaSize = document.getElementById('meta-size');

  if (metaVer) metaVer.textContent = rel.tag;
  if (metaOffset) metaOffset.textContent = '0x10000';
  if (metaSize) metaSize.textContent = rel.size ? `${Math.round(rel.size / 1024)} KB` : '~568 KB';

  // Construct absolute/resolved URL to ensure <esp-web-install-button> can always load it
  const manifestUrl = new URL(rel.manifest, window.location.href).href;

  // Update ESP Web Tools Install Button Manifest attribute & property
  const installBtn = document.getElementById('esp-install-btn');
  if (installBtn) {
    installBtn.setAttribute('manifest', manifestUrl);
    installBtn.manifest = manifestUrl;
  }

  // Update CLI command sample
  const cliSnippet = document.getElementById('cli-code-snippet');
  if (cliSnippet) {
    cliSnippet.textContent = `esptool.py --chip esp32 --baud 460800 --port COMx write_flash 0x0 releases/${rel.tag}/factory_firmware.bin`;
  }
}

function renderDownloadsTable() {
  const tbody = document.getElementById('releases-tbody');
  if (!tbody) return;

  tbody.innerHTML = availableReleases.map(rel => `
    <tr>
      <td>
        <span class="tag-badge ${rel.is_latest ? 'latest' : ''}">${rel.tag}</span>
        ${rel.is_latest ? '<span style="color:var(--accent-emerald);margin-left:0.5rem;font-size:0.8rem;font-weight:600;">LATEST</span>' : ''}
      </td>
      <td style="color:var(--text-secondary);font-family:var(--font-mono);font-size:0.85rem;">
        <strong style="color:var(--accent-cyan);">0x0</strong> (Merged Factory)
      </td>
      <td style="color:var(--text-muted);font-size:0.85rem;">${rel.factory_size ? `${Math.round(rel.factory_size / 1024)} KB` : '~420 KB'}</td>
      <td style="text-align:right;">
        <a href="${rel.factory_bin || 'factory_firmware.bin'}" download="clicker-${rel.tag}-factory.bin" class="btn-download" title="Download Merged Factory Binary for web.esphome.io / esptool at offset 0x0">
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"></path>
            <polyline points="7 10 12 15 17 10"></polyline>
            <line x1="12" y1="15" x2="12" y2="3"></line>
          </svg>
          <span>factory_firmware.bin</span>
        </a>
      </td>
    </tr>
  `).join('');
}

function initClipboardButtons() {
  document.querySelectorAll('.btn-copy').forEach(btn => {
    btn.addEventListener('click', () => {
      const targetId = btn.getAttribute('data-target');
      const targetEl = document.getElementById(targetId);
      if (targetEl) {
        navigator.clipboard.writeText(targetEl.textContent.trim()).then(() => {
          const originalText = btn.textContent;
          btn.textContent = 'Copied!';
          btn.style.background = 'var(--accent-emerald)';
          btn.style.color = '#000';
          setTimeout(() => {
            btn.textContent = originalText;
            btn.style.background = '';
            btn.style.color = '';
          }, 2000);
        });
      }
    });
  });
}

