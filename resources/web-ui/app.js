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
  }

  function render(snapshot) {
    if (!snapshot || typeof snapshot !== "object") return;
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
  }

  function auth() {
    return token ? { Authorization: "Bearer " + token } : {};
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
    running = false;
    if (controller) controller.abort();
    token = el("token_input").value;
    el("auth_error").textContent = "";
    running = true;
    stream(runId);
  });

  setInterval(() => {
    if (lastUpdate && Date.now() - lastUpdate > 15000) setConnected(false);
  }, 1000);
}());
