import { createServer } from "node:http";
import { readFile, writeFile } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const sourceRoot = path.resolve(scriptDir, "../..");
const port = Number.parseInt(process.env.JTDX_WEB_FIXTURE_PORT || "18767", 10);
const defaultReportPath = "local-support/evidence/dependency-audit/live-state-browser-fixture-report.json";
const reportPath = path.resolve(sourceRoot, process.env.JTDX_WEB_FIXTURE_REPORT_PATH || defaultReportPath);
const evidencePrefix = path.resolve(sourceRoot, "local-support/evidence") + path.sep;
if (!reportPath.startsWith(evidencePrefix)) throw new Error("Browser fixture report must stay under local-support/evidence");
let state;
let eventId = 0;
let holdNextRadio = false;
let pendingRadio = [];
let streams = new Set();
let resyncRequests = 0;

function initialState(epoch = "fixture-epoch-A") {
  return {
    server_epoch: epoch,
    state_revision: 1,
    operation_revision: 1,
    application_name: "JTDX · 浏览器夹具",
    application_version: "2.2.159.028",
    instance_id: "fixture",
    online: true,
    mode: "FT8",
    band: "20m",
    frequency_hz: "14074000",
    rig_online: true,
    tx_enabled: false,
    transmitting: false,
    ptt: false,
    tune: false,
    watchdog_timeout: false,
    decoding: false,
    rx_df: 1200,
    tx_df: 1200,
    report: "-10",
    tx_mode: "FT8",
    tx_first: false,
    dx_call: "K1ABC",
    dx_grid: "FN31",
    recent_decodes: [],
    frequency_candidate_limit: 0,
    frequency_candidates: [],
    radio_controls: {
      known: true,
      multi_decode: false,
      agc_compensation: false,
      narrow: false,
      sync: false,
      skip_tx1: false,
      current_tx_index: 1,
      tx_messages: ["CQ N0CALL", "TX2", "TX3", "TX4", "TX5", "TX6"],
      can_log_qso: false,
      qso_draft_open: false,
      qso_generation: 0,
      qso_draft: {}
    },
    operations: [],
    jtdx_time_ms: Date.now(),
    server_monotonic_ms: 0,
    cycle_period_ms: 15000
  };
}

state = initialState();

function sendSnapshot(response, name = "snapshot", data = state) {
  const id = String(++eventId);
  response.write(`id: ${id}\nevent: ${name}\ndata: ${JSON.stringify(data)}\n\n`);
}

function broadcast() {
  for (const response of [...streams]) {
    if (response.destroyed || response.writableEnded) {
      streams.delete(response);
      continue;
    }
    sendSnapshot(response);
  }
}

function json(response, status, value) {
  const body = JSON.stringify(value);
  response.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
    "Content-Length": Buffer.byteLength(body)
  });
  response.end(body);
}

async function bodyJson(request) {
  const parts = [];
  for await (const part of request) parts.push(part);
  return parts.length ? JSON.parse(Buffer.concat(parts).toString("utf8")) : {};
}

function makeReadback(action, value, revision) {
  return {
    confirmed: true,
    state_revision: revision,
    radio_state_known: true,
    safety_known: true,
    tx_enabled: action === "enable-tx" ? value : state.tx_enabled,
    transmitting: false,
    ptt: false,
    tune: false,
    radio_multi_decode: action === "multi-decode" ? value : state.radio_controls.multi_decode,
    radio_sync: action === "sync" ? value : state.radio_controls.sync,
    radio_log_dialog_open: false,
    radio_qso_generation: state.radio_controls.qso_generation
  };
}

function controlResult(request, revision, txEnabled) {
  const readback = makeReadback(request.action, request.value, revision);
  if (request.action === "enable-tx") readback.tx_enabled = txEnabled;
  return {
    request_id: request.request_id,
    server_epoch: request.server_epoch,
    operation: "radio",
    status: "completed",
    state_revision: revision,
    readback
  };
}

async function handle(request, response) {
  const pathname = new URL(request.url, "http://127.0.0.1").pathname;
  if (request.method === "GET" && pathname === "/__fixture") {
    const html = await readFile(path.join(sourceRoot, "tests/web_ui_live_state_fixture.html"));
    response.writeHead(200, {"Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store"});
    response.end(html);
    return;
  }
  if (request.method === "GET" && pathname === "/") {
    const html = await readFile(path.join(sourceRoot, "resources/web-ui/index.html"));
    response.writeHead(200, {"Content-Type": "text/html; charset=utf-8", "Cache-Control": "no-store"});
    response.end(html);
    return;
  }
  if (request.method === "GET" && pathname === "/app.js") {
    const js = await readFile(path.join(sourceRoot, "resources/web-ui/app.js"));
    response.writeHead(200, {"Content-Type": "text/javascript; charset=utf-8", "Cache-Control": "no-store"});
    response.end(js);
    return;
  }
  if (request.method === "GET" && pathname === "/style.css") {
    const css = await readFile(path.join(sourceRoot, "resources/web-ui/style.css"));
    response.writeHead(200, {"Content-Type": "text/css; charset=utf-8", "Cache-Control": "no-store"});
    response.end(css);
    return;
  }
  if (request.method === "GET" && pathname === "/api/v1/state") {
    json(response, 200, state);
    return;
  }
  if (request.method === "GET" && pathname === "/api/v1/events") {
    response.writeHead(200, {
      "Content-Type": "text/event-stream; charset=utf-8",
      "Cache-Control": "no-store",
      "Connection": "keep-alive"
    });
    response.flushHeaders();
    response.on("close", () => streams.delete(response));
    streams.add(response);
    if (request.headers["last-event-id"]) {
      resyncRequests++;
      sendSnapshot(response, "resync_required", {
        reason: "history_not_retained",
        server_epoch: state.server_epoch,
        state_revision: state.state_revision
      });
    }
    sendSnapshot(response);
    return;
  }
  if (request.method === "POST" && pathname === "/api/v1/control/radio") {
    const command = await bodyJson(request);
    if (holdNextRadio) {
      holdNextRadio = false;
      pendingRadio.push({...command, state_revision: state.state_revision, response});
      return;
    }
    if (command.action === "enable-tx") state.tx_enabled = command.value === true;
    if (command.action === "sync") state.radio_controls.sync = command.value === true;
    if (command.action === "multi-decode") state.radio_controls.multi_decode = command.value === true;
    state.state_revision++;
    state.operation_revision++;
    const row = {
      ...controlResult(command, state.state_revision, state.tx_enabled),
      initial_state_revision: command.state_revision || state.state_revision
    };
    state.operations = [row, ...state.operations].slice(0, 16);
    broadcast();
    json(response, 200, row);
    return;
  }
  if (request.method === "POST" && pathname.startsWith("/__fixture/")) {
    const action = pathname.slice("/__fixture/".length);
    const data = await bodyJson(request);
    if (action === "reset") {
      state = initialState();
      pendingRadio = [];
      holdNextRadio = false;
      resyncRequests = 0;
      json(response, 200, {ok: true});
      return;
    }
    if (action === "broadcast") {
      const newEpoch = typeof data.server_epoch === "string" && data.server_epoch !== state.server_epoch;
      if (newEpoch) {
        state = initialState(data.server_epoch);
      } else {
        state.state_revision++;
      }
      if (typeof data.report === "string") state.report = data.report;
      if (typeof data.tx_enabled === "boolean") state.tx_enabled = data.tx_enabled;
      if (data.radio_controls && typeof data.radio_controls === "object")
        state.radio_controls = {...state.radio_controls, ...data.radio_controls};
      broadcast();
      json(response, 200, state);
      return;
    }
    if (action === "arm-hold") {
      holdNextRadio = true;
      json(response, 200, {ok: true});
      return;
    }
    if (action === "pending") {
      json(response, 200, {items: pendingRadio.map(({response: _response, ...item}) => item)});
      return;
    }
    if (action === "release") {
      const pending = pendingRadio.shift();
      if (!pending) { json(response, 409, {error: "no held HTTP request"}); return; }
      const payload = controlResult(pending, Number.isSafeInteger(data.state_revision)
        ? data.state_revision : pending.state_revision, data.tx_enabled === true);
      json(pending.response, 200, payload);
      json(response, 200, {ok: true});
      return;
    }
    if (action === "disconnect") {
      for (const stream of [...streams]) stream.end();
      streams.clear();
      json(response, 200, {ok: true});
      return;
    }
    if (action === "metrics") {
      json(response, 200, {resyncRequests, activeStreams: streams.size, eventId});
      return;
    }
    if (action === "report") {
      const report = {
        captured_at: new Date().toISOString(),
        fixture: "mock HTTP/SSE server; no MainWindow, CAT, PTT, TX, or hardware",
        source_commit: process.env.JTDX_WEB_FIXTURE_SOURCE_COMMIT || "uncommitted",
        ...data
      };
      await writeFile(reportPath, JSON.stringify(report, null, 2) + "\n", "utf8");
      json(response, 200, {ok: true, path: reportPath});
      return;
    }
  }

  response.writeHead(404, {"Content-Type": "text/plain; charset=utf-8"});
  response.end("not found");
}

const server = createServer((request, response) => {
  handle(request, response).catch((error) => {
    if (!response.headersSent) response.writeHead(500, {"Content-Type": "text/plain; charset=utf-8"});
    if (!response.writableEnded) response.end(String(error && error.stack || error));
  });
});

const heartbeatTimer = setInterval(() => {
  for (const response of [...streams]) {
    if (response.destroyed || response.writableEnded) streams.delete(response);
    else response.write(": heartbeat\n\n");
  }
}, 8000);
heartbeatTimer.unref();

server.listen(port, "127.0.0.1", () => {
  process.stdout.write(`JTDX browser fixture ready at http://127.0.0.1:${port}/__fixture\n`);
});

function shutdown() {
  clearInterval(heartbeatTimer);
  for (const stream of streams) stream.end();
  server.close(() => process.exit(0));
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
