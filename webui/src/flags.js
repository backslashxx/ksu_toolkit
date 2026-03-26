const KSU_FLAGS = Object.freeze({
    LKM:       1 << 0, // 1
    MANAGER:   1 << 1, // 2
    LATE_LOAD: 1 << 2, // 4
    PR_BUILD:  1 << 3, // 8
});

const getKsuFlagStateKey = (flag) => flag
    .toLowerCase()
    .replace(/_([a-z])/g, (_, char) => char.toUpperCase());

const getFlagState = (flagsValue) =>
    Object.entries(KSU_FLAGS).reduce((state, [flag, bit]) => {
        state[getKsuFlagStateKey(flag)] = Boolean(flagsValue & bit);
        return state;
    }, {});

const buildFlagsValue = (state) => {
    let nextValue = KSU_FLAGS.MANAGER; // always enable manager

    for (const [flag, bit] of Object.entries(KSU_FLAGS)) {
        if (flag === 'MANAGER') continue;
        if (state[getKsuFlagStateKey(flag)]) nextValue |= bit;
    }

    return nextValue;
};

const applyFlagStateToChip = (state) => {
    document.querySelectorAll('.ksu-flag-chip').forEach((chip) => {
        chip.disabled = false;
        chip.selected = state[getKsuFlagStateKey(chip.dataset.flag)];
    });
};

const readFlagState = () => {
    let flagsValue = 0;
    document.querySelectorAll('.ksu-flag-chip').forEach((chip) => {
        if (chip.selected) flagsValue |= KSU_FLAGS[chip.dataset.flag];
    });
    return getFlagState(flagsValue);
};

export { applyFlagStateToChip, buildFlagsValue, getFlagState, readFlagState };
