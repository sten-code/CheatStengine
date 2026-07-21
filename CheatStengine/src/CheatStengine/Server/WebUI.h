#pragma once

namespace Server {

    inline constexpr const char* kDashboardHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Cheat Stengine &mdash; MCP</title>
<link rel="icon" href="/favicon.ico"/>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; font-family: "Segoe UI", system-ui, sans-serif;
    background: #14161b; color: #d6d9df; line-height: 1.5;
  }
  header {
    padding: 20px 28px; border-bottom: 1px solid #262a33;
    display: flex; align-items: center; gap: 14px; background: #191c22;
  }
  header h1 { font-size: 18px; margin: 0; font-weight: 600; letter-spacing: .3px; }
  .dot { width: 10px; height: 10px; border-radius: 50%; background: #d64550; }
  .dot.up { background: #3fb950; }
  header .nav { margin-left: auto; font-size: 13px; }
  header .nav a { color: #7fb0ff; text-decoration: none; }
  header .nav a:hover { text-decoration: underline; }
  main { padding: 28px; max-width: 1040px; margin: 0 auto; }
  .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 14px; margin-bottom: 30px; }
  .card { background: #191c22; border: 1px solid #262a33; border-radius: 10px; padding: 16px 18px; }
  .card .label { font-size: 11px; text-transform: uppercase; letter-spacing: .8px; color: #7d838f; }
  .card .value { font-size: 22px; font-weight: 600; margin-top: 6px; }
  section { margin-bottom: 30px; }
  section h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .8px; color: #7d838f; margin: 0 0 12px; }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th, td { text-align: left; padding: 9px 12px; border-bottom: 1px solid #23272f; }
  th { color: #7d838f; font-weight: 500; }
  code { font-family: "JetBrains Mono", Consolas, monospace; color: #7fb0ff; }
  .muted { color: #6b7280; }
  .tools { display: grid; gap: 10px; }
  .toolrow {
    background: #191c22; border: 1px solid #262a33; border-radius: 10px;
    padding: 12px 16px; display: flex; align-items: center; gap: 14px;
  }
  .toolrow .info { flex: 1; min-width: 0; }
  .toolrow .name { font-family: "JetBrains Mono", Consolas, monospace; color: #7fdfa0; font-size: 13px; }
  .toolrow.off .name { color: #6b7280; }
  .toolrow .desc { color: #9aa0ab; font-size: 12.5px; margin-top: 3px; }
  .switch { position: relative; width: 42px; height: 22px; flex-shrink: 0; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider { position: absolute; inset: 0; background: #2b303a; border-radius: 22px; transition: .15s; cursor: pointer; }
  .slider::before { content: ""; position: absolute; width: 16px; height: 16px; left: 3px; top: 3px; background: #d6d9df; border-radius: 50%; transition: .15s; }
  .switch input:checked + .slider { background: #2b6cff; }
  .switch input:checked + .slider::before { transform: translateX(20px); }
  .toolbar { display: flex; gap: 8px; margin-bottom: 12px; }
  button.mini { background: #21252e; color: #d6d9df; border: 1px solid #2b303a; border-radius: 6px; padding: 5px 12px; font-size: 12px; cursor: pointer; }
  button.mini:hover { background: #2b303a; }
</style>
</head>
<body>
<header>
  <span class="dot" id="statusDot"></span>
  <h1>Cheat Stengine &mdash; MCP</h1>
  <span class="nav"><a href="/config.html">Configuration &rarr;</a></span>
</header>
<main>
  <div class="grid">
    <div class="card"><div class="label">Server</div><div class="value" id="serverState">&mdash;</div></div>
    <div class="card"><div class="label">Attached process</div><div class="value" id="procState">&mdash;</div></div>
    <div class="card"><div class="label">Sessions</div><div class="value" id="sessionCount">&mdash;</div></div>
    <div class="card"><div class="label">Active jobs</div><div class="value" id="jobCount">&mdash;</div></div>
  </div>

  <section>
    <h2>Sessions</h2>
    <table><thead><tr><th>ID</th><th>Client</th><th>Version</th><th>Age (s)</th></tr></thead>
    <tbody id="sessionRows"><tr><td colspan="4" class="muted">none</td></tr></tbody></table>
  </section>

  <section>
    <h2>Jobs</h2>
    <table><thead><tr><th>ID</th><th>Label</th><th>Status</th></tr></thead>
    <tbody id="jobRows"><tr><td colspan="3" class="muted">none</td></tr></tbody></table>
  </section>

  <section>
    <h2>Tools</h2>
    <div class="toolbar">
      <button class="mini" onclick="setAll(true)">Enable all</button>
      <button class="mini" onclick="setAll(false)">Disable all</button>
    </div>
    <div class="tools" id="tools"><p class="muted">loading&hellip;</p></div>
  </section>
</main>
<script>
// The dashboard reads the bearer token from /config (loopback-only, no auth) so
// it works without pasting anything. Status, jobs and tools all use that token.
let token = "";

async function bootstrap() {
  try {
    const res = await fetch("/config");
    const c = await res.json();
    token = c.token || "";
  } catch (e) { /* leave token empty; auth may be off anyway */ }
  refresh();
  setInterval(refresh, 2000);
}

function authHeaders() {
  return { "Authorization": "Bearer " + token, "Content-Type": "application/json" };
}

async function refresh() {
  try {
    const res = await fetch("/status", { headers: authHeaders() });
    if (!res.ok) throw new Error("unauthorized");
    const s = await res.json();
    setDot(true);
    document.getElementById("serverState").textContent = "online";
    document.getElementById("procState").textContent = s.attached ? (s.processName || "yes") : "none";
    document.getElementById("sessionCount").textContent = s.sessions.length;
    document.getElementById("jobCount").textContent = s.jobs.filter(j => j.status === "running").length;
    renderSessions(s.sessions);
    renderJobs(s.jobs);
  } catch (e) {
    setDot(false);
    document.getElementById("serverState").textContent = "offline";
  }
  loadTools();
}

function setDot(up) { document.getElementById("statusDot").classList.toggle("up", up); }

function renderSessions(sessions) {
  const body = document.getElementById("sessionRows");
  if (!sessions.length) { body.innerHTML = '<tr><td colspan="4" class="muted">none</td></tr>'; return; }
  body.innerHTML = sessions.map(s =>
    `<tr><td><code>${s.id}</code></td><td>${esc(s.client)}</td><td>${esc(s.version)}</td><td>${s.ageSeconds}</td></tr>`
  ).join("");
}

function renderJobs(jobs) {
  const body = document.getElementById("jobRows");
  if (!jobs.length) { body.innerHTML = '<tr><td colspan="3" class="muted">none</td></tr>'; return; }
  body.innerHTML = jobs.map(j =>
    `<tr><td>${j.id}</td><td>${esc(j.label)}</td><td>${esc(j.status)}</td></tr>`
  ).join("");
}

async function loadTools() {
  try {
    const res = await fetch("/tools", { headers: authHeaders() });
    if (!res.ok) throw new Error();
    const data = await res.json();
    const tools = data.tools || [];
    document.getElementById("tools").innerHTML = tools.map(t =>
      `<div class="toolrow ${t.enabled ? "" : "off"}">
        <div class="info">
          <div class="name">${esc(t.name)}</div>
          <div class="desc">${esc(t.description)}</div>
        </div>
        <label class="switch">
          <input type="checkbox" ${t.enabled ? "checked" : ""} onchange="toggle('${esc(t.name)}', this.checked)"/>
          <span class="slider"></span>
        </label>
      </div>`
    ).join("");
  } catch (e) {
    document.getElementById("tools").innerHTML = '<p class="muted">could not load tools</p>';
  }
}

async function toggle(name, enabled) {
  await fetch("/tools/toggle", {
    method: "POST", headers: authHeaders(),
    body: JSON.stringify({ name, enabled })
  });
  loadTools();
}

async function setAll(enabled) {
  const res = await fetch("/tools", { headers: authHeaders() });
  const data = await res.json();
  await Promise.all((data.tools || []).map(t =>
    fetch("/tools/toggle", {
      method: "POST", headers: authHeaders(),
      body: JSON.stringify({ name: t.name, enabled })
    })));
  loadTools();
}

function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"]/g, c =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

bootstrap();
</script>
</body>
</html>)HTML";

    inline constexpr const char* kConfigHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Cheat Stengine &mdash; MCP Config</title>
<link rel="icon" href="/favicon.ico"/>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body {
    margin: 0; font-family: "Segoe UI", system-ui, sans-serif;
    background: #14161b; color: #d6d9df; line-height: 1.5;
  }
  header {
    padding: 20px 28px; border-bottom: 1px solid #262a33;
    display: flex; align-items: center; gap: 14px; background: #191c22;
  }
  header h1 { font-size: 18px; margin: 0; font-weight: 600; letter-spacing: .3px; }
  header .nav { margin-left: auto; font-size: 13px; }
  header .nav a { color: #7fb0ff; text-decoration: none; }
  header .nav a:hover { text-decoration: underline; }
  main { padding: 28px; max-width: 900px; margin: 0 auto; }
  section { margin-bottom: 30px; }
  section h2 { font-size: 13px; text-transform: uppercase; letter-spacing: .8px; color: #7d838f; margin: 0 0 12px; }
  .panel { background: #191c22; border: 1px solid #262a33; border-radius: 10px; padding: 18px 20px; }
  .row { display: flex; align-items: center; gap: 10px; margin-bottom: 12px; }
  .row:last-child { margin-bottom: 0; }
  .row .k { width: 130px; font-size: 12px; color: #7d838f; text-transform: uppercase; letter-spacing: .6px; }
  .row .v { flex: 1; font-family: "JetBrains Mono", Consolas, monospace; font-size: 13px; color: #7fb0ff; word-break: break-all; }
  .copy { background: #21252e; color: #d6d9df; border: 1px solid #2b303a; border-radius: 6px; padding: 5px 10px; font-size: 12px; cursor: pointer; }
  .copy:hover { background: #2b303a; }
  .toggle { display: flex; align-items: center; gap: 12px; }
  .switch { position: relative; width: 42px; height: 22px; }
  .switch input { opacity: 0; width: 0; height: 0; }
  .slider { position: absolute; inset: 0; background: #2b303a; border-radius: 22px; transition: .15s; }
  .slider::before { content: ""; position: absolute; width: 16px; height: 16px; left: 3px; top: 3px; background: #d6d9df; border-radius: 50%; transition: .15s; }
  .switch input:checked + .slider { background: #2b6cff; }
  .switch input:checked + .slider::before { transform: translateX(20px); }
  .targets { display: grid; gap: 12px; }
  .target { background: #191c22; border: 1px solid #262a33; border-radius: 10px; padding: 14px 16px; display: flex; align-items: center; gap: 14px; }
  .target .info { flex: 1; min-width: 0; }
  .target .label { font-size: 14px; font-weight: 500; }
  .target .path { font-family: "JetBrains Mono", Consolas, monospace; font-size: 12px; color: #7d838f; margin-top: 3px; word-break: break-all; }
  .badge { font-size: 11px; padding: 3px 9px; border-radius: 20px; white-space: nowrap; }
  .badge.on { background: rgba(63,185,80,.15); color: #3fb950; }
  .badge.off { background: rgba(125,131,143,.15); color: #9aa0ab; }
  .badge.na { background: rgba(214,69,80,.12); color: #d64550; }
  button.install { background: #2b6cff; color: #fff; border: none; border-radius: 7px; padding: 8px 16px; font-size: 13px; cursor: pointer; font-weight: 500; white-space: nowrap; }
  button.install:hover { background: #3f7bff; }
  button.install:disabled { background: #262a33; color: #6b7280; cursor: not-allowed; }
  .toast { position: fixed; bottom: 24px; left: 50%; transform: translateX(-50%); background: #21252e; border: 1px solid #2b303a; color: #d6d9df; padding: 10px 18px; border-radius: 8px; font-size: 13px; opacity: 0; transition: opacity .2s; pointer-events: none; }
  .toast.show { opacity: 1; }
  .muted { color: #6b7280; font-size: 13px; }
</style>
</head>
<body>
<header>
  <h1>Cheat Stengine &mdash; MCP Configuration</h1>
  <span class="nav"><a href="/">&larr; Dashboard</a></span>
</header>
<main>
  <section>
    <h2>Connection</h2>
    <div class="panel">
      <div class="row"><span class="k">Endpoint</span><span class="v" id="endpoint">&mdash;</span><button class="copy" onclick="copy('endpoint')">Copy</button></div>
      <div class="row"><span class="k">Bearer token</span><span class="v" id="token">&mdash;</span><button class="copy" onclick="copy('token')">Copy</button></div>
      <div class="row toggle">
        <span class="k">Require auth</span>
        <label class="switch"><input type="checkbox" id="authToggle" onchange="setAuth(this.checked)"/><span class="slider"></span></label>
        <span class="muted" id="authHint"></span>
      </div>
    </div>
  </section>

  <section>
    <h2>Install targets</h2>
    <div class="targets" id="targets"><p class="muted">loading&hellip;</p></div>
  </section>
</main>
<div class="toast" id="toast"></div>
<script>
async function load() {
  const res = await fetch("/config");
  const c = await res.json();

  document.getElementById("endpoint").textContent = c.endpoint;
  document.getElementById("token").textContent = c.token;
  document.getElementById("authToggle").checked = c.authRequired;
  document.getElementById("authHint").textContent = c.authRequired
    ? "Clients must send the bearer token."
    : "Loopback only — any local client can connect.";

  const box = document.getElementById("targets");
  if (!c.targets.length) { box.innerHTML = '<p class="muted">no targets</p>'; return; }
  box.innerHTML = c.targets.map(t => {
    const badge = !t.available ? '<span class="badge na">unavailable</span>'
      : t.installed ? '<span class="badge on">installed</span>'
      : '<span class="badge off">not installed</span>';
    const btn = t.available
      ? `<button class="install" onclick="install('${t.key}', this)">${t.installed ? "Reinstall" : "Install"}</button>`
      : '<button class="install" disabled>Install</button>';
    return `<div class="target">
      <div class="info">
        <div class="label">${esc(t.label)}</div>
        <div class="path">${esc(t.path || "path unavailable")}</div>
      </div>
      ${badge}${btn}
    </div>`;
  }).join("");
}

async function install(key, btn) {
  btn.disabled = true; btn.textContent = "Installing…";
  try {
    const res = await fetch("/install", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ target: key })
    });
    const data = await res.json();
    toast(data.ok ? "Installed " + key : "Failed to install " + key);
  } catch (e) {
    toast("Install request failed");
  }
  load();
}

async function setAuth(required) {
  try {
    await fetch("/auth", {
      method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ required })
    });
    toast(required ? "Auth required" : "Auth disabled");
  } catch (e) {
    toast("Could not change auth");
  }
  load();
}

function copy(id) {
  const text = document.getElementById(id).textContent;
  navigator.clipboard.writeText(text).then(() => toast("Copied"));
}

let toastTimer;
function toast(msg) {
  const el = document.getElementById("toast");
  el.textContent = msg; el.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.remove("show"), 1800);
}

function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"]/g, c =>
    ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
}

load();
</script>
</body>
</html>)HTML";

}
