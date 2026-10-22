// KCD1 Config Guide: config generator, config checker and cvar database.
// Data: data.json from ../tools/build_site_data.py. The audit rules (judge, parse) are a port of
// ../tools/cfg_audit.py; keep the two in step.
"use strict";

const GITHUB = "https://github.com/eonflux7/kcd1-clarity/blob/main/docs/cvars/";
const LEVELS = { 1: "Low", 2: "Medium", 3: "High", 4: "Very High", 7: "7" };
const PREFIX = {
  r: "Renderer", e: "3D engine (world, shadows, vegetation, streaming)", sys: "System (threads, memory, specs)",
  ca: "Character animation", wh: "Warhorse game code", p: "Physics", ai: "AI", s: "Sound", g: "Game",
  cl: "Camera / client", i: "Input", mfx: "Material effects", q: "Shader quality", es: "Entity system",
  log: "Logging", con: "Console", net: "Network", v: "Vehicles", mov: "Cutscenes",
};
const STATUS = {
  free: { label: "Settable", cls: "b-ok", where: "user.cfg (or autoexec.cfg)",
    text: "Not touched by the video options or presets, so a value in user.cfg sticks." },
  menu: { label: "Video option", cls: "b-menu", where: "autoexec.cfg",
    text: "Set by a video option or quality preset from your profile after user.cfg is read, so a user.cfg value is replaced. autoexec.cfg most likely wins, until you change that option in the menu." },
  init: { label: "Startup only", cls: "b-init", where: "user.cfg",
    text: "Read once while the engine starts, before autoexec.cfg runs. Only user.cfg works." },
  blocked: { label: "Locked", cls: "b-blocked", where: "only with -devmode",
    text: "Cheat, const or read-only. Config files are refused unless the game runs with the -devmode launch option." },
  dead: { label: "No effect", cls: "b-dead", where: "nowhere",
    text: "Registered, but the game never uses the value." },
};
const VERDICT_CLS = { OK: "b-ok", OK_INIT: "b-init", TOO_LATE: "b-blocked", OVERRIDDEN: "b-menu", AT_RISK: "b-risk",
  BLOCKED: "b-blocked", DEAD: "b-dead", MISSING: "b-missing", IGNORED: "b-ignored", GROUP: "b-group" };

const $ = (s, el = document) => el.querySelector(s);
const esc = (s) => String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" })[c]);
const store = {
  get(k, d) { try { const v = localStorage.getItem("kcd1cfg." + k); return v == null ? d : JSON.parse(v); } catch { return d; } },
  set(k, v) { try { localStorage.setItem("kcd1cfg." + k, JSON.stringify(v)); } catch { /* private mode */ } },
};

let D;                       // data.json
const byName = new Map();    // lower name -> cvar
const groupsOf = new Map();  // lower cvar name -> [group names]
const groupByLc = new Map(); // lower group name -> group name
let menuGroups, rec;

// ---------------------------------------------------------------- data

function prepare(data) {
  D = data;
  menuGroups = new Set(D.menuGroups);
  rec = new Map(Object.entries(D.rec.cvars).map(([n, r]) => [n.toLowerCase(), { name: n, ...r }]));
  for (const [name, type, def, flags, help] of D.cvars)
    byName.set(name.toLowerCase(), { name, type, def, flags, help: help.trim(), lc: name.toLowerCase() });
  for (const [g, d] of Object.entries(D.groups)) {
    groupByLc.set(g.toLowerCase(), g);
    for (const k of Object.keys(d.cvars)) {
      const lc = k.toLowerCase();
      if (!groupsOf.has(lc)) groupsOf.set(lc, []);
      groupsOf.get(lc).push(g);
    }
  }
}

const isBlocked = (cv) => (cv.flags & D.block) !== 0 || (cv.flags & 0x800000) !== 0;
const menuGroupsOf = (lc) => (groupsOf.get(lc) || []).filter((g) => menuGroups.has(g.toLowerCase()));

function status(cv) {
  if (cv.group) return menuGroups.has(cv.lc) ? "menu" : "free";
  if (isBlocked(cv)) return "blocked";
  if (cv.lc in D.dead) return "dead";
  if (cv.lc in D.initOnly) return "init";
  if (cv.lc in D.menu || menuGroupsOf(cv.lc).length) return "menu";
  return "free";
}

function whereCode(lc) {
  const r = rec.get(lc);
  if (r) return r.where;
  const cv = byName.get(lc) || (groupByLc.has(lc) && { group: true, lc });
  if (!cv) return "E";
  return { free: "E", menu: "A", init: "U", blocked: "D", dead: "-" }[status(cv)];
}

// ---------------------------------------------------------------- audit (port of cfg_audit.py)

function* parse(text, kind) {
  for (const raw of text.split(/[\r\n]+/)) {
    const s = raw.trim();
    if (!s || s.startsWith(";") || s.startsWith("--")) continue;
    if (kind === "autoexec") {
      for (const cmd of s.split(";").map((c) => c.trim()).filter(Boolean)) {
        const i = cmd.includes("=") ? cmd.indexOf("=") : cmd.indexOf(" ");
        const k = i < 0 ? cmd : cmd.slice(0, i), v = i < 0 ? "" : cmd.slice(i + 1);
        yield { k: k.trim(), v: v.trim().replace(/^[\s"']+|[\s"']+$/g, ""), raw: s };
      }
    } else if (!s.includes("=")) {
      yield { k: null, v: null, raw: s };
    } else {
      const i = s.indexOf("=");
      const v = s.slice(i + 1).trim(), m = v.match(/^"(.*)"/);
      yield { k: s.slice(0, i).trim(), v: m ? m[1] : v, raw: s };
    }
  }
}

function number(v) {
  const m = String(v ?? "").match(/^\s*([-+]?\d*\.?\d+(?:[eE][-+]?\d+)?)/);
  return m ? m[1] : null;
}

function judge(kind, k, v) {
  const kl = k.toLowerCase();
  if (groupByLc.has(kl)) {
    const what = `Preset group: sets every cvar in Config/CVarGroups/${groupByLc.get(kl)}.cfg`;
    if (!menuGroups.has(kl)) return ["GROUP", what];
    if (kind === "autoexec") return ["AT_RISK", what + "; the menu's quality level is re-applied whenever it's changed"];
    return ["OVERRIDDEN", what + "; the menu's quality level replaces it at profile load"];
  }
  const r = byName.get(kl);
  if (!r) return kl in D.dead ? ["DEAD", D.dead[kl]] : ["MISSING", "not a cvar in this game version; silently ignored"];
  const fl = r.flags, notes = [];
  if (isBlocked(r)) {
    const why = [...new Set([[0x2, "cheat"], [0x2000000, "cheat"], [0x1000000, "cheat"], [0x800, "read-only"],
      [0x40000000, "locked"], [0x800000, "const"]].filter(([b]) => fl & b).map(([, n]) => n))].sort();
    return ["BLOCKED", `${why.join("/")}: refused unless the game runs with -devmode`];
  }
  const n = number(v);
  if ((r.type === "int" || r.type === "float") && n === null) notes.push(`value "${v}" is not a number (parses as 0)`);
  if (r.type === "int" && n && /[.eE]/.test(n)) notes.push(`whole-number cvar: ${n} is cut to ${Math.trunc(parseFloat(n))}`);
  if (fl & 0x4) notes.push("flagged deprecated by the developers");
  if (kl in D.dead) return ["DEAD", D.dead[kl]];
  if (kl in D.initOnly) {
    if (kind === "autoexec") return ["TOO_LATE", D.initOnly[kl] + "; read during startup, before autoexec.cfg runs"];
    return ["OK_INIT", D.initOnly[kl]];
  }
  const over = [];
  if (kl in D.menu) over.push("menu: " + D.menu[kl]);
  const ms = menuGroupsOf(kl).sort();
  if (ms.length) over.push("quality preset " + ms.map((g) => g.replace("sys_spec_", "")).join("/"));
  if (n !== null && typeof r.def === "number" && parseFloat(n) === r.def) notes.push("equals the default");
  if (over.length) {
    const tail = notes.length ? "; " + notes.join("; ") : "";
    if (kind === "autoexec") return ["AT_RISK", "re-applied by " + over.join(", ") + " whenever that setting is changed" + tail];
    return ["OVERRIDDEN", "re-applied by " + over.join(", ") + " at profile load, after user.cfg" + tail];
  }
  return ["OK", notes.join("; ")];
}

// ---------------------------------------------------------------- tabs / routing

const TABS = ["generator", "check", "db", "presets", "how"];

function route() {
  const [tab, arg] = decodeURIComponent(location.hash.slice(1)).split("/");
  const t = TABS.includes(tab) ? tab : "generator";
  for (const id of TABS) $("#tab-" + id).hidden = id !== t;
  for (const a of document.querySelectorAll("nav.tabs a")) {
    if (a.getAttribute("href") === "#" + t) a.setAttribute("aria-current", "page");
    else a.removeAttribute("aria-current");
  }
  if (t === "db" && arg) openCvar(arg);
  if (t === "presets" && arg) document.getElementById("preset-" + arg.toLowerCase())?.scrollIntoView({ block: "start" });
}

// ---------------------------------------------------------------- generator

// [cvar, {tier: value}, short comment]; tier "all" applies to every goal except "none"
const GPU = [
  ["e_svoTI_LowSpecMode", { low: 6, mid: 4, high: 3 }, "global illumination resolution and cost (biggest GPU knob)"],
  ["e_ShadowsMaxTexRes", { low: 512, mid: 1024 }, "sun shadow resolution"],
  ["e_GsmLodsNum", { low: 4, mid: 4 }, "sun shadow cascades"],
  ["e_ShadowsCastViewDistRatio", { low: 0.5, mid: 0.8 }, "sun shadow caster distance"],
  ["r_ShadowCastingLightsMaxCount", { low: 6, mid: 10, high: 14 }, "local lights that cast shadows"],
  ["r_DeferredShadingAreaLights", { low: 0, mid: 0 }, "complex area lights"],
  ["r_ssdoHalfRes", { low: 2, mid: 2, high: 2 }, "half-resolution SSDO"],
  ["r_ParticlesHalfRes", { low: 1 }, "half-resolution particles"],
  ["e_ParticlesMaxScreenFill", { low: 32, mid: 64 }, "particle screen coverage limit"],
];
const CPU = [
  ["e_MergedMeshesViewDistRatio", { low: 50, mid: 60 }, "grass draw distance (also the vegetation slider)"],
  ["e_MergedMeshesInstanceDist", { low: 2, mid: 4 }, "animated grass distance"],
  ["e_MergedMeshesLodRatio", { low: 1, mid: 4 }, "grass LOD distance"],
  ["e_MergedMeshesUseSpines", { low: 0 }, "grass bending simulation"],
  ["wh_cc_LodRatioRelative", { low: 0.5, mid: 0.6 }, "NPC LOD distance (crowds are KCD's main CPU cost)"],
  ["wh_cc_LodForItemNoShadowsBase", { low: 2, mid: 3 }, "NPC items stop casting shadows sooner"],
  ["wh_cc_LodForAttachmentNoShadows", { low: 0, mid: 1 }, "NPC attachments stop casting shadows sooner"],
  ["ca_AttachmentCullingRation", { low: 145, mid: 300 }, "NPC attachment culling distance"],
  ["e_DynamicLightsMaxEntityLights", { low: 7, mid: 11 }, "lights per object"],
  ["es_MaxPhysDist", { low: 50 }, "physics activation range"],
  ["e_CullVegActivation", { low: 30 }, "vegetation physics activation range"],
  ["p_max_substeps", { low: 2 }, "physics substeps"],
];
const POOL = { 2: 1024, 3: 1024, 4: 1536, 6: 3072, 8: 4096, 10: 5120, 12: 6144, 16: 6144 };

function presetValue(name, level) {
  for (const g of menuGroupsOf(name.toLowerCase())) {
    const cv = Object.entries(D.groups[g].cvars).find(([k]) => k.toLowerCase() === name.toLowerCase());
    if (cv && cv[1][level] != null) return { group: g, value: cv[1][level], low: cv[1]["1"], high: cv[1]["4"] };
  }
  return null;
}

// Skips a value that wouldn't save anything at the player's preset level: when the preset already sits at or
// beyond it on the cheap side (the Low preset's side).
function alreadyCheaper(name, value, level) {
  const p = presetValue(name, level);
  if (!p) return null;
  const cur = parseFloat(p.value), lo = parseFloat(p.low), hi = parseFloat(p.high);
  if ([cur, lo, hi].some(Number.isNaN) || lo === hi) return null;
  const cheaperIsLower = lo < hi;
  return (cheaperIsLower ? cur <= value : cur >= value) ? p : null;
}

function readGen() {
  return {
    vram: +$("#g-vram").value, ram: +$("#g-ram").value, threads: +$("#g-threads").value, drive: $("#g-drive").value,
    goal: $("#g-goal input:checked").value, level: $("#g-level").value, fps: $("#g-fps").value,
    vsync: $("#g-vsync").checked, extras: $("#g-extras").checked, finalize: $("#g-finalize").checked,
    nvme: $("#g-nvme").checked,
  };
}

function generate() {
  const o = readGen();
  store.set("gen", o);
  $("#g-nvme").disabled = o.drive !== "nvme";
  $("#g-nvme-row").classList.toggle("disabled", o.drive !== "nvme");
  if (o.drive !== "nvme") $("#g-nvme").checked = o.nvme = false;

  const lines = [], skipped = [];
  const add = (name, value, comment, section) => lines.push({ name, value, comment, section, where: whereCode(name.toLowerCase()) });
  const tierAdd = (list, section) => {
    for (const [name, vals, comment] of list) {
      const v = vals[o.goal];
      if (v === undefined) continue;
      const p = alreadyCheaper(name, v, o.level);
      if (p) skipped.push(`${name} (your ${LEVELS[o.level]} preset already sets ${p.value})`);
      else add(name, v, comment, section);
    }
  };

  add("r_TexturesStreamPoolSize", POOL[o.vram], `texture pool for ${o.vram === 16 ? "16+" : o.vram} GB VRAM; the preset's 1536/2048 is too small for the HD textures`, "Stutter");
  if (o.vram <= 3) add("r_TexturesStreamingMipBias", 1, "slightly lower texture mips: less VRAM and streaming", "Stutter");
  if (o.fps) {
    add("sys_MaxFPS", +o.fps, "frame cap: smoother frame pacing, leaves CPU time for streaming", "Stutter");
    if (o.vsync) add("r_VSync", 0, "the cap replaces V-Sync", "Stutter");
  }
  if (o.ram >= 32) add("e_StreamCgfPoolSize", 1024, "bigger mesh streaming cache (32 GB RAM)", "Stutter");
  if (o.goal === "low" || o.goal === "mid")
    tierAdd([["r_ShadowPoolMaxTimeslicedUpdatesPerFrame", { low: 20, mid: 30 }, "fewer shadow updates per frame"]], "Stutter");
  if (o.finalize) add("sys_streaming_max_finalize_per_frame", 12, "experimental: cap streaming finalize work per frame", "Stutter");
  if (o.nvme) {
    add("e_StreamCgfMaxTasksInProgress", 64, "experimental: more parallel mesh streaming (NVMe)", "Stutter");
    add("e_StreamInstancesMaxTasks", 64, "experimental: more parallel instance streaming (NVMe)", "Stutter");
  }
  if (o.goal !== "none") {
    tierAdd(GPU, "GPU");
    tierAdd(CPU, "CPU");
  }
  if (o.extras) {
    add("r_SSReflections", 0, "screen-space reflections off", "GPU");
    add("r_WaterVolumeCaustics", 0, "underwater caustics off", "GPU");
    add("e_ParticlesMotionBlur", 0, "particle motion blur off", "GPU");
  }
  if (o.threads > 8) add("sys_job_system_max_worker", 0, "use all CPU threads (default caps at 8)", "CPU");
  for (const c of custom) add(c.name, c.value, "your line", "Your own lines");

  // later lines win, like in the game
  const seen = new Map();
  for (const l of lines) seen.set(l.name.toLowerCase(), l);
  const final = [...seen.values()];
  const user = final.filter((l) => l.where === "E" || l.where === "U");
  const auto = final.filter((l) => l.where === "A" || l.where === "D");
  const dev = final.filter((l) => l.where === "D");
  const dead = final.filter((l) => l.where === "-");

  const today = new Date().toISOString().slice(0, 10);
  const head = (file, what) => [
    `-- ${file}, generated by the KCD1 Config Guide on ${today}`,
    "-- https://eonflux7.github.io/kcd1-clarity/cvars/",
    "-- Kingdom Come: Deliverance 1.9.8 (Steam). Lines starting with -- are comments.",
    `-- ${what}`, ""];
  const body = (ls) => {
    if (!ls.length) return ["-- Nothing for this file with these choices."];
    const out = [];
    let sec = "";
    for (const l of ls) {
      if (l.section !== sec) { if (sec) out.push(""); out.push(`-- ${l.section}`); sec = l.section; }
      out.push(`-- ${l.comment}${l.where === "D" ? " (needs -devmode)" : ""}`);
      out.push(`${l.name} = ${l.value}`);
    }
    return out;
  };
  $("#g-user").value = [...head("user.cfg", "Read at startup. Video-option cvars are in autoexec.cfg, because the game replaces them here."), ...body(user)].join("\n");
  $("#g-auto").value = [...head("autoexec.cfg", "Video-option and preset cvars. Changing that option in the menu undoes the line until the next restart."), ...body(auto)].join("\n");

  $("#g-devmode").innerHTML = dev.length
    ? `<div class="note"><b>Needs <code>-devmode</code>:</b> ${dev.map((l) => `<code>${esc(l.name)}</code>`).join(", ")}. Add
       <code>-devmode</code> in Steam &gt; the game's Properties &gt; Launch options. It also unlocks developer and cheat
       cvars; without it these lines are refused (the rest still works).</div>` : "";
  const notes = [];
  if (skipped.length) notes.push(`Left out because your preset is already cheaper: ${skipped.map(esc).join(", ")}.`);
  if (dead.length) notes.push(`No effect, left out: ${dead.map((l) => esc(l.name)).join(", ")}.`);
  if (notes.length) $("#g-devmode").innerHTML += `<p class="small muted">${notes.join(" ")}</p>`;
  $("#g-summary").textContent = `${user.length} line${user.length === 1 ? "" : "s"} in user.cfg, ${auto.length} in autoexec.cfg`;
}

let custom = [];

function renderCustom() {
  store.set("custom", custom);
  const el = $("#g-custom");
  if (!custom.length) { el.innerHTML = '<p class="muted">None yet.</p>'; generate(); return; }
  el.innerHTML = `<table><tr><th>cvar</th><th>value</th><th>goes to</th><th></th></tr>${custom.map((c, i) => {
    const w = whereCode(c.name.toLowerCase());
    const file = { E: "user.cfg", U: "user.cfg", A: "autoexec.cfg", D: "autoexec.cfg + -devmode", "-": "left out (no effect)" }[w];
    return `<tr><td><a href="#db/${esc(c.name)}"><code>${esc(c.name)}</code></a></td>
      <td><input type="text" size="8" value="${esc(c.value)}" data-i="${i}" aria-label="value of ${esc(c.name)}"></td>
      <td>${file}</td><td><button class="small" data-del="${i}">Remove</button></td></tr>`;
  }).join("")}</table>`;
  generate();
}

function addCustom(name, value) {
  const i = custom.findIndex((c) => c.name.toLowerCase() === name.toLowerCase());
  if (i >= 0) custom[i].value = value; else custom.push({ name, value });
  renderCustom();
}

function setupGenerator() {
  const saved = store.get("gen", null);
  if (saved) {
    for (const k of ["vram", "ram", "threads", "drive", "level", "fps"]) if (saved[k] != null) $("#g-" + k).value = saved[k];
    for (const k of ["vsync", "extras", "finalize", "nvme"]) if (saved[k] != null) $("#g-" + k).checked = saved[k];
    const r = $(`#g-goal input[value="${saved.goal}"]`);
    if (r) r.checked = true;
  }
  custom = store.get("custom", []);
  $("#tab-generator").addEventListener("change", (e) => {
    if (e.target.name === "goal") $("#g-extras").checked = e.target.value === "low" || e.target.value === "mid";
    if (e.target.dataset.i != null) { custom[+e.target.dataset.i].value = e.target.value.trim(); store.set("custom", custom); }
    generate();
  });
  $("#g-custom").addEventListener("click", (e) => {
    if (e.target.dataset.del != null) { custom.splice(+e.target.dataset.del, 1); renderCustom(); }
  });
  document.addEventListener("click", (e) => {
    const c = e.target.closest("[data-copy]"), d = e.target.closest("[data-download]");
    if (c) {
      const ta = $("#" + c.dataset.copy);
      const done = () => { c.textContent = "Copied"; setTimeout(() => (c.textContent = "Copy"), 1200); };
      if (navigator.clipboard) navigator.clipboard.writeText(ta.value).then(done, () => ta.select());
      else ta.select();
    }
    if (d) {
      // CRLF so Notepad shows it properly; the game splits on CR/LF either way
      const blob = new Blob([$("#" + d.dataset.download).value.replace(/\n/g, "\r\n") + "\r\n"], { type: "text/plain" });
      const a = Object.assign(document.createElement("a"), { href: URL.createObjectURL(blob), download: d.dataset.name });
      a.click();
      setTimeout(() => URL.revokeObjectURL(a.href), 1000);
    }
  });
  renderCustom();
}

// ---------------------------------------------------------------- checker

const EXAMPLE = `; Typical lines from community configs
r_TexturesStreamPoolSize = 4096
sys_MaxFPS = 50
r_HDRBloom = 0.4
r_MultiThreaded = 2
e_svoTI_ResScaleBase = 0
sys_job_system_max_worker = 0
#Shadows
e_ShadowsMaxTexRes = 2048
r_ShadowsCache = 1
sys_PakStreamCache = 1
e_ShadowsClouds = 0`;

function check() {
  const kind = $("#c-kind").value, text = $("#c-input").value;
  const rows = [];
  for (const { k, v, raw } of parse(text, kind)) {
    if (k === null) rows.push({ raw, verdict: "IGNORED", why: "no '=': not a config line (the game logs it as invalid)" });
    else { const [verdict, why] = judge(kind, k, v); rows.push({ raw, k, verdict, why }); }
  }
  $("#c-results-panel").hidden = !rows.length;
  if (!rows.length) return;
  const counts = {};
  for (const r of rows) counts[r.verdict] = (counts[r.verdict] || 0) + 1;
  $("#c-summary").innerHTML = Object.entries(counts).sort((a, b) => b[1] - a[1])
    .map(([v, n]) => `<span class="badge ${VERDICT_CLS[v]}">${n} ${v}</span>`).join(" ");
  $("#c-results").innerHTML = `<tr><th>line</th><th>verdict</th><th>why</th></tr>` + rows.map((r) => {
    const name = r.k && (byName.get(r.k.toLowerCase()) || groupByLc.has(r.k.toLowerCase()))
      ? `<a href="#db/${esc(r.k)}"><code>${esc(r.raw)}</code></a>` : `<code>${esc(r.raw)}</code>`;
    return `<tr><td>${name}</td><td><span class="badge ${VERDICT_CLS[r.verdict]}">${r.verdict}</span></td><td>${esc(r.why)}</td></tr>`;
  }).join("");
  $("#c-legend").innerHTML = Object.keys(counts)
    .map((v) => `<span class="badge ${VERDICT_CLS[v]}">${v}</span><span>${esc(D.legend[v])}</span>`).join("");
}

function setupChecker() {
  $("#c-input").addEventListener("input", check);
  $("#c-kind").addEventListener("change", check);
  $("#c-example").addEventListener("click", () => { $("#c-input").value = EXAMPLE; $("#c-kind").value = "user"; check(); });
  $("#c-file").addEventListener("change", async (e) => {
    const f = e.target.files[0];
    if (!f) return;
    const name = f.name.toLowerCase();
    $("#c-kind").value = name.includes("autoexec") ? "autoexec" : "user";
    $("#c-file-warn").innerHTML = name.endsWith(".txt")
      ? `<div class="note">This file is named <code>${esc(f.name)}</code>. The game only loads files named exactly
         <code>user.cfg</code> / <code>autoexec.cfg</code>, so nothing in it is used until you rename it.</div>` : "";
    $("#c-input").value = await f.text();
    check();
  });
}

// ---------------------------------------------------------------- database

let dbItems = [], dbShown = 0, dbChip = "all";
const CHIPS = [["all", "All"], ["free", "Settable"], ["menu", "Video option"], ["init", "Startup only"],
  ["blocked", "Locked"], ["dead", "No effect"], ["rec", "Has advice"], ["group", "Preset groups"]];
const PAGE = 120;

function allEntries() {
  const groups = Object.keys(D.groups).map((g) => ({
    name: g, lc: g.toLowerCase(), group: true, type: "group", def: D.groups[g].default_level, flags: 0,
    help: menuGroups.has(g.toLowerCase())
      ? "Quality preset group. The matching video option sets its level from your profile, which sets every cvar listed below."
      : "Preset group (not a menu option). Setting it applies every cvar listed below.",
  }));
  return [...byName.values(), ...groups].sort((a, b) => a.lc.localeCompare(b.lc));
}

function prefixOf(lc) { const i = lc.indexOf("_"); return i > 0 ? lc.slice(0, i) : ""; }

function setupDb() {
  const entries = allEntries();
  const counts = {};
  for (const e of entries) if (!e.group) counts[prefixOf(e.lc)] = (counts[prefixOf(e.lc)] || 0) + 1;
  $("#d-prefix").innerHTML = `<option value="">All subsystems</option>` + Object.entries(counts)
    .filter(([p, n]) => p && n >= 10).sort((a, b) => b[1] - a[1])
    .map(([p, n]) => `<option value="${esc(p)}">${esc(p)}_ ${PREFIX[p] ? "· " + esc(PREFIX[p]) : ""} (${n})</option>`).join("");
  $("#d-chips").innerHTML = CHIPS.map(([k, l]) => `<button class="chip" data-chip="${k}" aria-pressed="${k === "all"}">${l}</button>`).join("");
  $("#d-chips").addEventListener("click", (e) => {
    const b = e.target.closest("[data-chip]");
    if (!b) return;
    dbChip = b.dataset.chip;
    for (const c of document.querySelectorAll("#d-chips .chip")) c.setAttribute("aria-pressed", c === b);
    filterDb(entries);
  });
  let t;
  $("#d-q").addEventListener("input", () => { clearTimeout(t); t = setTimeout(() => filterDb(entries), 120); });
  $("#d-prefix").addEventListener("change", () => filterDb(entries));
  $("#d-more").addEventListener("click", () => renderDb(false));
  $("#d-list").addEventListener("click", (e) => {
    const row = e.target.closest("button.cv-row");
    if (row) { toggleCvar(row.parentElement); return; }
    const add = e.target.closest("[data-add]");
    if (add) {
      const v = add.parentElement.querySelector("input").value.trim();
      addCustom(add.dataset.add, v);
      add.textContent = "Added to the generator";
    }
  });
  filterDb(entries);
}

function filterDb(entries) {
  const words = $("#d-q").value.toLowerCase().split(/\s+/).filter(Boolean);
  const prefix = $("#d-prefix").value;
  dbItems = entries.filter((e) => {
    if (prefix && (e.group || prefixOf(e.lc) !== prefix)) return false;
    if (dbChip === "group" && !e.group) return false;
    if (dbChip === "rec" && !rec.has(e.lc)) return false;
    if (!["all", "group", "rec"].includes(dbChip) && (e.group || status(e) !== dbChip)) return false;
    const hay = e.lc + " " + e.help.toLowerCase();
    return words.every((w) => hay.includes(w));
  });
  if (words.length) dbItems.sort((a, b) => (words.some((w) => b.lc.includes(w)) - words.some((w) => a.lc.includes(w))) || a.lc.localeCompare(b.lc));
  $("#d-count").textContent = `${dbItems.length} of ${entries.length}`;
  renderDb(true);
}

function renderDb(reset) {
  if (reset) { $("#d-list").innerHTML = ""; dbShown = 0; }
  const next = dbItems.slice(dbShown, dbShown + PAGE);
  $("#d-list").insertAdjacentHTML("beforeend", next.map(rowHtml).join(""));
  dbShown += next.length;
  $("#d-more").hidden = dbShown >= dbItems.length;
  $("#d-more").textContent = `Show more (${dbItems.length - dbShown} left)`;
}

function rowHtml(e) {
  const s = STATUS[status(e)];
  const label = e.group ? "Preset group" : s.label;
  return `<div class="cv" data-name="${esc(e.name)}"><button class="cv-row" aria-expanded="false">
    <span class="cv-name">${esc(e.name)}</span>
    <span><span class="badge ${e.group ? "b-group" : s.cls}">${label}</span>${rec.has(e.lc) ? ' <span class="badge b-init">advice</span>' : ""}</span>
    <span class="cv-help">${esc(e.help.split("\n")[0]) || '<i>no description in the game</i>'}</span></button></div>`;
}

function toggleCvar(el, open) {
  const isOpen = el.classList.contains("open");
  if (open === undefined) open = !isOpen;
  if (open === isOpen) return;
  el.classList.toggle("open", open);
  el.querySelector("button.cv-row").setAttribute("aria-expanded", open);
  if (!open) { el.querySelector(".cv-detail")?.remove(); return; }
  const lc = el.dataset.name.toLowerCase();
  const e = byName.get(lc) || allEntries().find((x) => x.lc === lc);
  el.insertAdjacentHTML("beforeend", detailHtml(e));
}

function flagNames(f) {
  const out = [];
  for (const [bit, name] of Object.entries(D.flags)) if (f & +bit) out.push(name);
  let known = 0;
  for (const bit of Object.keys(D.flags)) known |= +bit;
  const rest = f & ~known;
  if (rest) out.push("0x" + (rest >>> 0).toString(16));
  return out.join(", ") || "none";
}

function levelsTable(rows) {
  const lv = [1, 2, 3, 4, 7];
  return `<div class="table-wrap"><table class="levels"><tr><th>cvar</th>${lv.map((l) => `<th>${LEVELS[l]}</th>`).join("")}</tr>${rows
    .map(([n, vals]) => `<tr><td><a href="#${groupByLc.has(n.toLowerCase()) ? "presets" : "db"}/${esc(n)}">${esc(n)}</a></td>${lv.map((l) => `<td>${esc(vals[l] ?? "")}</td>`).join("")}</tr>`)
    .join("")}</table></div>`;
}

function detailHtml(e) {
  const st = status(e), s = STATUS[st], r = rec.get(e.lc);
  let h = `<div class="cv-detail">`;
  if (e.group) {
    const g = D.groups[e.name];
    h += `<p>${esc(e.help)}</p><p class="small muted">File default level: ${g.default_level}. Level 7 exists in the files;
      the menu offers Low to Very High. <a href="#presets/${esc(e.name)}">Compare levels in Graphics presets</a></p>`;
    h += levelsTable(Object.entries(g.cvars));
  } else {
    h += `<dl>
      <dt>Can a config set it?</dt><dd><span class="badge ${s.cls}">${s.label}</span> ${esc(s.text)}${st === "dead" ? " " + esc(D.dead[e.lc]) : ""}${st === "init" ? " " + esc(D.initOnly[e.lc]) + "." : ""}</dd>
      <dt>Put it in</dt><dd>${esc(s.where)}${e.lc in D.menu ? ` (menu: ${esc(D.menu[e.lc])})` : ""}</dd>
      <dt>Game's description</dt><dd>${e.help ? `<pre>${esc(e.help)}</pre>` : '<i class="muted">none</i>'}</dd>
      <dt>Type</dt><dd>${esc(e.type)}</dd>
      <dt>Default</dt><dd><code>${esc(e.def ?? "?")}</code> <span class="small muted">(where it's registered; system.cfg or a preset may change it)</span></dd>
      <dt>Flags</dt><dd class="small">${esc(flagNames(e.flags))}</dd>
    </dl>`;
    if (r) {
      h += `<div class="rec${r.dont ? " dont" : ""}"><b>Advice:</b> ${esc(r.rec)}${r.experimental ? " <i>(experimental)</i>" : ""}<br>${esc(r.why)}
        ${r.dont ? `<br><b>Avoid:</b> ${esc(r.dont)}` : ""}
        <br><span class="small muted">Not benchmarked. From <a href="${GITHUB}perf-cvars.md">perf-cvars.md</a>.</span></div>`;
    }
    for (const g of groupsOf.get(e.lc) || []) {
      const vals = Object.entries(D.groups[g].cvars).find(([k]) => k.toLowerCase() === e.lc)[1];
      h += `<p class="small"><a href="#presets/${esc(g)}"><b>${esc(g)}</b></a> preset values${menuGroups.has(g.toLowerCase()) ? " (set from the video options)" : ""}:</p>`;
      h += levelsTable([[e.name, vals]]);
    }
    if (st !== "dead") {
      h += `<div class="add"><input type="text" value="${esc(e.def ?? "")}" aria-label="value">
        <button class="small" data-add="${esc(e.name)}">Add to my config</button>
        <span class="small muted">goes to the generator, in ${esc(s.where)}</span></div>`;
    }
  }
  h += `<p class="small"><a href="#db/${esc(e.name)}">Link to this cvar</a></p></div>`;
  return h;
}

function openCvar(name) {
  const lc = name.toLowerCase();
  $("#d-q").value = name;
  $("#d-prefix").value = "";
  dbChip = "all";
  for (const c of document.querySelectorAll("#d-chips .chip")) c.setAttribute("aria-pressed", c.dataset.chip === "all");
  filterDb(allEntries());
  const el = [...document.querySelectorAll("#d-list .cv")].find((x) => x.dataset.name.toLowerCase() === lc);
  if (el) { toggleCvar(el, true); el.scrollIntoView({ block: "start" }); }
}

// ---------------------------------------------------------------- graphics presets

// Our names for the groups; the menu's own wording may differ.
const PRESET_TITLE = {
  gameeffects: "Game effects", light: "Lighting", objectdetail: "Object detail", particles: "Particles",
  physics: "Physics", postprocessing: "Post-processing", shading: "Shading", shadows: "Shadows", texture: "Textures",
  textureresolution: "Texture resolution", vegetation: "Vegetation", volumetriceffects: "Volumetric effects",
  water: "Water", full: "Full", quality: "Shader quality", sound: "Sound",
};
const presetTitle = (g) => PRESET_TITLE[g.toLowerCase().replace("sys_spec_", "")] || g;

function presetCard(g, lv, all) {
  const cvars = Object.entries(D.groups[g].cvars);
  const changes = (vals) => lv.some((l, i) => i && vals[l] !== vals[lv[i - 1]]);
  const changed = cvars.filter(([, vals]) => changes(vals)).length;
  const rows = all || lv.length < 2 ? cvars : cvars.filter(([, vals]) => changes(vals));
  const names = lv.map((l) => LEVELS[l]);
  let h = `<article class="panel preset" id="preset-${esc(g.toLowerCase())}"><div class="out-head">
    <h2>${esc(presetTitle(g))} <a class="preset-id" href="#db/${esc(g)}">${esc(g)}</a></h2>
    <span class="small muted">${lv.length < 2 ? `${cvars.length} cvars` : `${changed} of ${cvars.length} change`}</span></div>`;
  if (!rows.length) return h + `<p class="small muted">Nothing changes between ${names[0]} and ${names[names.length - 1]}.</p></article>`;
  h += `<div class="table-wrap"><table class="levels ptable" style="--n:${lv.length}"><thead><tr><th>cvar</th>${names.map((n) => `<th>${n}</th>`).join("")}</tr></thead><tbody>`;
  for (const [n, vals] of rows) {
    const lc = n.toLowerCase(), cv = byName.get(lc);
    const href = groupByLc.has(lc) ? "#presets/" + n : "#db/" + n;
    let badge = "";
    if (cv && (status(cv) === "blocked" || status(cv) === "dead")) badge = ` <span class="badge ${STATUS[status(cv)].cls}">${STATUS[status(cv)].label}</span>`;
    else if (!cv && !groupByLc.has(lc)) badge = ' <span class="badge b-missing">not registered</span>';
    // lists like r_ShadowsCacheResolutions' "3162,2107,1405,936,312" may wrap after a comma; cvar names never wrap
    h += `<tr${changes(vals) ? "" : ' class="same"'}><td><a class="pname" href="${esc(href)}">${esc(n)}</a>${badge}</td>${lv.map((l, i) =>
      `<td data-l="${names[i]}"${i && vals[l] !== vals[lv[i - 1]] ? ' class="chg"' : ""}>${esc(vals[l] ?? "").replace(/,/g, ",<wbr>")}</td>`).join("")}</tr>`;
  }
  return h + "</tbody></table></div></article>";
}

function renderPresets() {
  const from = +$("#p-from").value, to = +$("#p-to").value, all = $("#p-all").checked;
  store.set("presets", { from, to, all });
  const order = [1, 2, 3, 4, 7];
  let lv = order.filter((l) => l >= Math.min(from, to) && l <= Math.max(from, to));
  if (from > to) lv = lv.reverse();
  const groups = Object.keys(D.groups).sort((a, b) => presetTitle(a).localeCompare(presetTitle(b)));
  const menu = groups.filter((g) => menuGroups.has(g.toLowerCase()));
  const other = groups.filter((g) => !menuGroups.has(g.toLowerCase()));
  let total = 0;
  for (const g of menu)
    total += Object.values(D.groups[g].cvars).filter((vals) => lv.some((l, i) => i && vals[l] !== vals[lv[i - 1]])).length;
  $("#p-summary").textContent = lv.length < 2 ? "Pick two different levels to see what changes."
    : `${total} cvar values change between ${LEVELS[from]} and ${LEVELS[to]} across the ${menu.length} video options.`;
  $("#p-list").innerHTML = menu.map((g) => presetCard(g, lv, all)).join("")
    + `<h2 class="p-sep">Not tied to a video option</h2><p class="small muted p-sep-text">The game has these groups too, but no
      menu setting picks their level. Setting one from a config applies its column; Full sets the other groups.</p>`
    + other.map((g) => presetCard(g, lv, all)).join("");
}

function setupPresets() {
  const saved = store.get("presets", null);
  if (saved) {
    for (const k of ["from", "to"]) if ($(`#p-${k} option[value="${saved[k]}"]`)) $("#p-" + k).value = saved[k];
    $("#p-all").checked = !!saved.all;
  }
  $("#tab-presets").addEventListener("change", renderPresets);
  renderPresets();
}

// ---------------------------------------------------------------- boot

fetch("site/data.json")
  .then((r) => { if (!r.ok) throw new Error(r.status); return r.json(); })
  .then((data) => {
    prepare(data);
    $("#loading").remove();
    setupGenerator();
    setupChecker();
    setupDb();
    setupPresets();
    window.addEventListener("hashchange", route);
    route();
  })
  .catch((err) => { $("#loading").textContent = "Couldn't load the cvar data (" + err.message + "). Try reloading."; });
