import { exec } from 'kernelsu-alt';
import '@material/web/fab/fab.js';
import '@material/web/icon/icon.js';
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

export const modDir = '/data/adb/modules/ksu_toolkit';
export const bin = 'toolkit';
export const ksud = "/data/adb/ksud";
export const ksuDir = '/data/adb/ksu';
export const keyword = [
    "KernelSU",
    "SukiSU",
    "KowSU"
];

const uidFile = ksuDir + "/.manager_uid";
export const umountEntryFile = ksuDir + "/.umount_list";

// Manager uid crown
function appendManagerList() {
    const managerList = document.getElementById('manager-list');
    managerList.innerHTML = '';
    if (uidModule.manager.length === 0) document.getElementById('manager-empty').style.display = "block";
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
                    <span class="package-name">${item.packageName}</span>
                </div>
            </label>
            <md-radio id="${item.packageName}" name="manager-group" value="${item.uid.toString()}"></md-radio>
            <md-ripple></md-ripple>
        `;
        if (uidModule.currentUid && item.uid == uidModule.currentUid) {
            listItem.querySelector('md-radio').checked = true;
        }
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
            uidModule.saveManager(saveSwitch.selected ? radio.value : null);
            uidModule.setManager(radio.value, radio.id);
        });
    }
}

function checkUidFeature() {
    exec(
        `${bin} --setuid $(${bin} --getuid) || exit 1`,
        { env: { PATH: `$PATH:${modDir}` }}
    ).then((result) => {
        if (result.errno !== 0 && !import.meta.env.DEV) {
            document.getElementById('crown-unsupported').style.display = 'block';
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
    document.getElementById('umount-empty').style.display = umountModule.umountList.length === 0 ? "block" : "none";
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
        const entry = umountModule.mountEntryList.find(e => e.mount_point === item);
        if (entry && entry.source === 'KSU' && umountModule.umountProvider !== 'none') {
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
    const addBtn = searchBox.querySelector('.add-button');
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
        document.getElementById('rezygisk').style.display = 'block';
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
            document.getElementById('umount-unsupported').style.display = 'block';
            return;
        }
        appendUmountList();
        setupSeachOption();
        setupUmountPageListener();
    }).catch(() => { });
}

document.addEventListener('DOMContentLoaded', async () => {
    document.querySelectorAll('[unresolved]').forEach(el => el.removeAttribute('unresolved'));

    // tab init
    const mdTab = document.querySelector('md-tabs');
    mdTab.addEventListener('change', async () => {
        await Promise.resolve();
        mdTab.querySelectorAll('md-primary-tab').forEach(tab => {
            const panelId = tab.getAttribute('aria-controls');
            const isActive = tab.hasAttribute('active');
            const panel = document.getElementById(panelId);
            isActive ? panel.removeAttribute('hidden') : panel.setAttribute('hidden', '');
        });
    });

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
});
