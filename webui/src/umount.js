import { exec } from 'kernelsu-alt';
import { modDir, bin, ksuDir, ksud, umountEntryFile } from './index.js';

let umountProvider = 'none';
let umountList = [], umountedList = [], mountEntryList = [];

async function getUmountList() {
    // Vite debug
    if (import.meta.env.DEV) {
        umountList = ["/system/etc/hosts", "/system/bin/su"];
        return;
    }
    await exec(`cat ${umountEntryFile}`).then((result) => {
        if (result.errno !== 0 || result.stdout.trim() === '') return;
        umountList.length = 0;
        const lines = result.stdout.trim().split('\n');
        lines.forEach(line => {
            if (!umountList.includes(line)) umountList.push(line)
        });
    }).catch(() => { });
}

async function getUmountedList() {
    // Vite debug
    if (import.meta.env.DEV) {
        umountedList = ["/system/etc/hosts"];
        return;
    }
    await exec(`${bin} --getlist`, { env: { PATH: `$PATH:${modDir}` }}).then((result) => {
        if (result.stdout.trim() === '') return;
        umountedList.length = 0;
        const lines = result.stdout.trim().split('\n');
        lines.forEach(line => {
            if (!umountedList.includes(line)) umountedList.push(line)
        });
    }).catch(() => { });
}

async function getMountEntryList() {
    // Vite debug
    if (import.meta.env.DEV) {
        mountEntryList = [
            { source: "overlay", mount_point: "/system/app", fs_type: "overlay", options: "ro", dump: 0, pass: 0 },
            { source: "KSU", mount_point: "/system/etc/hosts", fs_type: "overlay", options: "ro", dump: 0, pass: 0 },
        ];
        return;
    }

    const decodeEscapes = (str) => {
        return str.replace(/\\([0-7]{3})/g, (match, octal) => {
            return String.fromCharCode(parseInt(octal, 8));
        });
    };

    await exec(`cat /proc/1/mounts`).then((result) => {
        if (result.stdout.trim() === '') return;
        mountEntryList.length = 0;
        const lines = result.stdout.trim().split('\n').filter(line => line.length > 0);
        lines.forEach(line => {
            const fields = line.split(/\s+/, 6);
            if (fields.length !== 6) return;
            const [ source, mount_point, fs_type, options, dump_str, pass_str ] = fields;
            const mountEntry = {
                source: decodeEscapes(source),
                mount_point: decodeEscapes(mount_point),
                fs_type: decodeEscapes(fs_type),
                options: options.split(','),
                dump: parseInt(dump_str, 10),
                pass: parseInt(pass_str, 10)
            };
            mountEntryList.push(mountEntry);
        });
    }).catch(() => { });
}

async function getUmountProvider() {
    const znctl = "/data/adb/modules/zygisksu/bin/zygiskd";

    await exec(`
        ${znctl} status | grep enforce_denylist || true
        grep -i "Welcome to" ${ksuDir}/logcat.log || true
    `).then((result) => {
        const output = result.stdout.trim();
        if (import.meta.env.DEV) { // Vite debug
            umountProvider = "zygisknext";
        } else if (output.includes("enforce_denylist")) {
            if (parseInt(output.split(':')[1] !== 0)) {
                umountProvider = "zygisknext";
            }
        } else if (output.tolower().includes("neozygisk")) {
            umountProvider = "neozygisk";
        } else if (output.tolower().includes("rezygisk")) {
            umountProvider = "rezygisk";
            document.getElementById('rezygisk').style.display = block;
        }
    }).catch(() => { });
}

async function addUmount(entry) {
    if (umountList.includes(entry)) return;
    umountList.push(entry);
    await exec(`echo "${umountList.join('\n')}" > ${umountEntryFile}`).catch(() => { });
    if (!umountedList.includes(entry)) await umount('add', entry);
    await getUmountList();
    await getUmountedList();
}

async function removeUmount(entry) {
    if (!umountList.includes(entry)) return;
    umountList.pop(entry);
    let cmd;
    if (umountList.length === 0) {
        cmd = 'rm -rf';
    } else {
        cmd = `echo "${umountList.join('\n')}" >`;
    }
    await exec(`${cmd} ${umountEntryFile}`).catch(() => { });
    if (umountedList.includes(entry)) await umount('del', entry);
    await getUmountList();
    await getUmountedList();
}

async function umount(action, entry) {
    const cmd = action === 'add' ? `add ${entry} -f 2 && ${ksud} kernel notify-module-mounted` : `del ${entry}`;
    await exec(`${ksud} kernel umount ${cmd}`).then((result) => {
        if (result.errno !== 0) toast(result.stderr);
    }).catch(() => { });
}

export { umountProvider, umountList, umountedList, mountEntryList, getUmountList, getUmountedList, getMountEntryList, getUmountProvider, addUmount, removeUmount };
