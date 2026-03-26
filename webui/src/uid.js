import { toast, exec, spawn, listPackages, getPackagesInfo } from 'kernelsu-alt';
import { modDir, bin, ksuDir, uidFile, versionFile} from './index.js';

let manager = [];
let currentUid = null;

async function getKsuManager() {
    return new Promise((resolve) => {
        let packages = [];
        const result = spawn(`
            abi=$(getprop ro.bionic.arch)
            pm list packages -3 | cut -d: -f2 | busybox xargs -n1 -P$(busybox nproc) sh -c '
                dir=$(pm path "$1" 2>/dev/null | sed -n "1s/^package://p" | busybox xargs dirname)
                [ -f "$dir/lib/'"$abi"'/libksud.so" ] && echo "$1"
            ' sh
        `, [], { env: { PATH: `$PATH:${ksuDir}/bin` }});
        result.stdout.on('data', (data) => packages.push(data.trim()));
        result.on('exit', async () => {
            try {
                const pmPackages = await listPackages("user");
                packages = packages.filter(val => pmPackages.includes(val)).sort();
                const pkgInfos = await getPackagesInfo(packages);
                pkgInfos.forEach(pkg => {
                    manager.push({
                        packageName: pkg.packageName,
                        appLabel: pkg.appLabel,
                        uid: pkg.uid,
                        versionCode: pkg.versionCode
                    });
                });
            } catch (e) {
                // Vite debug
                if (import.meta.env.DEV) {
                    manager = [
                        { packageName: "me.weishu.kernelsu", appLabel: "KernelSU", uid: 10006 },
                        { packageName: "com.kowx712.supermanager", appLabel: "KowSU", uid: 10007 }
                    ];
                }
            }
            resolve();
        });
    });
}

async function getCurrentUid() {
    await exec(`${bin} --getuid`, { env: { PATH: `$PATH:${modDir}` }}).then((result) => {
        if (result.errno !== 0 || result.stdout.trim() === '') return;
        currentUid = result.stdout.trim();
    }).catch(() => { });
}

async function setManager(uid, manager) {
    await exec(
        `${bin} --setuid ${uid} && { kill -9 $(busybox pidof ${manager}) || true; }`,
        { env: { PATH: `$PATH:${modDir}:${ksuDir}/bin` }}
    ).then((result) => {
        if (result.errno !== 0) {
            toast("Failed to crown manager: " + result.stderr);
        } else {
            toast("Success, root access might no longer avaible in current window.");
        }
    }).catch(() => { });
}

function saveManager(uid) {
    const cmd = uid ? `echo ${uid} >` : `rm -rf ${versionFile}`;
    exec(`${cmd} ${uidFile}`).then((result) => {
        if (result.errno !== 0) toast("Failed to save manager_uid: " + result.stderr);
    }).catch(() => { });
}

export { manager, currentUid, getKsuManager, getCurrentUid, setManager, saveManager };
