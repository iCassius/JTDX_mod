(function () {
  "use strict";

  const MAX_OPERATION_ROWS = 128;
  const DISPLAY_OPERATION_ROWS = 20;
  let token = "";
  let lastId = "";
  let lastUpdate = 0;
  let controller = null;
  let running = false;
  let runId = 0;
  let operationEpoch = null;
  let operationRows = [];
  let currentSnapshot = null;
  let frequencyRequest = null;
  let frequencyUnknown = false;
  let frequencyAbort = null;
  let connectionSession = 0;
  let connected = false;

  const el = (id) => document.getElementById(id);
  const text = (id, value) => {
    const node = el(id);
    if (node) node.textContent = value == null ? "未知" : String(value);
  };

  if (location.hash === "#fixture") el("demo_notice").hidden = false;

  function setOperationsFreshness(kind, message) {
    const card = el("operations_card");
    const meta = el("operations_meta");
    if (!card || !meta) return;
    card.classList.toggle("stale", kind === "stale");
    card.classList.toggle("unknown", kind === "unknown");
    meta.textContent = message;
  }

  function setConnected(yes) {
    connected = yes;
    const connection = el("connection");
    connection.textContent = yes ? "已连接" : "未连接";
    connection.classList.toggle("ok", yes);
    if (!yes) {
      if (lastUpdate) {
        text("online", "未连接（旧快照）");
        text("freshness", "连接断开");
        text("frequency_freshness", "连接断开");
        setOperationsFreshness("stale", "连接断开，保留旧操作快照");
      } else {
        setOperationsFreshness("unknown", "未取得操作快照");
      }
    }
    updateFrequencyForm();
  }

  function bool(value) {
    return value == null ? "未知" : value ? "是" : "否";
  }

  function age(value) {
    if (value == null) return null;
    const timestamp = Date.parse(value);
    if (!Number.isFinite(timestamp)) return null;
    return Math.max(0, Math.round((Date.now() - timestamp) / 1000)) + " 秒前";
  }

  function boundedString(value, limit) {
    if (typeof value !== "string" || value.length === 0) return "未知";
    return value.length > limit ? value.slice(0, limit) + "…" : value;
  }

  function integerValue(value) {
    if (typeof value === "number" && Number.isSafeInteger(value)) return value;
    if (typeof value === "string" && /^-?\d+$/.test(value)) {
      const parsed = Number(value);
      if (Number.isSafeInteger(parsed)) return parsed;
    }
    return null;
  }

  function canonicalHz(value) {
    if (typeof value !== "string" || !/^\d+$/.test(value)) return null;
    const normalized = value.replace(/^0+/, "");
    return normalized || null;
  }

  function frequencyInputToHz(value) {
    if (typeof value !== "string" || !/^(?:0|[1-9]\d*)(?:\.\d{1,6})?$/.test(value)) return null;
    const parts = value.split(".");
    const whole = parts[0];
    const fraction = (parts[1] || "").padEnd(6, "0");
    const hz = (whole + fraction).replace(/^0+/, "");
    return hz && hz.length <= 19 ? hz : null;
  }

  function secureRequestId() {
    if (globalThis.crypto && typeof globalThis.crypto.randomUUID === "function") {
      return globalThis.crypto.randomUUID();
    }
    if (!globalThis.crypto || typeof globalThis.crypto.getRandomValues !== "function") {
      throw new Error("secure_random_unavailable");
    }
    const bytes = new Uint8Array(16);
    globalThis.crypto.getRandomValues(bytes);
    bytes[6] = (bytes[6] & 0x0f) | 0x40;
    bytes[8] = (bytes[8] & 0x3f) | 0x80;
    const hex = Array.from(bytes, (byte) => byte.toString(16).padStart(2, "0")).join("");
    return hex.slice(0, 8) + "-" + hex.slice(8, 12) + "-" + hex.slice(12, 16)
      + "-" + hex.slice(16, 20) + "-" + hex.slice(20);
  }

  function frequencyStatus(message, kind) {
    const node = el("frequency_result");
    if (!node) return;
    node.textContent = message;
    node.className = "frequency-result " + (kind || "");
  }

  function operationForRequest(snapshot) {
    if (!frequencyRequest || !snapshot || !Array.isArray(snapshot.operations)) return null;
    return snapshot.operations.find((row) => row && typeof row === "object"
      && row.operation === "frequency"
      && row.request_id === frequencyRequest.requestId
      && row.server_epoch === frequencyRequest.epoch) || null;
  }

  function readbackMatches(row, targetHz) {
    const readback = row && row.readback && typeof row.readback === "object" ? row.readback : null;
    return !!(readback && readback.confirmed === true
      && canonicalHz(readback.frequency_hz) === targetHz);
  }

  function terminalOperation(row) {
    return !!(row && ["completed", "failed", "rejected", "timeout"].includes(row.status));
  }

  function reconcileFrequency(snapshot) {
    if (!frequencyRequest || !snapshot) return;
    const epoch = typeof snapshot.server_epoch === "string" ? snapshot.server_epoch : "";
    if (epoch && epoch !== frequencyRequest.epoch) {
      frequencyRequest = null;
      frequencyUnknown = false;
      frequencyStatus("服务 epoch 已变化，旧频率请求已失效；请以新快照为准。", "warning");
      return;
    }
    const row = operationForRequest(snapshot);
    if (row && terminalOperation(row)) {
      if (row.status === "completed" && readbackMatches(row, frequencyRequest.targetHz)) {
        frequencyStatus("频率已完成，并已由实际回读确认。", "success");
        frequencyUnknown = false;
        frequencyRequest = null;
      } else if (row.status === "completed") {
        frequencyUnknown = true;
        frequencyStatus("服务报告完成，但实际频率回读与目标不匹配，结果未知。", "warning");
      } else {
        frequencyUnknown = false;
        frequencyStatus("频率请求" + (row.reason ? "：" + boundedString(row.reason, 180) : "未完成。"), "error");
        frequencyRequest = null;
      }
      return;
    }
    updateFrequencyForm();
  }

  function frequencyGateReason(snapshot) {
    if (!snapshot) return "等待新鲜状态快照";
    if (snapshot.frequency_control_enabled !== true) return "桌面尚未开放频率控制";
    if (!connected) return "等待连接和最新状态快照";
    if (snapshot.online !== true) return "主程序未在线";
    if (snapshot.rig_online !== true) return "电台连接未在线";
    if (snapshot.rig_fresh !== true || snapshot.freshness !== "fresh") return "实际频率或状态快照陈旧";
    if (snapshot.tx_enabled !== false) return "TX 允许状态不是明确安全值";
    if (snapshot.transmitting !== false) return "当前正在发射或发射状态未知";
    if (snapshot.ptt !== false) return "PTT 状态不是明确关闭";
    if (snapshot.watchdog_timeout !== false) return "看门狗状态不是明确安全值";
    if (typeof snapshot.server_epoch !== "string" || snapshot.server_epoch.length === 0) return "缺少有效服务 epoch";
    if (integerValue(snapshot.state_revision) == null || integerValue(snapshot.state_revision) < 0) return "缺少安全状态 revision";
    if (!lastUpdate || Date.now() - lastUpdate > 15000) return "状态快照已超时";
    if (frequencyUnknown) return "上一次请求结果未知，等待明确回读或新 epoch";
    if (frequencyRequest) return "已有频率请求处理中";
    return "";
  }

  function updateFrequencyForm() {
    const button = el("frequency_send");
    const input = el("frequency_input");
    const reason = el("frequency_control_status");
    if (!button || !input || !reason) return;
    const targetHz = frequencyInputToHz(input.value);
    const gate = frequencyGateReason(currentSnapshot);
    const inputReason = targetHz ? "" : "请输入正的十进制 MHz，最多 6 位小数";
    const disabledReason = gate || inputReason;
    button.disabled = !!disabledReason;
    reason.textContent = disabledReason || "可以发送；服务端仍会进行最终校验";
    reason.className = "frequency-control-status " + (disabledReason ? "blocked" : "ready");
    const label = el("access_mode");
    if (label) label.textContent = currentSnapshot && currentSnapshot.frequency_control_enabled === true
      ? "频率控制已开放 · 其他控制未开放" : "按能力开放 · 当前仅只读";
  }

  function clearRenderedSnapshot(message) {
    currentSnapshot = null;
    lastUpdate = 0;
    lastId = "";
    operationEpoch = null;
    operationRows = [];
    ["web_server_state", "application_name", "mode_band", "instance_id", "online", "frequency",
      "freshness", "frequency_freshness", "last_status_update", "last_decode_update", "decode_age",
      "dx_call", "dx_grid", "report", "df", "tx_mode", "tx_enabled", "transmitting", "decoding",
      "tx_first", "watchdog_timeout", "cq_qso", "auto_sequence_state", "current_tx_text", "decode_count"]
      .forEach((id) => text(id, null));
    const box = el("decodes");
    if (box) box.replaceChildren();
    renderOperations("unknown", message || "等待新令牌对应的状态快照");
    updateFrequencyForm();
  }

  function operationLabel(value) {
    if (value === "frequency") return "频率";
    if (value === "select-dx") return "选择 DX";
    return "未知";
  }

  function statusLabel(row) {
    const status = typeof row.status === "string" ? row.status : "";
    const readback = row.readback && typeof row.readback === "object" ? row.readback : null;
    if (status === "accepted" || status === "pending") {
      return { label: "处理中", className: "processing" };
    }
    if (status === "completed") {
      return readback && readback.confirmed === true
        ? { label: "已完成（已确认）", className: "completed" }
        : { label: "未知（完成未确认）", className: "unknown" };
    }
    if (status === "received") return { label: "已接收", className: "processing" };
    if (status === "failed") return { label: "失败", className: "failed" };
    if (status === "rejected") return { label: "已拒绝", className: "rejected" };
    if (status === "timeout") return { label: "超时", className: "timeout" };
    return { label: "未知", className: "unknown" };
  }

  function confirmedFrequency(row, status) {
    const readback = row.readback && typeof row.readback === "object" ? row.readback : null;
    if (row.operation !== "frequency" || status !== "completed" || !readback
        || readback.confirmed !== true || readback.frequency_known !== true) {
      return "未知";
    }
    const value = typeof readback.frequency_hz === "string"
      && /^\d+$/.test(readback.frequency_hz) ? Number(readback.frequency_hz) : null;
    if (!Number.isSafeInteger(value) || value <= 0) return "未知";
    return (value / 1000000).toFixed(6) + " MHz";
  }

  function operationElapsed(row) {
    const received = integerValue(row.received_ms);
    const completed = integerValue(row.completed_ms);
    if (received == null || completed == null || received < 0 || completed < received) return "未知";
    return (completed - received) + " ms";
  }

  function appendField(list, label, value) {
    const term = document.createElement("dt");
    term.textContent = label;
    const description = document.createElement("dd");
    description.textContent = value;
    list.appendChild(term);
    list.appendChild(description);
  }

  function renderOperations(kind, message) {
    const box = el("operations");
    if (!box) return;
    setOperationsFreshness(kind, message);
    box.replaceChildren();
    const visibleRows = operationRows.slice(-DISPLAY_OPERATION_ROWS).reverse();
    if (visibleRows.length === 0) {
      const empty = document.createElement("p");
      empty.className = "operation-empty";
      empty.textContent = kind === "unknown" ? "操作结果未知。" : "当前快照没有操作结果。";
      box.appendChild(empty);
      return;
    }
    visibleRows.forEach((row) => {
      const item = document.createElement("article");
      item.className = "operation-row";
      const heading = document.createElement("div");
      heading.className = "operation-heading";
      const title = document.createElement("strong");
      title.textContent = boundedString(row.request_id, 96);
      const status = statusLabel(row);
      const badge = document.createElement("span");
      badge.className = "operation-status " + status.className;
      badge.textContent = status.label;
      heading.appendChild(title);
      heading.appendChild(badge);
      item.appendChild(heading);

      const fields = document.createElement("dl");
      appendField(fields, "操作名", operationLabel(row.operation));
      appendField(fields, "原因", boundedString(row.reason, 160));
      appendField(fields, "确认频率", confirmedFrequency(row, row.status));
      appendField(fields, "操作耗时", operationElapsed(row));
      item.appendChild(fields);
      box.appendChild(item);
    });
  }

  function updateOperations(snapshot) {
    const epoch = typeof snapshot.server_epoch === "string" && snapshot.server_epoch.length > 0
      ? snapshot.server_epoch : null;
    if (!epoch) {
      operationEpoch = null;
      operationRows = [];
      renderOperations("unknown", "当前快照缺少服务 epoch");
      return;
    }
    if (operationEpoch !== epoch) operationRows = [];
    operationEpoch = epoch;
    if (!Array.isArray(snapshot.operations)) {
      operationRows = [];
      renderOperations("unknown", "当前快照缺少操作结果");
      return;
    }
    const incoming = snapshot.operations.slice(0, MAX_OPERATION_ROWS);
    operationRows = incoming.filter((row) => row && typeof row === "object"
      && row.server_epoch === epoch);
    const freshness = snapshot.freshness === "fresh" ? "fresh"
      : snapshot.freshness === "stale" ? "stale" : "unknown";
    const message = freshness === "stale" ? "服务状态陈旧，以下为旧操作快照"
      : freshness === "unknown" ? "操作快照新鲜度未知"
      : "当前服务 epoch · 保留 " + operationRows.length + " 条，显示最近 "
        + Math.min(operationRows.length, DISPLAY_OPERATION_ROWS) + " 条";
    renderOperations(freshness, message);
    reconcileFrequency(snapshot);
  }

  function render(snapshot) {
    if (!snapshot || typeof snapshot !== "object") return;
    currentSnapshot = snapshot;
    setConnected(true);
    lastUpdate = Date.now();
    const stale = snapshot.freshness === "stale";
    const rigStale = snapshot.rig_fresh === false;
    const rigState = snapshot.rig_fresh === true ? "实际频率新鲜"
      : rigStale ? "实际频率陈旧" : "实际频率未知";
    text("last_update", "流更新时间：" + (snapshot.generated_at || "未知"));
    text("web_server_state", snapshot.web_server_state);
    text("application_name", snapshot.application_name);
    text("mode_band", [snapshot.mode, snapshot.band].filter(Boolean).join(" / ") || "未知");
    text("instance_id", snapshot.instance_id);
    text("online", snapshot.online == null ? null : snapshot.online
      ? stale ? "在线（数据陈旧）" : "在线" : "离线");
    text("frequency", snapshot.frequency == null ? "未取得实际频率"
      : (Number(snapshot.frequency) / 1000000).toFixed(6) + " MHz");
    text("freshness", stale ? "陈旧" : snapshot.freshness === "fresh" ? "新鲜" : snapshot.freshness);
    text("frequency_freshness", rigState);
    text("last_status_update", snapshot.last_status_update);
    text("last_decode_update", snapshot.last_decode_update);
    text("decode_age", age(snapshot.last_decode_update));
    text("dx_call", snapshot.dx_call);
    text("dx_grid", snapshot.dx_grid);
    text("report", snapshot.report);
    text("df", (snapshot.rx_df == null ? "未知" : snapshot.rx_df) + " / "
      + (snapshot.tx_df == null ? "未知" : snapshot.tx_df));
    text("tx_mode", snapshot.tx_mode);
    text("tx_enabled", bool(snapshot.tx_enabled));
    text("transmitting", bool(snapshot.transmitting));
    text("decoding", bool(snapshot.decoding));
    text("tx_first", bool(snapshot.tx_first));
    text("watchdog_timeout", bool(snapshot.watchdog_timeout));
    text("cq_qso", [snapshot.cq_state, snapshot.qso_stage].filter(Boolean).join(" / ") || "未知");
    text("auto_sequence_state", snapshot.auto_sequence_state);
    text("current_tx_text", snapshot.current_tx_text);

    const rows = Array.isArray(snapshot.recent_decodes) ? snapshot.recent_decodes : [];
    text("decode_count", rows.length);
    const box = el("decodes");
    box.replaceChildren();
    rows.slice().reverse().forEach((decode) => {
      const row = document.createElement("div");
      row.className = "decode";
      [[decode.time, ""], [decode.snr, ""], [decode.delta_frequency, ""], [decode.mode, ""],
        [decode.callsign, ""], [decode.grid, ""], [decode.fresh ? "新鲜" : "陈旧", ""],
        [decode.is_new ? "新" : "旧", ""], [decode.message, "text"]].forEach(([value, className]) => {
        const node = document.createElement(className ? "b" : "span");
        node.className = className;
        node.textContent = value == null ? "未知" : String(value);
        row.appendChild(node);
      });
      box.appendChild(row);
    });
    updateOperations(snapshot);
    updateFrequencyForm();
  }

  function auth() {
    return token ? { Authorization: "Bearer " + token } : {};
  }

  function responseIdentityMatches(payload, request) {
    return !!(payload && typeof payload === "object"
      && payload.request_id === request.requestId
      && payload.server_epoch === request.epoch);
  }

  function responseReason(payload, fallback) {
    return payload && typeof payload.reason === "string" && payload.reason.length > 0
      ? payload.reason : fallback;
  }

  function settleFrequencyResponse(payload, request) {
    if (!responseIdentityMatches(payload, request)) {
      frequencyUnknown = true;
      frequencyStatus("响应无法与本次请求安全匹配，结果未知；等待回读或新 epoch。", "warning");
      updateFrequencyForm();
      return;
    }
    const status = typeof payload.status === "string" ? payload.status : "";
    if (status === "accepted" || status === "pending" || status === "received") {
      request.state = "pending";
      frequencyStatus("请求已登记，等待实际频率回读；HTTP 响应不代表完成。", "processing");
      updateFrequencyForm();
      return;
    }
    if (status === "completed") {
      if (readbackMatches(payload, request.targetHz)) {
        frequencyStatus("频率已完成，并已由实际回读确认。", "success");
        frequencyRequest = null;
      } else {
        frequencyUnknown = true;
        frequencyStatus("服务报告完成，但缺少与目标匹配的实际回读，结果未知。", "warning");
      }
      updateFrequencyForm();
      return;
    }
    if (["failed", "rejected", "timeout"].includes(status)) {
      frequencyRequest = null;
      frequencyUnknown = false;
      frequencyStatus("频率请求" + (status === "rejected" ? "已拒绝" : "未完成")
        + "：" + responseReason(payload, "服务未提供原因"), "error");
      updateFrequencyForm();
      return;
    }
    frequencyUnknown = true;
    frequencyStatus("响应状态未知，等待实际回读或新 epoch。", "warning");
    updateFrequencyForm();
  }

  async function sendFrequency() {
    const targetHz = frequencyInputToHz(el("frequency_input").value);
    const gate = frequencyGateReason(currentSnapshot);
    if (!targetHz || gate) {
      updateFrequencyForm();
      return;
    }
    let requestId;
    try {
      requestId = secureRequestId();
    } catch (_) {
      frequencyStatus("浏览器没有可用的安全随机源，无法发送请求。", "error");
      return;
    }
    const request = {
      requestId,
      epoch: currentSnapshot.server_epoch,
      targetHz,
      session: connectionSession,
      state: "sending"
    };
    frequencyRequest = request;
    frequencyUnknown = false;
    frequencyStatus("正在发送频率请求…", "processing");
    updateFrequencyForm();
    const local = new AbortController();
    frequencyAbort = local;
    const timer = setTimeout(() => local.abort(), 5000);
    try {
      const response = await fetch("/api/v1/control/frequency", {
        method: "POST",
        headers: Object.assign({ "Content-Type": "application/json", "Accept": "application/json" }, auth()),
        body: JSON.stringify({
          request_id: request.requestId,
          server_epoch: request.epoch,
          state_revision: integerValue(currentSnapshot.state_revision),
          frequency_hz: request.targetHz
        }),
        cache: "no-store",
        signal: local.signal
      });
      if (request.session !== connectionSession || frequencyRequest !== request) return;
      let payload = null;
      try { payload = await response.json(); } catch (_) { payload = null; }
      if (request.session !== connectionSession || frequencyRequest !== request) return;
      if (!payload || typeof payload !== "object") {
        frequencyUnknown = true;
        frequencyStatus("服务响应无法解析，结果未知；等待回读或新 epoch。", "warning");
        updateFrequencyForm();
      } else {
        settleFrequencyResponse(payload, request);
      }
      if (!response.ok && responseIdentityMatches(payload, request)
          && !["failed", "rejected", "timeout"].includes(payload.status)) {
        frequencyUnknown = true;
        frequencyStatus("服务拒绝请求：" + responseReason(payload, "HTTP " + response.status), "error");
        updateFrequencyForm();
      }
    } catch (error) {
      if (request.session !== connectionSession || frequencyRequest !== request) return;
      frequencyUnknown = true;
      frequencyStatus(error && error.name === "AbortError"
        ? "请求超时，结果未知；等待明确回读或新 epoch。"
        : "传输异常，结果未知；等待明确回读或新 epoch。", "warning");
      updateFrequencyForm();
    } finally {
      clearTimeout(timer);
      if (frequencyAbort === local) frequencyAbort = null;
    }
  }

  function timedRead(reader, run) {
    let timer;
    const timeout = new Promise((_, reject) => {
      timer = setTimeout(() => reject(new Error("watchdog")), 15000);
    });
    return Promise.race([reader.read(), timeout]).finally(() => clearTimeout(timer)).then((value) => {
      if (run !== runId) throw new Error("superseded");
      return value;
    });
  }

  async function stream(run) {
    while (running && run === runId) {
      const local = new AbortController();
      controller = local;
      try {
        const headers = auth();
        if (lastId) headers["Last-Event-ID"] = lastId;
        const response = await fetch("/api/v1/events", {
          headers, cache: "no-store", signal: local.signal
        });
        if (run !== runId) throw new Error("superseded");
        if (response.status === 401) {
          el("auth_error").textContent = "令牌无效";
          running = false;
          setConnected(false);
          break;
        }
        if (!response.ok || !response.body) throw new Error("HTTP " + response.status);
        setConnected(true);
        const reader = response.body.getReader();
        const decoder = new TextDecoder();
        let buffer = "";
        while (running && run === runId) {
          const next = await timedRead(reader, run);
          if (next.done) throw new Error("stream ended");
          buffer += decoder.decode(next.value, { stream: true });
          const parts = buffer.split("\n\n");
          buffer = parts.pop();
          for (const part of parts) {
            const id = part.split("\n").find((line) => line.startsWith("id: "));
            if (id) lastId = id.slice(4);
            const event = (part.split("\n").find((line) => line.startsWith("event: ")) || "").slice(7);
            if (event !== "snapshot") continue;
            const line = part.split("\n").find((item) => item.startsWith("data: "));
            if (line) {
              try {
                render(JSON.parse(line.slice(6)));
              } catch (_) {
                // Malformed snapshots are ignored; the last bounded snapshot remains visible.
              }
            }
          }
        }
      } catch (_) {
        if (run === runId) {
          setConnected(false);
          if (running) await new Promise((resolve) => setTimeout(resolve, 1000));
        }
      } finally {
        local.abort();
        if (run === runId) controller = null;
      }
    }
  }

  el("connect_button").addEventListener("click", () => {
    runId++;
    connectionSession++;
    running = false;
    if (controller) controller.abort();
    if (frequencyAbort) frequencyAbort.abort();
    if (frequencyRequest) {
      frequencyUnknown = true;
      frequencyStatus("会话已更换，旧频率请求结果未知；等待匹配回读或新 epoch。", "warning");
    } else {
      frequencyUnknown = false;
    }
    token = el("token_input").value;
    el("auth_error").textContent = "";
    clearRenderedSnapshot("令牌已更换，等待对应会话的状态快照");
    setConnected(false);
    running = true;
    stream(runId);
  });

  el("frequency_send").addEventListener("click", sendFrequency);
  el("frequency_input").addEventListener("input", updateFrequencyForm);

  setInterval(() => {
    if (lastUpdate && Date.now() - lastUpdate > 15000) setConnected(false);
    updateFrequencyForm();
  }, 1000);
  updateFrequencyForm();
}());
