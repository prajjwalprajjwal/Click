(() => {
  const UF2_BLOCK_SIZE = 512;
  const UF2_MAGIC_START0 = 0x0a324655;
  const UF2_MAGIC_START1 = 0x9e5d5157;
  const UF2_MAGIC_END = 0x0ab16f30;
  const UF2_FLAG_FAMILY_ID = 0x00002000;
  const RP2350_FAMILY_ID = 0xe48bff59;

  document.addEventListener('DOMContentLoaded', () => {
    const button = document.getElementById('rp2350-flash-button');
    const status = document.getElementById('rp2350-flash-status');
    if (!button || !status) return;

    const copyButton = document.getElementById('rp2350-picotool-copy');
    const picotoolCommand = document.getElementById('rp2350-picotool-command');
    if (copyButton && picotoolCommand) {
      copyButton.addEventListener('click', async () => {
        try {
          await navigator.clipboard.writeText(picotoolCommand.textContent.trim());
          setStatus(status, 'Picotool command copied. Run it on this computer with click-rp2350.uf2 beside it.', 'success');
        } catch (error) {
          setStatus(status, 'Clipboard access is unavailable. Use the desktop helper download or select the command to copy it.', 'notice');
        }
      });
    }

    if (!window.isSecureContext || typeof window.showDirectoryPicker !== 'function') {
      button.disabled = true;
      setStatus(status, 'Use desktop Chrome or Edge over HTTPS or localhost to flash directly. The manual UF2 download remains available.', 'notice');
      return;
    }

    button.addEventListener('click', () => flashRp2350(button, status));
  });

  async function flashRp2350(button, status) {
    button.disabled = true;

    try {
      setStatus(status, 'Select the mounted RP2350 BOOTSEL drive.', 'working');
      const directory = await window.showDirectoryPicker({ mode: 'readwrite' });
      const infoHandle = await directory.getFileHandle('INFO_UF2.TXT');
      const info = await (await infoHandle.getFile()).text();

      if (!/RP2350/i.test(info)) {
        throw new Error('The selected drive does not identify itself as an RP2350 boot volume.');
      }

      setStatus(status, 'Checking the RP2350 firmware image…', 'working');
      const firmwareUrl = new URL(button.dataset.firmwareUrl || 'firmware.uf2', window.location.href);
      const response = await fetch(firmwareUrl, { cache: 'no-store' });
      if (!response.ok) {
        throw new Error(`Firmware download failed (${response.status}).`);
      }

      const firmware = await response.arrayBuffer();
      validateRp2350Uf2(firmware);

      setStatus(status, `Writing firmware to ${directory.name}…`, 'working');
      const firmwareHandle = await directory.getFileHandle('click-rp2350.uf2', { create: true });
      const writable = await firmwareHandle.createWritable();
      try {
        await writable.write(firmware);
        await writable.close();
      } catch (error) {
        await writable.abort().catch(() => {});
        throw error;
      }

      setStatus(status, 'Transfer complete. The RP2350 bootloader should install the UF2 and reboot the Clicker.', 'success');
    } catch (error) {
      if (error.name === 'AbortError') {
        setStatus(status, 'Drive selection cancelled. No firmware was written.', 'notice');
      } else {
        console.error('[RP2350 Flasher]', error);
        setStatus(status, error.message || 'RP2350 flashing failed. Download the UF2 and copy it to the BOOTSEL drive manually.', 'error');
      }
    } finally {
      button.disabled = false;
    }
  }

  function validateRp2350Uf2(buffer) {
    if (buffer.byteLength === 0 || buffer.byteLength % UF2_BLOCK_SIZE !== 0) {
      throw new Error('The RP2350 UF2 image has an invalid block size.');
    }

    const view = new DataView(buffer);
    const blockCount = buffer.byteLength / UF2_BLOCK_SIZE;

    for (let offset = 0; offset < buffer.byteLength; offset += UF2_BLOCK_SIZE) {
      const flags = view.getUint32(offset + 8, true);
      if (view.getUint32(offset, true) !== UF2_MAGIC_START0 ||
          view.getUint32(offset + 4, true) !== UF2_MAGIC_START1 ||
          view.getUint32(offset + 24, true) !== blockCount ||
          !(flags & UF2_FLAG_FAMILY_ID) ||
          view.getUint32(offset + 28, true) !== RP2350_FAMILY_ID ||
          view.getUint32(offset + 508, true) !== UF2_MAGIC_END) {
        throw new Error('The selected firmware is not a valid RP2350 UF2 image.');
      }
    }
  }

  function setStatus(element, message, state) {
    element.textContent = message;
    element.dataset.state = state;
  }
})();