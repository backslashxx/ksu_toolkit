import { exec, toast } from 'kernelsu-alt';
import { modDir, bin, ksud } from './index.js';

// KernelSU Version
function setupKsuVersionLogic() {
    const display = document.getElementById('current-ksu-version');
    const input = document.querySelector('#ksu-version md-outlined-text-field');
    const btnDone = document.getElementById('spoof-ksuver-done');
    const btnUndo = document.getElementById('spoof-ksuver-undo');

    const updateDisplay = () => {
        exec(`${ksud} --version && ${ksud} debug version`).then(res => {
            const output = res.stdout.trim();

            const versionMatch = output.match(/ksud\s+([0-9.]+)/);
            const semVer = versionMatch ? versionMatch[1] : "Unknown";

            const codeMatch = output.match(/Kernel Version:\s+(\d+)/);
            const buildCode = codeMatch ? codeMatch[1] : "???";

            display.textContent = `v${semVer} (${buildCode})`;

        }).catch((err) => {
            console.error(err);
            display.textContent = "Unknown";
        });
    };

    // Button Check: Apply version
    btnDone.onclick = () => {
        const val = input.value.trim();
        if (!val) return toast("Please enter a version code");

        exec(`${bin} --setver ${val}`, { env: { PATH: `$PATH:${modDir}` }}).then(res => {
            toast(res.errno === 0 ? "KernelSU Version Applied!" : res.stderr);
            setTimeout(updateDisplay, 500);
        });
    };

    // Button Undo: Reset to default
    btnUndo.onclick = () => {
        exec(`rm /data/adb/ksu/.manager_version; ${bin} --setver`, { env: { PATH: `$PATH:${modDir}` }}).then(() => {
            toast("KernelSU Version Reset!");
            input.value = "";
            setTimeout(updateDisplay, 500);
        });
    };

    updateDisplay(); // load at init
}

// Kernel Uname
function setupUnameLogic() {
    const displayBox = document.getElementById('uname-display-box');
    const inputRelease = document.getElementById('input-release');
    const inputVersion = document.getElementById('input-version');

    // Release buttons
    const btnReleaseDone = document.getElementById('input-release-done');
    const btnReleaseUndo = document.getElementById('input-release-undo');

    // Build buttons
    const btnVersionDone = document.getElementById('input-version-done');
    const btnVersionUndo = document.getElementById('input-version-undo');

    let realRelease = "";
    let realVersion = "";

    const updateDisplay = () => {
        exec(`uname -r && uname -v`).then(res => {
            const lines = res.stdout.trim().split('\n');
            if (lines.length >= 2) {
                realRelease = lines[0];
                realVersion = lines[1];
                displayBox.innerHTML = `
                    <div style="font-size: 0.9em;"><b>R:</b> ${realRelease}</div>
                    <div style="font-size: 0.9em; opacity: 0.8; margin-top: 4px;"><b>B:</b> ${realVersion}</div>
                `;
            }
        });
    };

    // Apply logic
    // No matter which button you press, it reads BOTH fields.
    // If one is empty, it uses the actual system value to avoid breaking anything
    const applySpoof = () => {
        let r = inputRelease.value.trim();
        let v = inputVersion.value.trim();

        if (r === "") r = realRelease; // If it empty, keep the current one
        if (v === "") v = realVersion; // If it empty, keep the current one

        exec(`${bin} --fkuname "${r}" "${v}"`, { env: { PATH: `$PATH:${modDir}` }}).then(res => {
            if (res.errno === 0) {
                toast("Kernel Uname Spoofed!");
                setTimeout(updateDisplay, 500);
            } else {
                toast("Error: " + res.stderr);
            }
        });
    };

    // Reset logic
    const resetSpoof = () => {
        exec(`${bin} --fkuname default default`, { env: { PATH: `$PATH:${modDir}` }}).then(() => {
            toast("Kernel Uname Reset!");
            inputRelease.value = "";
            inputVersion.value = "";
            setTimeout(updateDisplay, 500);
        });
    };

    // We connected the buttons
    if (btnReleaseDone) btnReleaseDone.onclick = applySpoof;
    if (btnVersionDone) btnVersionDone.onclick = applySpoof;

    if (btnReleaseUndo) btnReleaseUndo.onclick = resetSpoof;
    if (btnVersionUndo) btnVersionUndo.onclick = resetSpoof;

    updateDisplay(); // load at init
}

// init
export function initSpoofing() {
    setupKsuVersionLogic();
    setupUnameLogic();
}