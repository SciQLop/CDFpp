// Example files to try the Explorer on: real archive files, fetched from hosts that allow
// cross-origin reads, each opened on a variable that shows something worth zooming into.
// Rendered as cards on the empty page and behind the "Examples" button.

const CDAWEB = "https://cdaweb.gsfc.nasa.gov/pub/data";

export const PRESETS = [
    {
        title: "MMS1 burst electric field",
        description: "65 kHz waveform in burst segments: zoom down to single samples.",
        url: "https://lasp.colorado.edu/mms/sdc/public/files/api/v1/download/science?file=mms1_edp_brst_l2_hmfe_20210705040813_v2.0.0.cdf",
        variable: "mms1_edp_hmfe_par_epar_brst_l2",
        size: "111 MB",
    },
    {
        title: "MMS1 FPI ion energy spectrum",
        description: "Omni-directional ion spectrogram, gzip-compressed variables.",
        url: `${CDAWEB}/mms/mms1/fpi/fast/l2/dis-moms/2021/07/mms1_fpi_fast_l2_dis-moms_20210705040000_v3.4.0.cdf`,
        variable: "mms1_dis_energyspectr_omni_fast",
        size: "2 MB",
    },
    {
        title: "THEMIS-A ESA electrons",
        description: "A day of reduced-mode electron energy flux, with a time-varying energy table.",
        url: `${CDAWEB}/themis/tha/l2/esa/2021/tha_l2_esa_20210705_v01.cdf`,
        variable: "tha_peer_en_eflux",
        size: "20 MB",
    },
    {
        title: "Solar Orbiter magnetic field",
        description: "A day of B in RTN, 691k vectors at 8 Hz.",
        url: `${CDAWEB}/solar-orbiter/mag/science/l2/rtn-normal/2021/solo_l2_mag-rtn-normal_20210705_v01.cdf`,
        variable: "B_RTN",
        size: "12 MB",
    },
    {
        title: "OMNI SYM-H, July 2021",
        description: "A month of the 1-min ring current index, with fill values masked.",
        url: `${CDAWEB}/omni/omni_cdaweb/hro_1min/2021/omni_hro_1min_20210701_v01.cdf`,
        variable: "SYM_H",
        size: "9 MB",
    },
    {
        title: "Cluster C1 FGM",
        description: "Spin-resolution magnetic field in GSE, times in CDF_EPOCH.",
        url: `${CDAWEB}/cluster/c1/cp/2003/c1_cp_fgm_spin_20030115_v01.cdf`,
        variable: "B_vec_xyz_gse__C1_CP_FGM_SPIN",
        size: "0.4 MB",
    },
];

// The shareable Explorer link of a file, opened on a variable when one is given.
export function presetHref({ url, variable }) {
    const params = new URLSearchParams({ url });
    if (variable) params.set("var", variable);
    return `?${params}`;
}

export function renderPresets(container, onOpen) {
    const intro = document.createElement("div");
    intro.className = "log-dim";
    intro.textContent = "Load a CDF to begin, or try one of these files:";
    const grid = document.createElement("div");
    grid.className = "preset-grid";
    for (const preset of PRESETS) {
        const card = document.createElement("a");
        card.className = "preset-card";
        card.href = presetHref(preset);
        card.innerHTML = "<strong></strong><span></span><small></small>";
        card.querySelector("strong").textContent = preset.title;
        card.querySelector("span").textContent = preset.description;
        card.querySelector("small").textContent = `${preset.variable} · ${preset.size}`;
        card.addEventListener("click", (e) => {
            if (e.ctrlKey || e.metaKey || e.shiftKey || e.button !== 0) return;  // new tab / window
            e.preventDefault();
            onOpen(preset);
        });
        grid.appendChild(card);
    }
    container.replaceChildren(intro, grid);
}
