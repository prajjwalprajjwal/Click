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

// Session attempt tracking for CH340 driver error interception
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

  // 2. Observe DOM for esp-web-tools dialog errors
  const observer = new MutationObserver(() => {
    const dialog = document.querySelector('ewt-install-dialog');
    if (dialog && dialog.shadowRoot) {
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
  });

  observer.observe(document.body, { childList: true, subtree: true });
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
  if (metaSize) metaSize.textContent = rel.size ? `${Math.round(rel.size / 1024)} KB` : '~360 KB';

  // Construct absolute/resolved URL to ensure <esp-web-install-button> can always load it
  const manifestUrl = new URL(rel.manifest, window.location.href).href;

  // Update ESP Web Tools Install Button Manifest attribute & property
  const installBtn = document.getElementById('esp-install-btn');
  if (installBtn) {
    installBtn.setAttribute('manifest', manifestUrl);
    installBtn.setAttribute('baud-rate', '115200');
    installBtn.manifest = manifestUrl;
    installBtn.baudRate = 115200;
  }

  // Update CLI command sample
  const cliSnippet = document.getElementById('cli-code-snippet');
  if (cliSnippet) {
    cliSnippet.textContent = `esptool.py --chip esp32 --port COMx write_flash 0x0 releases/${rel.tag}/factory_firmware.bin`;
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
