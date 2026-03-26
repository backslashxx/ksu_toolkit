import { exec, toast } from 'kernelsu-alt';
import { bin, modDir, unameFile } from './index.js'

async function getUname() {
    if (import.meta.env.DEV) { // vite debug
        document.getElementById('uname-release').value = "6.18.2-spoofed";
        document.getElementById('uname-version').value = "#1 SMP PREEMPT_DYNAMIC Thu, 18 Dec 2025 18:00:18 +0000";
    }
    exec(`echo "RELEASE=$(uname -r)" && echo "VERSION=$(uname -v)"`).then((result) => {
        if (result.errno !== 0 || result.stdout.trim() === '') return;
        result.stdout.trim().split('\n').forEach((line) => {
            if (line.startsWith('RELEASE=')) {
                document.getElementById('uname-release').value = line.split('=').slice(1).join('');
            } else if (line.startsWith('VERSION=')) {
                document.getElementById('uname-version').value = line.split('=').slice(1).join('');
            }
        });
    }).catch(() => { });
}

async function applyUname(newRelease, newVersion) {
    const result = await exec(`${bin} --fkuname "${newRelease}" "${newVersion}"`, { env: { PATH: `$PATH:${modDir}` } });
    toast(result.errno === 0 ? result.stdout : result.stderr);
    if (result.errno !== 0) return;

    const setOnBoot = document.getElementById('uname-set-on-boot').selected;

    let cmd;
    if (setOnBoot && (newRelease !== "default" || newVersion !== "default")) {
        cmd = `printf "RELEASE=\\"${newRelease}\\"\nVERSION=\\"${newVersion}\\"" >`;
    } else {
        cmd = 'rm -rf';
    }
    exec(`${cmd} ${unameFile}`);
}

function initListeners() {
    const applyBtn = document.getElementById('uname-apply');
    const resetBtn = document.getElementById('uname-reset');
    const setOnBootBtn = document.getElementById('uname-set-on-boot');

    exec(`grep -qE "RELEASE=|VERSION=" ${unameFile}`).then((result) => {
        setOnBootBtn.selected = result.errno === 0;
    }).catch(() => { });

    async function applyHandler(release, version) {
        if (!release) release = document.getElementById('uname-release').value;
        if (!version) version = document.getElementById('uname-version').value;

        await applyUname(release, version);
        await getUname();
    }

    applyBtn.onclick = () => {
        applyHandler();
    };
    resetBtn.onclick = () => {
        applyHandler("default", "default");
    };
    setOnBootBtn.addEventListener('change', () => {
        applyHandler();
    });
}

export { getUname, initListeners };
