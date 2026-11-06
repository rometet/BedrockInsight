#include "bedrock_insight/core/dashboard.hpp"

namespace bedrock_insight {
namespace {

constexpr char kIndexHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>BedrockInsight</title>
  <link rel="stylesheet" href="/styles.css">
</head>
<body>
  <main>
    <header>
      <div>
        <p class="eyebrow">LOCAL SERVER OBSERVABILITY</p>
        <h1>BedrockInsight</h1>
        <p class="muted">Analytics, audit, rollback, and administration.</p>
      </div>
      <form id="connection-form">
        <label for="token">API token</label>
        <input id="token" type="password" autocomplete="off" placeholder="Bearer token">
        <button type="submit">Refresh</button>
      </form>
    </header>
    <p id="error" class="error" role="alert"></p>
    <section class="cards" aria-label="Server metrics">
      <article><span>TPS</span><strong id="tps">—</strong></article>
      <article><span>MSPT average</span><strong id="average-mspt">—</strong></article>
      <article><span>Online</span><strong id="online">—</strong></article>
      <article><span>Peak online</span><strong id="peak-online">—</strong></article>
    </section>
    <section class="panel">
      <div class="panel-heading"><h2>Rankings</h2><span id="ranking-period" class="muted">—</span></div>
      <ol id="rankings" class="ranking-list"><li class="muted">Enter a token to load data.</li></ol>
    </section>
    <section class="panel">
      <div class="panel-heading"><h2>Player profile</h2></div>
      <form id="player-form" class="inline-form">
        <label for="player-id">Player id</label>
        <input id="player-id" required placeholder="xuid-...">
        <button type="submit">Load profile</button>
      </form>
      <pre id="player-profile" class="profile">No profile loaded.</pre>
    </section>
    <section class="panel">
      <div class="panel-heading"><h2>Admin actions</h2><span class="muted">admin role</span></div>
      <ol id="admin-actions" class="ranking-list"><li class="muted">Admin access required.</li></ol>
    </section>
    <footer>Loopback-only dashboard. API requests remain authenticated and role checked.</footer>
  </main>
  <script src="/app.js"></script>
</body>
</html>
)HTML";

constexpr char kStylesCss[] = R"CSS(:root {
  color-scheme: dark;
  font-family: ui-sans-serif, system-ui, sans-serif;
  background: #0d1117;
  color: #e6edf3;
}
* { box-sizing: border-box; }
body { margin: 0; min-height: 100vh; background: radial-gradient(circle at top right, #16324b, #0d1117 55%); }
main { width: min(1100px, calc(100% - 32px)); margin: 0 auto; padding: 42px 0 28px; }
header, .panel-heading, .inline-form { display: flex; gap: 20px; align-items: center; justify-content: space-between; }
h1, h2, p { margin: 0; }
h1 { font-size: clamp(2rem, 5vw, 3.5rem); letter-spacing: -0.05em; }
h2 { font-size: 1.1rem; }
.eyebrow { color: #58a6ff; font-size: .72rem; font-weight: 700; letter-spacing: .16em; margin-bottom: 8px; }
.muted { color: #8b949e; }
form { background: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 14px; }
label { color: #8b949e; font-size: .8rem; }
input { background: #0d1117; border: 1px solid #30363d; border-radius: 7px; color: #e6edf3; padding: 9px 10px; }
button { background: #238636; border: 1px solid #2ea043; border-radius: 7px; color: white; cursor: pointer; padding: 9px 13px; }
.cards { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; margin: 28px 0 14px; }
.cards article, .panel { background: #161b22; border: 1px solid #30363d; border-radius: 12px; }
.cards article { padding: 18px; }
.cards span { color: #8b949e; display: block; font-size: .8rem; }
.cards strong { display: block; font-size: 1.8rem; margin-top: 8px; }
.panel { margin-top: 14px; padding: 20px; }
.ranking-list { line-height: 1.9; margin: 12px 0 0; padding-left: 28px; }
.ranking-list li { display: flex; justify-content: space-between; gap: 16px; }
.inline-form { justify-content: flex-start; margin: 14px 0; padding: 0; background: none; border: 0; }
.profile { background: #0d1117; border-radius: 8px; color: #c9d1d9; margin: 0; min-height: 80px; overflow: auto; padding: 12px; }
.error { color: #f85149; min-height: 1.4em; margin-top: 14px; }
footer { color: #8b949e; font-size: .8rem; margin-top: 22px; }
@media (max-width: 720px) { header { align-items: stretch; flex-direction: column; } .cards { grid-template-columns: repeat(2, 1fr); } .inline-form { align-items: stretch; flex-direction: column; } }
)CSS";

constexpr char kAppJs[] = R"JS((() => {
  const tokenInput = document.querySelector('#token');
  const error = document.querySelector('#error');
  const token = () => tokenInput.value.trim();
  const headers = () => ({ 'Authorization': `Bearer ${token()}` });
  const getJson = async (path) => {
    const response = await fetch(path, { headers: headers() });
    const body = await response.json();
    if (!response.ok) throw new Error(body.error || `HTTP ${response.status}`);
    return body;
  };
  const renderMetrics = (metrics) => {
    document.querySelector('#tps').textContent = metrics.tps ?? '—';
    document.querySelector('#average-mspt').textContent = metrics.average_mspt ?? '—';
    document.querySelector('#online').textContent = metrics.current_online ?? '—';
    document.querySelector('#peak-online').textContent = metrics.peak_online ?? '—';
  };
  const renderRankings = (data) => {
    document.querySelector('#ranking-period').textContent = data.period_key || '—';
    const list = document.querySelector('#rankings');
    list.replaceChildren();
    for (const entry of (data.entries || [])) {
      const item = document.createElement('li');
      const name = document.createElement('span');
      const value = document.createElement('strong');
      name.textContent = `${entry.rank}. ${entry.display_name || entry.player_id}`;
      value.textContent = entry.value;
      item.append(name, value);
      list.append(item);
    }
    if (!list.children.length) list.innerHTML = '<li class="muted">No ranking data.</li>';
  };
  const renderAdminActions = (data) => {
    const list = document.querySelector('#admin-actions');
    list.replaceChildren();
    for (const entry of (data.entries || [])) {
      const item = document.createElement('li');
      const action = document.createElement('span');
      const result = document.createElement('strong');
      action.textContent = `${entry.action} — ${entry.actor_id || 'unknown'}`;
      result.textContent = entry.allowed ? `allowed (${entry.status})` : `denied (${entry.status})`;
      item.append(action, result);
      list.append(item);
    }
    if (!list.children.length) list.innerHTML = '<li class="muted">No admin actions.</li>';
  };
  const refresh = async () => {
    error.textContent = '';
    if (!token()) { error.textContent = 'Enter an API token.'; return; }
    try {
      const [metrics, rankings] = await Promise.all([
        getJson('/api/v1/metrics'), getJson('/api/v1/rankings')
      ]);
      renderMetrics(metrics); renderRankings(rankings);
      try { renderAdminActions(await getJson('/api/v1/admin/actions')); }
      catch (_) { document.querySelector('#admin-actions').innerHTML = '<li class="muted">Admin access required.</li>'; }
    } catch (reason) { error.textContent = reason.message; }
  };
  document.querySelector('#connection-form').addEventListener('submit', (event) => { event.preventDefault(); refresh(); });
  document.querySelector('#player-form').addEventListener('submit', async (event) => {
    event.preventDefault(); error.textContent = '';
    const id = document.querySelector('#player-id').value.trim();
    if (!id) return;
    try {
      const profile = await getJson(`/api/v1/player?id=${encodeURIComponent(id)}`);
      document.querySelector('#player-profile').textContent = JSON.stringify(profile, null, 2);
    } catch (reason) { error.textContent = reason.message; }
  });
})();
)JS";

} // namespace

std::optional<DashboardAsset> dashboard_asset(std::string_view path) {
    if (path == "/" || path == "/index.html") {
        return DashboardAsset{"text/html; charset=utf-8", kIndexHtml};
    }
    if (path == "/styles.css") {
        return DashboardAsset{"text/css; charset=utf-8", kStylesCss};
    }
    if (path == "/app.js") {
        return DashboardAsset{"text/javascript; charset=utf-8", kAppJs};
    }
    return std::nullopt;
}

} // namespace bedrock_insight
