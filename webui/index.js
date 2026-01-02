import { exec, spawn, toast } from 'kernelsu-alt';
import '@material/web/chips/chip-set.js';
import '@material/web/chips/filter-chip.js';
import '@material/web/fab/fab.js';
import '@material/web/icon/icon.js';
import '@material/web/iconbutton/icon-button.js';
import '@material/web/iconbutton/filled-icon-button.js';
import '@material/web/iconbutton/outlined-icon-button.js';
import '@material/web/menu/menu.js';
import '@material/web/menu/menu-item.js';
import '@material/web/tabs/primary-tab.js';
import '@material/web/radio/radio.js';
import '@material/web/ripple/ripple.js';
import '@material/web/switch/switch.js';
import '@material/web/tabs/tabs.js';
import '@material/web/textfield/outlined-text-field.js';
import * as uidModule from './uid.js';
import * as umountModule from './umount.js';
import * as sulogModule from './sulog.js';
import * as spoofModule from './spoof.js';

export const modDir = '/data/adb/modules/ksu_toolkit';
export const bin = 'toolkit';
export const ksud = "/data/adb/ksud";
export const ksuDir = '/data/adb/ksu';

export const uidFile = ksuDir + "/.manager_uid";
export const versionFile = ksuDir + "/.manager_version";
export const umountEntryFile = ksuDir + "/.umount_list";

// Manager uid crown
function appendManagerList() {
    const managerList = document.getElementById('manager-list');
    managerList.innerHTML = '';
    if (uidModule.manager.length === 0) document.getElementById('manager-empty').classList.add('active');
    uidModule.manager.forEach(item => {
        const listItem = document.createElement('div');
        listItem.className = 'list-item';
        listItem.innerHTML = `
            <label for="${item.packageName}">
                <img class="app-icon" src="ksu://icon/${item.packageName}" />
                <div class="app-info">
                    <span class="app-label">
                        ${item.appLabel}
                        <div class="app-uid">${item.uid}</div>
                    </span>
                    <span class="package-name">${item.packageName} (${item.versionCode})</span>
                </div>
            </label>
            <md-radio id="${item.packageName}" name="manager-group" value="${item.uid.toString()}" version="${item.versionCode}"></md-radio>
            <md-ripple></md-ripple>
        `;
        if (uidModule.currentUid && item.uid == uidModule.currentUid) {
            listItem.querySelector('md-radio').checked = true;
        }
        listItem.querySelector('md-radio').addEventListener('change', () => {
            document.getElementById('ksu-version').querySelector('md-outlined-text-field').value = item.versionCode;
        });
        managerList.append(listItem);
    });
}

function setupUidPageListener() {
    const saveSwitch = document.getElementById('save');
    const crownBtn = document.getElementById('crown');

    if (uidModule.manager.length === 0) {
        saveSwitch.selected = false;
        uidModule.saveManager();
    } else {
        saveSwitch.disabled = false;
        exec(`cat ${uidFile}`).then((result) => {
            saveSwitch.selected = result.stdout.trim() !== '';
        }).catch(() => { });
    }

    saveSwitch.addEventListener('change', () => {
        if (saveSwitch.selected) {
            document.querySelectorAll('md-radio').forEach(radio => {
                if (!radio.checked) return;
                uidModule.saveManager(radio.value);
            });
        } else {
            uidModule.saveManager();
        }
    });

    crownBtn.classList.add('show');
    crownBtn.onclick = () => {
        document.querySelectorAll('md-radio').forEach(radio => {
            if (!radio.checked) return;
            exec("am start -a android.intent.action.MAIN -c android.intent.category.HOME").catch(() => { });
            uidModule.saveManager(saveSwitch.selected ? radio.value : null);
            uidModule.setManager(radio.value, radio.id);
            crownBtn.classList.add('hide');
            document.getElementById('exit-btn').click();
        });
    }
}

function checkUidFeature() {
    exec(
        `${bin} --setuid $(${bin} --getuid) || exit 1`,
        { env: { PATH: `$PATH:${modDir}` }}
    ).then((result) => {
        document.getElementById('manager-loading').classList.remove('active');
        if (result.errno !== 0 && !import.meta.env.DEV) {
            document.getElementById('crown-unsupported').classList.add('active');
            return;
        }
        appendManagerList();
        setupUidPageListener();
    }).catch(() => { });
}

// Kernel umount
function appendUmountList() {
    const umountEntryList = document.getElementById('umount-list');
    umountEntryList.innerHTML = '';
    document.getElementById('umount-empty').classList.toggle('active', umountModule.umountList.length === 0);
    umountModule.umountList.forEach(item => {
        const listItem = document.createElement('div');
        listItem.className = 'list-item';
        listItem.innerHTML = `
            <div class="indicator">
                <svg viewBox="0 0 6.35 6.35" xmlns="http://www.w3.org/2000/svg"><circle cx="3.17" cy="3.17" r="1.09"/></svg>
            </div>
            <div class="mount-entry">
                <div>${item}</div>
                <div class="reminder"></div>
            </div>
            <md-outlined-icon-button class="remove-btn">
                <md-icon><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 -960 960 960"><path d="M280-120q-33 0-56.5-23.5T200-200v-520h-40v-80h200v-40h240v40h200v80h-40v520q0 33-23.5 56.5T680-120H280Zm400-600H280v520h400v-520ZM360-280h80v-360h-80v360Zm160 0h80v-360h-80v360ZM280-720v520-520Z"/></svg></md-icon>
            </md-outlined-icon-button>
        `;
        if (!umountModule.umountedList.includes(item)) {
            listItem.querySelector('.indicator').classList.add('inactive');
        }
        const entry = umountModule.mountEntryList.find(e => e.mount_point === item && e.source === 'KSU');
        if (entry && umountModule.umountProvider !== 'none') {
            let provider;
            if (umountModule.umountProvider === 'zygisknext') {
                provider = 'ZygiskNext';
            } else if (umountModule.umountProvider === 'neozygisk') {
                provider = 'NeoZygisk';
            }
            listItem.querySelector('.reminder').textContent = `${provider} is likely handling this entry.`;
        }
        listItem.querySelector('.remove-btn').onclick = async () => {
            await umountModule.removeUmount(item);
            appendUmountList();
        }
        umountEntryList.append(listItem);
    });
}

function setupSeachOption() {
    const searchBox = document.getElementById('mount-entry-search');
    const seach = searchBox.querySelector('md-outlined-text-field');
    const addBtn = searchBox.querySelector('.text-field-button');
    const menu = searchBox.querySelector('md-menu');

    menu.defaultFocus = '';
    menu.skipRestoreFocus = true;
    menu.anchorElement = seach;

    const options = Array.from(
        new Set(umountModule.mountEntryList
            .map(entry => entry.mount_point)
            .filter(opt => !umountModule.umountedList.includes(opt))
        )
    ).sort();
    options.forEach(opt => {
        const menuItem = document.createElement('md-menu-item');
        menuItem.dataset.option = opt;
        menuItem.textContent = opt;
        menuItem.addEventListener('click', () => {
            seach.value = opt;
            menu.close();
        });
        menu.appendChild(menuItem);
    });

    const filterMenuItems = (value) => {
        const needle = value.toLowerCase();
        let visible = 0;
        menu.querySelectorAll('md-menu-item').forEach(mi => {
            const opt = (mi.dataset.option || '').toLowerCase();
            const show = opt.includes(needle) && opt !== needle;
            mi.style.display = show ? '' : 'none';
            if (show) visible++;
        });

        if (visible > 0) {
            menu.show();
        } else {
            menu.close();
        }
    }

    seach.addEventListener('input', (event) => filterMenuItems(event.target.value));
    seach.addEventListener('focus', (event) => {
        setTimeout(() => {
            if (document.activeElement === seach) filterMenuItems(event.target.value);
        }, 100);
    });

    addBtn.onclick = async () => {
        if (seach.value.trim() === '') return;
        await umountModule.addUmount(seach.value);
        seach.value = '';
        appendUmountList();
    };
}

function setupUmountPageListener() {
    const kernelUmountSwitch = document.getElementById('kernel-umount');
    const seachBox = document.getElementById('mount-entry-search');

    if (umountModule.umountProvider === "rezygisk") {
        kernelUmountSwitch.selected = false;
        document.getElementById('rezygisk').classList.add('active');
    } else {
        kernelUmountSwitch.disabled = false;
        seachBox.removeAttribute('hidden');
        exec(`${ksud} feature get kernel_umount | grep Status`).then((result) => {
            kernelUmountSwitch.selected = result.stdout.includes('enabled');
        }).catch(() => { });
    }

    kernelUmountSwitch.addEventListener('change', () => {
        const state = kernelUmountSwitch.selected ? '1' : '0';
        exec(`${ksud} feature set kernel_umount ${state} && ${ksud} feature save`).catch(() => { });
    });
}

function checkUmountFeature() {
    exec(`${bin} --getlist`, { env: { PATH: `$PATH:${modDir}` }}).then((result) => {
        if (result.stderr.trim() === 'fail' && !import.meta.env.DEV) {
            document.getElementById('umount-unsupported').classList.add('active');
            return;
        }
        appendUmountList();
        setupSeachOption();
        setupUmountPageListener();
    }).catch(() => { });
}

function handleChipChange(selectedId) {
    const allChip = document.getElementById('filter-all');
    const opChips = ['filter-access', 'filter-stat', 'filter-exec', 'filter-ioctl'];
    
    if (selectedId === 'filter-all' && allChip.selected) {
        opChips.forEach(id => document.getElementById(id).removeAttribute('selected'));
    } else if (opChips.includes(selectedId)) {
        if (document.getElementById(selectedId).hasAttribute('selected')) {
            allChip.removeAttribute('selected');
        }
    }
    filterSuLogList();
}

function setupSuLogFilter() {
    const chips = ['filter-all', 'filter-exclude-manager', 'filter-access', 'filter-stat', 'filter-exec', 'filter-ioctl'];
    chips.forEach(id => {
        const chip = document.getElementById(id);
        chip.addEventListener('click', () => requestAnimationFrame(() => handleChipChange(id)));
    });
    const allChip = document.getElementById('filter-all');
    const opChips = ['filter-access', 'filter-stat', 'filter-exec', 'filter-ioctl'];
    if (!allChip.hasAttribute('selected') && opChips.every(id => !document.getElementById(id).hasAttribute('selected'))) {
        allChip.setAttribute('selected', '');
    }
    filterSuLogList();
}

function filterSuLogList() {
    const allChip = document.getElementById('filter-all');
    const excludeChip = document.getElementById('filter-exclude-manager');
    const opChips = ['filter-access', 'filter-stat', 'filter-exec', 'filter-ioctl'];
    const selectedOps = opChips.filter(id => document.getElementById(id).selected).map(id => id.replace('filter-', ''));
    const showAll = allChip.selected;
    const excludeManager = excludeChip.selected;
    const list = document.getElementById('sulog-list');

    list.querySelectorAll('.sulog-item').forEach(item => {
        const hasOp = showAll || selectedOps.some(op => item.classList.contains(`operation-${op}`));
        const notManager = !excludeManager || !item.classList.contains(`uid-${uidModule.currentUid}`);
        item.style.display = (hasOp && notManager) ? '' : 'none';
    });
}

// SU log
export function appendSuLogList(newList, currentDate) {
    const suLogList = document.getElementById('sulog-list');
    const loading = document.getElementById('sulist-loading');
    if (!newList) {
        suLogList.innerHTML = '';
        newList = sulogModule.sulogList;
    }
    newList.forEach(item => {
        const operations = sulogModule.symbolMap[item.sym] || [item.sym];
        const operation = operations.map(op => `<div class="operation ${op}">${op}</div>`).join('');
        const app = sulogModule.appList.find(a => a.uid === item.uid);
        const timestamp = new Date(currentDate - sulogModule.upTime * 1000 + item.time * 1000);
        const year = timestamp.getFullYear();
        const month = String(timestamp.getMonth() + 1).padStart(2, '0');
        const day = String(timestamp.getDate()).padStart(2, '0');
        const hours = String(timestamp.getHours()).padStart(2, '0');
        const minutes = String(timestamp.getMinutes()).padStart(2, '0');
        const seconds = String(timestamp.getSeconds()).padStart(2, '0');
        const formattedTime = `${year}-${month}-${day} ${hours}:${minutes}:${seconds}`;

        const listItem = document.createElement('div');
        listItem.className = 'list-item sulog-item';
        listItem.classList.add(...operations.map(op => `operation-${op}`), `uid-${item.uid}`);
        listItem.innerHTML = `
            <div class="sulog-uid">
                <div>${item.uid}</div>
                ${app ? `<div class="package-name">${app.appLabel} (${app.packageName})</div>`: '' }
            </div>
            <div class="sulog-trailing">
                ${sulogModule.upTime === 0 ? '' : `<div class="timestamp">${formattedTime}</div>`}
                <div class="operations">${operation}</div>
            </div>
        `;
        suLogList.prepend(listItem);
    });
    loading.classList.remove('active');
    filterSuLogList();
}

function checkSuLogFeature() {
    exec(`${bin} --sulog`, { env: { PATH: `$PATH:${modDir}` }}).then((result) => {
        if (result.stdout.trim() === '' && !import.meta.env.DEV) {
            document.getElementById('sulist-loading').classList.remove('active');
            document.getElementById('sulog-unsupported').classList.add('active');
            return;
        }
        setInterval(sulogModule.getSulog, 2500);
        setupSuLogFilter();
    });
}

function checkUpdate() {
    const link = 'https://nightly.link/backslashxx/ksu_toolkit/workflows/module/master?preview';
    let htmlContent;
    const remote = spawn(`
        if command -v curl; then
            curl -Ls ${link}
        else
            busybox wget -qO- ${link}
        fi
    `, [], { env: { PATH: `$PATH:${ksuDir}/bin` }});
    remote.stdout.on('data', (data) => htmlContent += data);
    remote.on('exit', (code) => {
        if (code !== 0) return;
        const parser = new DOMParser();
        const doc = parser.parseFromString(htmlContent, "text/html");
        const zipURL = doc.querySelector('a[href$=".zip"]')?.href;

        if (!zipURL) return;
        const remoteVersion = parseInt(zipURL.split("-")[1]) || 0;

        exec(
            `cat /data/adb/modules_update/ksu_toolkit/module.prop ${modDir}/module.prop | grep "^versionCode=" | head -n1 | cut -d= -f2`
        ).then((local) => {
            if (local.stdout.trim() === '') return;
            const localVersion = parseInt(local.stdout.trim());
            if (localVersion < remoteVersion) {
                toast("Update available!");
                document.getElementById('update-btn').classList.add('show');
                document.getElementById('update-btn').onclick = () => {
                    toast("Redirecting to " + link);
                    setTimeout(() => {
                        exec(`am start -a android.intent.action.VIEW -d ${link}`)
                            .then(({ errno }) => {
                                if (errno !== 0) toast("Failed to open link");
                            });
                    }, 100);
                    document.getElementById('exit-btn').click();
                }
            }
        });
    });
}

function initTab() {
    const mdTab = document.querySelector('md-tabs');
    const contentContainers = document.querySelectorAll('.content-container');

    const updateTabPositions = () => {
        const activeTab = mdTab.querySelector('md-primary-tab[active]');
        if (!activeTab) return;

        const tabIndex = Array.from(mdTab.querySelectorAll('md-primary-tab')).indexOf(activeTab);
        contentContainers.forEach((container, index) => {
            const translateX = (index - tabIndex) * 100;
            container.style.transform = `translateX(${translateX}%)`;
            setTimeout(() => {
                container.style.transition = 'transform 0.3s ease';
                container.classList.remove('unresolved');
            }, 10);
        });
        // fab
        document.getElementById('crown').classList.toggle('show', activeTab.id === 'crown-tab');
    };

    contentContainers.forEach((container, index) => {
        const translateX = index * 100;
        container.style.transform = `translateX(${translateX}%)`;
    });

    updateTabPositions();
    mdTab.addEventListener('change', async () => {
        await Promise.resolve();
        updateTabPositions();
    });
}

document.addEventListener('DOMContentLoaded', async () => {
    document.querySelectorAll('[unresolved]').forEach(el => el.removeAttribute('unresolved'));

    // exit button
    const exitBtn = document.getElementById('exit-btn');
    if (typeof window.ksu !== 'undefined' && typeof window.ksu.exit !== 'undefined') {
        exitBtn.onclick = () => ksu.exit()
    } else if (typeof window.webui !== 'undefined' && typeof window.webui.exit !== 'undefined') {
        exitBtn.onclick = () => webui.exit()
    } else {
        exitBtn.style.display = 'none';
    }

    // tab init
    initTab();

    // Uid feature init
    await uidModule.getKsuManager();
    await uidModule.getCurrentUid();
    checkUidFeature();

    // Kernel umount feature init
    await umountModule.getUmountList();
    await umountModule.getUmountedList();
    await umountModule.getMountEntryList();
    await umountModule.getUmountProvider();
    checkUmountFeature();

    // SU log feature init
    await sulogModule.getAppList();
    checkSuLogFeature();

    // spoofing feature init
    spoofModule.initSpoofing();

    checkUpdate();
});
