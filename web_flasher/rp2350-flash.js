import { Picoboot } from './lib/picoboot/picoboot.js';
import { Target } from './lib/picoboot/target.js';
import { uf2ToFlashBuffer } from './lib/uf2.js';

let picoboot = null;

const RESET_REQUEST_BOOTSEL = 0x01;
const PICOBOOT_VID = 0x2e8a;

// Helper to find and configure a device by checking its interfaces
async function findDevice(devices, wantBootsel) {
    for (const d of devices) {
        if (d.vendorId !== PICOBOOT_VID) continue;
        try {
            await d.open();
            if (d.configuration === null || d.configuration.configurationValue !== d.configurations[0].configurationValue) {
                await d.selectConfiguration(d.configurations[0].configurationValue);
            }
            
            const info = getPicobootInterface(d);
            const isCurrentlyBootsel = (info.picobootIfNum !== -1 && !info.isAppMode);
            const isCurrentlyApp = (info.picobootIfNum !== -1 && info.isAppMode);
            
            if (wantBootsel && isCurrentlyBootsel) return d;
            if (!wantBootsel && isCurrentlyApp) return d;
            
            await d.close();
        } catch (e) {
            console.warn("Could not open/configure device", e);
        }
    }
    return null;
}

// Helper to check endpoints and find picoboot interface
function getPicobootInterface(device) {
    let picobootIfNum = -1;
    let inEp = null, outEp = null, inEpMaxPacketSize = 0;
    let isAppMode = false;
    
    for (const config of device.configurations) {
        for (const iface of config.interfaces) {
            for (const alt of iface.alternates) {
                if (alt.interfaceClass === 255) {
                    picobootIfNum = iface.interfaceNumber;
                    let hasBulk = false;
                    for (const ep of alt.endpoints) {
                        if (ep.type === 'bulk') {
                            hasBulk = true;
                            if (ep.direction === 'in') {
                                inEp = ep.endpointNumber;
                                inEpMaxPacketSize = ep.packetSize;
                            } else if (ep.direction === 'out') {
                                outEp = ep.endpointNumber;
                            }
                        }
                    }
                    if (!hasBulk) isAppMode = true;
                }
            }
        }
    }
    return { picobootIfNum, isAppMode, inEp, outEp, inEpMaxPacketSize };
}

document.addEventListener('DOMContentLoaded', () => {
    const flashBtn = document.getElementById('webusb-install-btn');
    const statusEl = document.getElementById('webusb-status');

    if (!flashBtn) return;

    if (!('usb' in navigator)) {
        flashBtn.disabled = true;
        if (statusEl) statusEl.textContent = 'WebUSB is not supported in this browser. Please use Chrome or Edge.';
        return;
    }

    flashBtn.addEventListener('click', async () => {
        try {
            flashBtn.disabled = true;

            // 0. Auto-sync lifetime telemetry & detect device name over serial BEFORE resetting into BOOTSEL!
            if ('serial' in navigator && window.syncDeviceTelemetry) {
                try {
                    let serialPort = null;
                    const permittedSerialPorts = await navigator.serial.getPorts();
                    if (permittedSerialPorts && permittedSerialPorts.length > 0) {
                        serialPort = permittedSerialPorts[0];
                    } else {
                        // Check if device is already in BOOTSEL mode
                        let existingUsbDevices = await navigator.usb.getDevices();
                        let alreadyInBootsel = false;
                        for (const d of existingUsbDevices) {
                            if (d.vendorId === PICOBOOT_VID) {
                                const pi = getPicobootInterface(d);
                                if (pi.picobootIfNum !== -1 && !pi.isAppMode) {
                                    alreadyInBootsel = true;
                                    break;
                                }
                            }
                        }

                        // If not in BOOTSEL, prompt user for serial port to sync stats and detect name
                        if (!alreadyInBootsel) {
                            if (statusEl) statusEl.textContent = 'Select your Clicker to auto-sync stats & detect name...';
                            try {
                                serialPort = await navigator.serial.requestPort();
                            } catch (pErr) {
                                console.info('[RP2350 Flasher] Serial selection skipped; proceeding with direct flash.');
                            }
                        }
                    }

                    if (serialPort) {
                        if (statusEl) statusEl.textContent = 'Auto-syncing device stats to global leaderboard...';
                        const nameInput = document.getElementById('device-name-input');
                        const customNewName = (window._userManuallyEditedName && nameInput) ? nameInput.value.trim() : null;
                        const stats = await window.syncDeviceTelemetry(serialPort, { customNewName });
                        if (stats && statusEl) {
                            const dName = stats.name || window._detectedHardwareName || 'Clicker';
                            const dClicks = Number(stats.clicks || 0);
                            statusEl.textContent = `✓ Auto-synced "${dName}" (${dClicks.toLocaleString()} clicks) to leaderboard! Preparing flash...`;
                            await new Promise(r => setTimeout(r, 600));
                        }
                    }
                } catch (syncErr) {
                    console.warn('[RP2350 Flasher] Auto-sync before flash error:', syncErr);
                }
            }

            if (statusEl) statusEl.textContent = 'Scanning USB bootloader...';
            
            let device = null;
            let info = null;

            // 1. Try to seamlessly find an already permitted device in BOOTSEL mode
            let permittedDevices = await navigator.usb.getDevices();
            device = await findDevice(permittedDevices, true);

            // 2. If no BOOTSEL device, try to seamlessly find an already permitted Application device and reset it
            if (!device) {
                const appDevice = await findDevice(permittedDevices, false);
                if (appDevice) {
                    const appInfo = getPicobootInterface(appDevice);
                    if (appInfo.picobootIfNum !== -1 && appInfo.isAppMode) {
                        if (statusEl) statusEl.textContent = 'Auto-resetting device to BOOTSEL...';
                        try {
                            await appDevice.claimInterface(appInfo.picobootIfNum);
                            await appDevice.controlTransferOut({
                                requestType: 'class', recipient: 'interface',
                                request: RESET_REQUEST_BOOTSEL, value: 0, index: appInfo.picobootIfNum
                            });
                        } catch (e) {
                            console.warn("Expected transfer error on reset:", e);
                        }
                        await appDevice.close();
                        
                        // Wait for it to reconnect as BOOTSEL
                        for (let i = 0; i < 15; i++) {
                            await new Promise(r => setTimeout(r, 200));
                            permittedDevices = await navigator.usb.getDevices();
                            device = await findDevice(permittedDevices, true);
                            if (device) break;
                        }
                    }
                }
            }

            // 3. If we STILL don't have a device (first time user), we must prompt them.
            if (!device) {
                if (statusEl) statusEl.textContent = 'Please select your device from the popup...';
                try {
                    device = await navigator.usb.requestDevice({ filters: [{ vendorId: PICOBOOT_VID }] });
                } catch (e) {
                    if (e.name === 'SecurityError' || (e.message && (e.message.includes('gesture') || e.message.includes('activation')))) {
                        flashBtn.disabled = false;
                        flashBtn.innerHTML = `
                            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                                <circle cx="12" cy="12" r="10"></circle>
                                <line x1="12" y1="8" x2="12" y2="12"></line>
                                <line x1="12" y1="16" x2="12.01" y2="16"></line>
                            </svg>
                            <span>Authorize Bootloader & Flash</span>
                        `;
                        if (statusEl) {
                            statusEl.innerHTML = `<span style="color:var(--accent-emerald, #34d399);">✓ Stats auto-synced to leaderboard!</span> Click the button above to authorize the USB bootloader and finish flashing.`;
                        }
                        return;
                    }
                    if (statusEl) statusEl.textContent = 'No device selected.';
                    flashBtn.disabled = false;
                    return;
                }
                
                await device.open();
                if (device.configuration === null || device.configuration.configurationValue !== device.configurations[0].configurationValue) {
                    await device.selectConfiguration(device.configurations[0].configurationValue);
                }
                
                info = getPicobootInterface(device);

                // If they selected an Application device, we reset it, but since they just consumed their user gesture,
                // we have to ask them to click again for the BOOTSEL prompt (Browser Security Policy).
                if (info.picobootIfNum !== -1 && info.isAppMode) {
                    if (statusEl) statusEl.textContent = 'Resetting to BOOTSEL...';
                    try {
                        await device.claimInterface(info.picobootIfNum);
                        await device.controlTransferOut({
                            requestType: 'class', recipient: 'interface',
                            request: RESET_REQUEST_BOOTSEL, value: 0, index: info.picobootIfNum
                        });
                    } catch (e) {
                        console.warn("Expected transfer error on reset:", e);
                    }
                    await device.close();
                    
                    // Loop to seamlessly find it as a BOOTSEL device (since the PID is the same, Chrome might just apply the permission automatically)
                    let bootDevice = null;
                    for (let i = 0; i < 20; i++) {
                        await new Promise(r => setTimeout(r, 200));
                        let devices = await navigator.usb.getDevices();
                        bootDevice = await findDevice(devices, true);
                        if (bootDevice) break;
                    }
                    
                    if (bootDevice) {
                        device = bootDevice;
                    } else {
                        // We try to auto-prompt for the BOOTSEL device.
                        // Because the PID is the same in both modes, you only need to grant permission once if it doesn't have a serial number.
                        // If Chrome requires a second permission due to a serial number change, this popup will catch it.
                        try {
                            device = await navigator.usb.requestDevice({ filters: [{ vendorId: PICOBOOT_VID }] });
                            await device.open();
                            if (device.configuration === null || device.configuration.configurationValue !== device.configurations[0].configurationValue) {
                                await device.selectConfiguration(device.configurations[0].configurationValue);
                            }
                        } catch (e) {
                            if (statusEl) statusEl.textContent = 'Device rebooted. Please click Flash AGAIN to grant permission to the BOOTSEL device.';
                            flashBtn.disabled = false;
                            return;
                        }
                    }
                }
            }

            // 4. We now have a BOOTSEL device!
            info = getPicobootInterface(device);
            if (info.picobootIfNum === -1 || info.isAppMode) {
                if (statusEl) statusEl.textContent = `Could not find BOOTSEL interface.`;
                await device.close();
                flashBtn.disabled = false;
                return;
            }

            const target = new Target('RP2350', device.vendorId, device.productId);
            picoboot = new Picoboot(device, target, info.picobootIfNum, info.outEp, info.inEp, info.inEpMaxPacketSize, {});

            if (statusEl) statusEl.textContent = 'Connecting...';
            await picoboot.connect();

            if (statusEl) statusEl.textContent = 'Downloading firmware.uf2...';
            const response = await fetch('firmware.uf2');
            if (!response.ok) throw new Error('Failed to fetch firmware.uf2');
            const uf2Buffer = await response.arrayBuffer();
            
            if (statusEl) statusEl.textContent = 'Parsing UF2...';
            const flashBuffer = uf2ToFlashBuffer(new Uint8Array(uf2Buffer));
            
            let finalBuffer = flashBuffer.data;

            // Read device name from UI input, window variable, or cached storage
            const nameInput = document.getElementById('device-name-input');
            const enteredName = nameInput ? nameInput.value.trim() : '';

            // Only patch if a custom name is explicitly provided and is NOT generic "CLICKER"
            let nameToUse = enteredName || window._customHardwareName || '';
            if (nameToUse.toUpperCase() === 'CLICKER') {
                nameToUse = '';
            }

            if (window.patchFirmwareCustomName && nameToUse) {
                console.info(`[RP2350 Flasher] Patching custom device name "${nameToUse}" into firmware image...`);
                const patched = await window.patchFirmwareCustomName(finalBuffer.buffer, nameToUse, false);
                finalBuffer = new Uint8Array(patched);
            } else {
                console.info('[RP2350 Flasher] No custom name change requested; firmware remains unpatched so existing NVS name or unique ID Click-<last4> is preserved.');
            }

            // Check optional Full Factory Wipe checkbox
            const wipeCheckbox = document.getElementById('factory-wipe-checkbox');
            const shouldFactoryWipe = Boolean(wipeCheckbox && wipeCheckbox.checked);

            if (shouldFactoryWipe) {
                if (statusEl) statusEl.textContent = 'Performing full factory wipe (erasing flash)...';
                console.info('[RP2350 Flasher] Factory wipe requested. Erasing 2MB flash space...');
                await picoboot.flashErase(0x10000000, 2 * 1024 * 1024);
            }

            if (statusEl) statusEl.textContent = 'Flashing firmware... Do not disconnect!';
            await picoboot.flashEraseAndWrite(flashBuffer.address, finalBuffer);

            if (statusEl) statusEl.textContent = 'Rebooting...';
            await picoboot.getConnection().reboot(500);
            
            // Persist custom name to local storage across reflashes
            if (nameToUse && nameToUse.toUpperCase() !== 'CLICKER') {
                localStorage.setItem('click_last_device_name', nameToUse);
                if (window._currentChipId) {
                    localStorage.setItem('click_name_' + window._currentChipId, nameToUse);
                }
            }

            if (statusEl) {
                statusEl.textContent = shouldFactoryWipe
                    ? 'Flash Complete! Clean factory install successful!'
                    : 'Flash Complete! Lifetime stats auto-synced to leaderboard & preserved!';
            }
        } catch (error) {
            console.error(error);
            if (statusEl) statusEl.textContent = 'Error: ' + error.message;
        } finally {
            if (picoboot) {
                try {
                    await picoboot.disconnect();
                } catch (e) {}
                picoboot = null;
            }
            flashBtn.disabled = false;
            flashBtn.innerHTML = `
              <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
                <circle cx="12" cy="12" r="10"></circle>
                <line x1="12" y1="8" x2="12" y2="12"></line>
                <line x1="12" y1="16" x2="12.01" y2="16"></line>
              </svg>
              <span>Connect & Flash Clicker (RP2354)</span>
            `;
        }
    });
});
