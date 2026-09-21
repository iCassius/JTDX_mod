(function () {
  "use strict";

  const MAX_OPERATION_ROWS = 128;
  const DISPLAY_OPERATION_ROWS = 20;
  let lastId = "";
  let lastUpdate = 0;
  let controller = null;
  let running = false;
  let runId = 0;
  let operationEpoch = null;
  let operationRows = [];
  let currentSnapshot = null;
  let qsoDraftEditGeneration = null;
  let qsoDraftDirty = false;
  const txEditTimers = {};
  let frequencyRequest = null;
  let frequencyUnknown = false;
  let frequencyAbort = null;
  let dxRequest = null;
  let dxUnknown = false;
  let dxAbort = null;
  let businessRequest = null;
  let businessUnknown = false;
  let businessAbort = null;
  let radioRequest = null;
  let radioUnknown = false;
  let radioAbort = null;
  let confirmationResolver = null;
  let confirmationPreviousFocus = null;
  let connectionSession = 0;
  let connected = false;
  let frequencyCandidates = [];

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
    updateDxControls();
    updateBusinessControls();
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

  function frequencyHzToMhz(value) {
    const hz = canonicalHz(value);
    if (!hz) return null;
    const padded = hz.padStart(7, "0");
    return padded.slice(0, -6) + "." + padded.slice(-6);
  }

  function frequencyCandidateLess(lhs, rhs) {
    return lhs.hz.length === rhs.hz.length ? lhs.hz < rhs.hz : lhs.hz.length < rhs.hz.length;
  }

  function addOption(select, value, label) {
    const option = document.createElement("option");
    option.value = value;
    option.textContent = label;
    select.appendChild(option);
  }

  function updateFrequencyChoices(snapshot) {
    const bandSelect = el("frequency_band");
    const presetSelect = el("frequency_preset");
    const meta = el("frequency_candidates_meta");
    if (!bandSelect || !presetSelect) return;

    const previousBand = bandSelect.value;
    const previousPreset = presetSelect.value;
    const limit = snapshot ? Math.min(500, integerValue(snapshot.frequency_candidate_limit) || 500) : 0;
    const seen = new Set();
    frequencyCandidates = [];
    const rows = snapshot && Array.isArray(snapshot.frequency_candidates)
      ? snapshot.frequency_candidates : [];
    rows.slice(0, limit).forEach((row) => {
      if (!row || typeof row !== "object") return;
      const hz = canonicalHz(row.frequency_hz);
      if (!hz || seen.has(hz) || typeof row.band !== "string" || row.band.length === 0) return;
      seen.add(hz);
      frequencyCandidates.push({
        hz,
        band: row.band,
        mode: typeof row.mode === "string" ? row.mode : "",
        region: typeof row.region === "string" ? row.region : "",
        defaultFrequency: row.default === true
      });
    });
    frequencyCandidates.sort(frequencyCandidateLess);

    const bands = [];
    const seenBands = new Set();
    frequencyCandidates.forEach((row) => {
      if (!seenBands.has(row.band)) {
        seenBands.add(row.band);
        bands.push(row.band);
      }
    });
    bandSelect.replaceChildren();
    addOption(bandSelect, "", "全部频段");
    bands.forEach((band) => addOption(bandSelect, band, band));
    bandSelect.value = bands.includes(previousBand) ? previousBand : "";

    const filtered = frequencyCandidates.filter((row) => !bandSelect.value || row.band === bandSelect.value);
    presetSelect.replaceChildren();
    addOption(presetSelect, "", "选择常用频率");
    filtered.forEach((row) => {
      const mhz = frequencyHzToMhz(row.hz);
      if (mhz) addOption(presetSelect, row.hz, mhz + " MHz · " + row.band);
    });
    const validPreset = filtered.some((row) => row.hz === previousPreset);
    presetSelect.value = validPreset ? previousPreset : "";

    if (meta) {
      if (!snapshot) meta.textContent = "等待当前模式与地区的候选";
      else if (frequencyCandidates.length === 0) meta.textContent = "当前模式与地区暂无候选，仍可手动输入";
      else {
        const mode = typeof snapshot.frequency_candidate_mode === "string"
          && snapshot.frequency_candidate_mode.length > 0 ? snapshot.frequency_candidate_mode : "未知模式";
        const region = typeof snapshot.frequency_candidate_region === "string"
          && snapshot.frequency_candidate_region.length > 0 ? snapshot.frequency_candidate_region : "未知地区";
        meta.textContent = mode + " / " + region + " · " + frequencyCandidates.length + " 条候选";
      }
    }
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

  function dxStatus(message, kind) {
    const node = el("dx_selection_result");
    if (!node) return;
    node.textContent = message;
    node.className = "frequency-result " + (kind || "");
  }

  function businessStatus(message, kind) {
    const node = el("business_result");
    if (!node) return;
    node.textContent = message;
    node.className = "frequency-result " + (kind || "");
  }

  function radioStatus(message, kind) {
    const node = el("radio_result");
    if (!node) return;
    node.textContent = message;
    node.className = "frequency-result " + (kind || "");
  }

  function finishConfirmation(accepted) {
    const resolver = confirmationResolver;
    if (!resolver) return;
    confirmationResolver = null;
    const previousFocus = confirmationPreviousFocus;
    confirmationPreviousFocus = null;
    const dialog = el("confirm_dialog");
    if (dialog) dialog.hidden = true;
    if (previousFocus && typeof previousFocus.focus === "function") previousFocus.focus();
    resolver(accepted);
  }

  function requestConfirmation(title, message) {
    if (confirmationResolver) return Promise.resolve(false);
    const dialog = el("confirm_dialog");
    const titleNode = el("confirm_title");
    const messageNode = el("confirm_message");
    const cancel = el("confirm_cancel");
    if (!dialog || !titleNode || !messageNode || !cancel) return Promise.resolve(false);
    titleNode.textContent = title;
    messageNode.textContent = message;
    confirmationPreviousFocus = document.activeElement;
    dialog.hidden = false;
    cancel.focus();
    return new Promise((resolve) => { confirmationResolver = resolve; });
  }

  function handleConfirmationKeydown(event) {
    if (!confirmationResolver) return;
    if (event.key === "Escape") {
      event.preventDefault();
      finishConfirmation(false);
      return;
    }
    if (event.key !== "Tab") return;
    const cancel = el("confirm_cancel");
    const accept = el("confirm_accept");
    if (!cancel || !accept) return;
    if (event.shiftKey && document.activeElement === cancel) {
      event.preventDefault();
      accept.focus();
    } else if (!event.shiftKey && document.activeElement === accept) {
      event.preventDefault();
      cancel.focus();
    }
  }

  function operationForRequest(snapshot) {
    if (!frequencyRequest || !snapshot || !Array.isArray(snapshot.operations)) return null;
    return snapshot.operations.find((row) => row && typeof row === "object"
      && row.operation === "frequency"
      && row.request_id === frequencyRequest.requestId
      && row.server_epoch === frequencyRequest.epoch) || null;
  }

  function operationForDx(snapshot) {
    if (!dxRequest || !snapshot || !Array.isArray(snapshot.operations)) return null;
    return snapshot.operations.find((row) => row && typeof row === "object"
      && row.operation === "select-dx"
      && row.request_id === dxRequest.requestId
      && row.server_epoch === dxRequest.epoch) || null;
  }

  function operationForBusiness(snapshot) {
    if (!businessRequest || !snapshot || !Array.isArray(snapshot.operations)) return null;
    return snapshot.operations.find((row) => row && typeof row === "object"
      && row.operation === businessRequest.operation
      && row.request_id === businessRequest.requestId
      && row.server_epoch === businessRequest.epoch) || null;
  }

  function operationForRadio(snapshot) {
    if (!radioRequest || !snapshot || !Array.isArray(snapshot.operations)) return null;
    return snapshot.operations.find((row) => row && typeof row === "object"
      && row.operation === "radio" && row.request_id === radioRequest.requestId
      && row.server_epoch === radioRequest.epoch) || null;
  }

  function readbackMatches(row, targetHz) {
    const readback = row && row.readback && typeof row.readback === "object" ? row.readback : null;
    return !!(readback && readback.confirmed === true
      && canonicalHz(readback.frequency_hz) === targetHz);
  }

  function readbackMatchesDx(row, request) {
    const readback = row && row.readback && typeof row.readback === "object" ? row.readback : null;
    return !!(readback && readback.confirmed === true
      && readback.dx_call === request.call && (readback.dx_grid || "") === (request.grid || "")
      && readback.dx_selection_source === "decode"
      && integerValue(readback.dx_source_decode_id) === request.decodeId);
  }

  function readbackMatchesBusiness(row, request) {
    const readback = row && row.readback && typeof row.readback === "object" ? row.readback : null;
    if (!readback || readback.confirmed !== true) return false;
    if (request.operation === "start-cq") return readback.cq_state === "armed";
    if (request.operation === "start-auto-call") return readback.auto_sequence_enabled === true;
    return readback.auto_sequence_enabled === false
      && ["idle", "not_selected"].includes(readback.cq_state);
  }

  function readbackMatchesRadio(row, request) {
    const readback = row && row.readback && typeof row.readback === "object" ? row.readback : null;
    if (!readback || readback.confirmed !== true || readback.radio_state_known !== true) return false;
    const values = {
      "enable-tx": readback.tx_enabled === true,
      "stop-tx": readback.tx_enabled === false,
      "multi-decode": readback.radio_multi_decode === request.value,
      "agc-compensation": readback.radio_agc_compensation === request.value,
      "narrow": readback.radio_narrow === request.value,
      "sync": readback.radio_sync === request.value,
      "skip-tx1": readback.radio_skip_tx1 === request.value,
      "select-tx": integerValue(readback.radio_current_tx_index) === request.index,
      "set-tx-message": Array.isArray(readback.radio_tx_messages)
        && readback.radio_tx_messages[request.index - 1] === request.message,
      "log-qso": readback.radio_log_dialog_open === true,
      "log-qso-cancel": readback.radio_log_dialog_open === false,
      "log-qso-confirm": readback.radio_log_dialog_open === false
        && integerValue(readback.radio_qso_generation) > request.qsoGeneration,
      "cq": readback.cq_state === "armed"
    };
    return Object.prototype.hasOwnProperty.call(values, request.action) ? values[request.action] : true;
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
      frequencyStatus("服务状态已刷新，旧频率请求已失效；请以当前状态为准。", "warning");
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

  function reconcileDx(snapshot) {
    if (!dxRequest || !snapshot) return;
    const epoch = typeof snapshot.server_epoch === "string" ? snapshot.server_epoch : "";
    if (epoch && epoch !== dxRequest.epoch) {
      dxRequest = null;
      dxUnknown = false;
      dxStatus("服务状态已刷新，旧 DX 选择已失效；请以当前状态为准。", "warning");
      return;
    }
    const row = operationForDx(snapshot);
    if (row && terminalOperation(row)) {
      if (row.status === "completed" && readbackMatchesDx(row, dxRequest)) {
        dxStatus("DX 已完成，并已由匹配回读确认。", "success");
        dxUnknown = false;
        dxRequest = null;
      } else if (row.status === "completed") {
        dxUnknown = true;
        dxStatus("服务报告 DX 完成，但回读不匹配，结果未知。", "warning");
      } else {
        dxUnknown = false;
        dxStatus("DX 请求未完成：" + boundedString(row.reason || "服务未提供原因", 180), "error");
        dxRequest = null;
      }
    }
    updateDxControls();
  }

  function reconcileBusiness(snapshot) {
    if (!businessRequest || !snapshot) return;
    const epoch = typeof snapshot.server_epoch === "string" ? snapshot.server_epoch : "";
    if (epoch && epoch !== businessRequest.epoch) {
      const preserveUnknown = businessRequest.preserveUnknown === true;
      businessRequest = null;
      businessUnknown = preserveUnknown;
      businessStatus(preserveUnknown
        ? "服务状态已刷新，但服务端未确认锁仍保留；请先确认停止状态。"
        : "服务状态已刷新，旧 CQ/AutoSeq 请求已失效。", "warning");
      return;
    }
    const row = operationForBusiness(snapshot);
    if (row && terminalOperation(row)) {
      const preserveUnknown = businessRequest.preserveUnknown === true;
      if (row.status === "completed" && readbackMatchesBusiness(row, businessRequest)) {
        businessStatus(preserveUnknown
          ? "停止已由业务状态回读确认；服务端仍保留旧操作的未确认锁。"
          : businessRequest.operation === "start-auto-call"
            ? "AutoSeq 已启用，等待下一批实时解码驱动自动呼叫；未表示已开始具体呼叫。"
            : "CQ/AutoSeq 操作已由业务状态回读确认。", preserveUnknown ? "warning" : "success");
        businessRequest = null;
        businessUnknown = preserveUnknown;
      } else if (row.status === "completed") {
        businessUnknown = true;
        businessStatus("服务报告完成，但业务回读不匹配，结果未知。", "warning");
      } else {
        businessRequest = null;
        businessUnknown = preserveUnknown || row.status === "timeout";
        businessStatus(businessUnknown
          ? "CQ/AutoSeq 请求结果未知，服务端保留未确认锁："
            + boundedString(row.reason || "feedback_timeout", 180)
          : "CQ/AutoSeq 请求未完成：" + boundedString(row.reason || "服务未提供原因", 180),
          businessUnknown ? "warning" : "error");
      }
    }
    updateBusinessControls();
  }

  function reconcileRadio(snapshot) {
    if (!radioRequest || !snapshot) return;
    const epoch = typeof snapshot.server_epoch === "string" ? snapshot.server_epoch : "";
    if (epoch && epoch !== radioRequest.epoch) {
      radioRequest = null;
      radioUnknown = true;
      radioStatus("服务状态已刷新，旧电台操作结果未知。", "warning");
      return;
    }
    const row = operationForRadio(snapshot);
    if (row && terminalOperation(row)) {
      if (row.status === "completed" && readbackMatchesRadio(row, radioRequest)) {
        radioUnknown = false;
        radioStatus("电台操作已由实际状态回读确认。", "success");
        radioRequest = null;
      } else if (row.status === "completed") {
        radioUnknown = true;
        radioStatus("服务报告完成，但电台回读不匹配，结果未知。", "warning");
      } else {
        radioUnknown = row.status === "timeout";
        radioStatus("电台操作未完成：" + boundedString(row.reason || "服务未提供原因", 180), radioUnknown ? "warning" : "error");
        radioRequest = null;
      }
    }
    updateRadioControls();
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
    if (typeof snapshot.server_epoch !== "string" || snapshot.server_epoch.length === 0) return "状态快照缺少必要信息";
    if (integerValue(snapshot.state_revision) == null || integerValue(snapshot.state_revision) < 0) return "状态快照版本无效";
    if (!lastUpdate || Date.now() - lastUpdate > 15000) return "状态快照已超时";
    if (frequencyUnknown) return "上一次请求结果未知，等待明确回读或新状态";
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
    if (label) {
      const enabled = [];
      if (currentSnapshot && currentSnapshot.frequency_control_enabled === true) enabled.push("频率");
      if (currentSnapshot && currentSnapshot.dx_control_enabled === true) enabled.push("DX");
      if (currentSnapshot && currentSnapshot.automation_control_enabled === true) enabled.push("CQ/AutoSeq");
      if (currentSnapshot && currentSnapshot.radio_control_enabled === true) enabled.push("电台面板");
      label.textContent = enabled.length ? enabled.join("、") + "控制已开放" : "按能力开放 · 当前仅只读";
    }
  }

  function dxGateReason(snapshot) {
    if (!snapshot) return "等待新鲜状态快照";
    if (snapshot.dx_control_enabled !== true) return "桌面尚未开放 DX 选择";
    if (!connected) return "等待连接和最新状态快照";
    if (snapshot.online !== true || snapshot.rig_online !== true) return "主程序或电台未在线";
    if (snapshot.rig_fresh !== true || snapshot.freshness !== "fresh") return "状态快照陈旧";
    if (snapshot.tx_enabled !== false || snapshot.transmitting !== false || snapshot.ptt !== false
        || snapshot.watchdog_timeout !== false) return "当前 TX/PTT 状态不是明确安全值";
    if (integerValue(snapshot.state_revision) == null || integerValue(snapshot.state_revision) < 0) return "状态快照版本无效";
    if (!lastUpdate || Date.now() - lastUpdate > 15000) return "状态快照已超时";
    if (dxUnknown) return "上一次 DX 选择结果未知，等待明确回读或新状态";
    if (dxRequest) return "已有 DX 选择处理中";
    return "";
  }

  function updateDxControls() {
    const reason = dxGateReason(currentSnapshot);
    const status = el("dx_selection_status");
    if (status) {
      status.textContent = reason || "可选择新鲜实时解码";
      status.className = "frequency-control-status " + (reason ? "blocked" : "ready");
    }
  }

  function businessGateReason(snapshot, operation) {
    if (!snapshot) return "等待新鲜状态快照";
    if (snapshot.automation_control_enabled !== true) return "桌面尚未开放 CQ/AutoSeq 控制";
    if (!connected) return "等待连接和最新状态快照";
    if (operation === "stop-auto-call") {
      if (integerValue(snapshot.state_revision) == null) return "状态快照版本无效";
      return "";
    }
    if (snapshot.online !== true || snapshot.rig_online !== true) return "主程序或电台未在线";
    if (snapshot.rig_fresh !== true || snapshot.freshness !== "fresh") return "状态快照陈旧";
    if (snapshot.tx_enabled !== false || snapshot.transmitting !== false || snapshot.ptt !== false
        || snapshot.watchdog_timeout !== false) return "当前 TX/PTT 状态不是明确安全值";
    if (integerValue(snapshot.state_revision) == null) return "状态快照版本无效";
    if (businessUnknown) return "上一次 CQ/AutoSeq 结果未知；若服务端仍保留未确认锁，请重启 JTDX";
    if (businessRequest) return "已有 CQ/AutoSeq 请求处理中";
    return "";
  }

  function updateBusinessControls() {
    const startReason = businessGateReason(currentSnapshot, "start-cq");
    const stopReason = businessGateReason(currentSnapshot, "stop-auto-call");
    ["business_start_cq", "business_start_auto"].forEach((id) => {
      const button = el(id);
      if (button) button.disabled = !!startReason;
    });
    const stop = el("business_stop");
    if (stop) stop.disabled = !!stopReason;
    const status = el("business_control_status");
    if (status) {
      status.textContent = startReason
        ? "启动受限：" + startReason + (stopReason ? "" : "；停止可用")
        : "每次操作都需要页面确认";
      status.className = "frequency-control-status " + (startReason ? "blocked" : "ready");
    }
  }

  function radioGateReason(snapshot, action) {
    if (!snapshot) return "等待新鲜状态快照";
    if (snapshot.radio_control_enabled !== true) return "桌面尚未开放 Web 电台操作面板";
    if (!connected || snapshot.online !== true) return "等待主程序在线";
    if (snapshot.freshness !== "fresh" || snapshot.rig_fresh !== true) return "状态快照陈旧";
    if (integerValue(snapshot.state_revision) == null) return "状态快照版本无效";
    if (radioUnknown) return "上一次电台操作结果未知；先确认桌面状态或重启 JTDX";
    if (radioRequest) return "已有电台操作处理中";
    if (["enable-tx", "cq"].includes(action)
        && (snapshot.rig_online !== true || snapshot.tx_enabled !== false
            || snapshot.transmitting !== false || snapshot.ptt !== false
            || snapshot.watchdog_timeout !== false)) return "TX/PTT 状态不是明确安全值";
    if (action === "log-qso" && snapshot.radio_controls
        && snapshot.radio_controls.can_log_qso !== true) return "当前没有可记录的 DX 通联";
    const draftOpen = !!(snapshot.radio_controls && snapshot.radio_controls.qso_draft_open === true);
    if (action === "log-qso-confirm" && !draftOpen) return "请先打开记录通联草稿";
    if (action === "log-qso-cancel" && !draftOpen) return "当前没有待确认的记录草稿";
    return "";
  }

  function updateRadioControls() {
    const snapshot = currentSnapshot;
    const state = snapshot && snapshot.radio_controls && typeof snapshot.radio_controls === "object"
      ? snapshot.radio_controls : {};
    const known = state.known === true;
    const actionIds = ["enable_tx", "stop_tx", "log_qso", "clear_windows", "sync", "multi_decode",
      "agc", "narrow", "decode", "clear_dx", "generate", "cq", "skip_tx1"];
    actionIds.forEach((id) => {
      const node = el("radio_" + id);
      if (!node) return;
      const action = id === "enable_tx" ? "enable-tx" : id === "stop_tx" ? "stop-tx"
        : id === "log_qso" ? "log-qso" : id === "clear_windows" ? "clear-windows"
        : id === "multi_decode" ? "multi-decode" : id === "agc" ? "agc-compensation"
        : id === "clear_dx" ? "clear-dx" : id === "generate" ? "generate-message"
        : id === "skip_tx1" ? "skip-tx1" : id;
      node.disabled = !!radioGateReason(snapshot, action) || !known;
      node.classList.toggle("active", (action === "multi-decode" && state.multi_decode === true)
        || (action === "agc-compensation" && state.agc_compensation === true)
        || (action === "narrow" && state.narrow === true)
        || (action === "sync" && state.sync === true)
        || (action === "skip-tx1" && state.skip_tx1 === true));
    });
    const qsoDraft = state.qso_draft && typeof state.qso_draft === "object" ? state.qso_draft : {};
    const qsoOpen = state.qso_draft_open === true;
    const qsoGeneration = integerValue(state.qso_generation);
    if (!qsoOpen) {
      qsoDraftEditGeneration = null;
      qsoDraftDirty = false;
    } else if (qsoDraftEditGeneration !== qsoGeneration) {
      qsoDraftEditGeneration = qsoGeneration;
      qsoDraftDirty = false;
    }
    const qsoBox = el("radio_qso_draft");
    if (qsoBox) qsoBox.hidden = !qsoOpen;
    ["call", "grid", "mode", "report_sent", "report_received", "name", "start", "end",
      "frequency_hz", "tx_power", "comments", "eqsl_comments"].forEach((key) => {
      const input = el("qso_" + key);
      if (input && !qsoDraftDirty && document.activeElement !== input)
        input.value = typeof qsoDraft[key] === "string" || typeof qsoDraft[key] === "number" ? qsoDraft[key] : "";
      if (input) input.disabled = !qsoOpen || !!radioGateReason(snapshot, "log-qso-confirm");
    });
    const qsoCancel = el("radio_qso_cancel");
    const qsoConfirm = el("radio_qso_confirm");
    if (qsoCancel) qsoCancel.disabled = !qsoOpen || !!radioGateReason(snapshot, "log-qso-cancel");
    if (qsoConfirm) qsoConfirm.disabled = !qsoOpen || !!radioGateReason(snapshot, "log-qso-confirm");
    const qsoStatus = el("radio_qso_status");
    if (qsoStatus) qsoStatus.textContent = qsoOpen ? "已打开草稿；修改字段后确认提交，取消不会写入 ADIF。" : "提交前不会写入 ADIF。";
    const status = el("radio_control_status");
    const reason = radioGateReason(snapshot, "clear-windows");
    if (status) { status.textContent = reason || "操作需等待主程序实际状态回读"; status.className = "frequency-control-status " + (reason ? "blocked" : "ready"); }
    text("radio_state_badge", known ? "已同步" : "未知");
    for (let index = 1; index <= 6; index++) {
      const input = el("radio_tx_" + index);
      if (input && Array.isArray(state.tx_messages)) {
        const value = typeof state.tx_messages[index - 1] === "string" ? state.tx_messages[index - 1] : "";
        if (input.dataset.webDirty === "1" && input.value === value) input.dataset.webDirty = "";
        if (document.activeElement !== input && input.dataset.webDirty !== "1") input.value = value;
      }
    }
    document.querySelectorAll("input[name='web_tx_index']").forEach((input) => {
      input.checked = integerValue(state.current_tx_index) === integerValue(input.value);
      input.disabled = !known || !!radioGateReason(snapshot, "select-tx");
    });
  }

  function clearRenderedSnapshot(message) {
    currentSnapshot = null;
    frequencyCandidates = [];
    lastUpdate = 0;
    lastId = "";
    operationEpoch = null;
    operationRows = [];
    ["web_server_state", "application_name", "mode_band", "instance_id", "online", "frequency",
      "freshness", "frequency_freshness", "last_status_update", "last_decode_update", "decode_age",
      "dx_call", "dx_grid", "report", "df", "tx_mode", "tx_enabled", "transmitting", "decoding",
      "tx_first", "watchdog_timeout", "cq_qso", "auto_sequence_state", "current_tx_text", "decode_count",
      "dx_selection_status", "dx_selection_result", "business_control_status", "business_result",
      "radio_control_status", "radio_result", "radio_state_badge"]
      .forEach((id) => text(id, null));
    const box = el("decodes");
    if (box) box.replaceChildren();
    renderOperations("unknown", message || "等待当前会话的状态快照");
    updateFrequencyChoices(null);
    updateFrequencyForm();
    updateDxControls();
    updateBusinessControls();
    updateRadioControls();
  }

  function operationLabel(value) {
    if (value === "frequency") return "频率";
    if (value === "select-dx") return "选择 DX";
    if (value === "start-cq") return "启动 CQ";
    if (value === "start-auto-call") return "启用 AutoSeq";
    if (value === "stop-auto-call") return "停止 CQ/AutoSeq";
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
      title.textContent = operationLabel(row.operation);
      const status = statusLabel(row);
      const badge = document.createElement("span");
      badge.className = "operation-status " + status.className;
      badge.textContent = status.label;
      heading.appendChild(title);
      heading.appendChild(badge);
      item.appendChild(heading);

      const fields = document.createElement("dl");
      appendField(fields, "原因", boundedString(row.reason, 160));
      appendField(fields, "确认频率", confirmedFrequency(row, row.status));
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
      renderOperations("unknown", "当前快照缺少操作状态");
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
      : "保留 " + operationRows.length + " 条，显示最近 "
        + Math.min(operationRows.length, DISPLAY_OPERATION_ROWS) + " 条";
    renderOperations(freshness, message);
    reconcileFrequency(snapshot);
    reconcileDx(snapshot);
    reconcileBusiness(snapshot);
    reconcileRadio(snapshot);
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
      row.className = "decode" + (decode.is_new === true ? " highlight" : "");
      const time = typeof decode.time === "string" ? decode.time.slice(0, 8) : "未知";
      [[time, ""], [decode.snr == null ? "未知" : decode.snr, ""],
        [decode.delta_time == null ? "未知" : Number(decode.delta_time).toFixed(2), ""],
        [decode.delta_frequency == null ? "未知" : decode.delta_frequency, ""],
        [decode.mode || "—", ""], [decode.country || "—", "country"],
        [decode.message || "未知", "message"]].forEach(([value, className]) => {
        const node = document.createElement(className === "message" ? "b" : "span");
        node.className = className;
        node.textContent = String(value);
        row.appendChild(node);
      });
      if (decode.is_new === true && decode.fresh === true && typeof decode.callsign === "string"
          && decode.callsign.length > 0) {
        const button = document.createElement("button");
        button.type = "button";
        button.textContent = "选择 DX";
        button.disabled = !!dxGateReason(snapshot);
        button.addEventListener("click", () => sendSelectDx(decode));
        row.appendChild(button);
      }
      box.appendChild(row);
    });
    updateFrequencyChoices(snapshot);
    updateOperations(snapshot);
    updateFrequencyForm();
    updateDxControls();
    updateBusinessControls();
    updateRadioControls();
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

  function settleFrequencyResponse(payload, request, httpOk) {
    if (!responseIdentityMatches(payload, request)) {
      frequencyUnknown = true;
      frequencyStatus("响应无法与本次请求安全匹配，结果未知；等待回读或新状态。", "warning");
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
      if (!httpOk) {
        frequencyUnknown = true;
        frequencyStatus("服务返回非成功状态，结果未知；等待回读或新状态。", "warning");
      } else if (readbackMatches(payload, request.targetHz)) {
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
    frequencyStatus("响应状态未知，等待实际回读或新状态。", "warning");
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
        headers: { "Content-Type": "application/json", "Accept": "application/json" },
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
        frequencyStatus("服务响应无法解析，结果未知；等待回读或新状态。", "warning");
        updateFrequencyForm();
      } else {
        settleFrequencyResponse(payload, request, response.ok);
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
        ? "请求超时，结果未知；等待明确回读或新状态。"
        : "传输异常，结果未知；等待明确回读或新状态。", "warning");
      updateFrequencyForm();
    } finally {
      clearTimeout(timer);
      if (frequencyAbort === local) frequencyAbort = null;
    }
  }

  function settleDxResponse(payload, request, httpOk) {
    if (!responseIdentityMatches(payload, request)) {
      dxUnknown = true;
      dxStatus("响应无法与本次 DX 请求安全匹配，结果未知。", "warning");
      updateDxControls();
      return;
    }
    const status = typeof payload.status === "string" ? payload.status : "";
    if (["accepted", "pending", "received"].includes(status)) {
      dxStatus("DX 请求已登记，等待桌面状态回读；HTTP 响应不代表完成。", "processing");
    } else if (status === "completed") {
      if (!httpOk || !readbackMatchesDx(payload, request)) {
        dxUnknown = true;
        dxStatus("服务报告 DX 完成，但缺少匹配回读，结果未知。", "warning");
      } else {
        dxUnknown = false;
        dxStatus("DX 已完成，并已由匹配回读确认。", "success");
        dxRequest = null;
      }
    } else if (["failed", "rejected", "timeout"].includes(status)) {
      dxUnknown = false;
      dxRequest = null;
      dxStatus("DX 请求未完成：" + responseReason(payload, "服务未提供原因"), "error");
    } else {
      dxUnknown = true;
      dxStatus("响应状态未知，等待回读或新状态。", "warning");
    }
    updateDxControls();
  }

  async function sendSelectDx(decode) {
    const gate = dxGateReason(currentSnapshot);
    const decodeId = integerValue(decode && decode.decode_id);
    if (gate || decodeId == null || !currentSnapshot) {
      updateDxControls();
      return;
    }
    let requestId;
    try { requestId = secureRequestId(); } catch (_) {
      dxStatus("浏览器没有可用的安全随机源，无法发送 DX 请求。", "error");
      return;
    }
    const request = {
      requestId,
      epoch: currentSnapshot.server_epoch,
      decodeId,
      call: String(decode.callsign || "").toUpperCase(),
      grid: String(decode.grid || "").toUpperCase(),
      session: connectionSession,
      state: "sending"
    };
    dxRequest = request;
    dxUnknown = false;
    dxStatus("正在发送 DX 选择请求…", "processing");
    updateDxControls();
    const local = new AbortController();
    dxAbort = local;
    const timer = setTimeout(() => local.abort(), 5000);
    try {
      const response = await fetch("/api/v1/control/select-dx", {
        method: "POST",
        headers: { "Content-Type": "application/json", "Accept": "application/json" },
        body: JSON.stringify({
          request_id: request.requestId,
          server_epoch: request.epoch,
          state_revision: integerValue(currentSnapshot.state_revision),
          decode_id: request.decodeId
        }),
        cache: "no-store",
        signal: local.signal
      });
      if (request.session !== connectionSession || dxRequest !== request) return;
      let payload = null;
      try { payload = await response.json(); } catch (_) { payload = null; }
      if (request.session !== connectionSession || dxRequest !== request) return;
      if (!payload || typeof payload !== "object") {
        dxUnknown = true;
        dxStatus("服务响应无法解析，结果未知。", "warning");
      } else settleDxResponse(payload, request, response.ok);
      if (!response.ok && responseIdentityMatches(payload, request)
          && !["failed", "rejected", "timeout"].includes(payload.status)) {
        dxUnknown = true;
        dxStatus("服务拒绝 DX 请求：" + responseReason(payload, "HTTP " + response.status), "error");
      }
      updateDxControls();
    } catch (error) {
      if (request.session !== connectionSession || dxRequest !== request) return;
      dxUnknown = true;
      dxStatus(error && error.name === "AbortError"
        ? "DX 请求超时，结果未知；等待明确回读或新状态。"
        : "DX 请求传输异常，结果未知；等待明确回读或新状态。", "warning");
      updateDxControls();
    } finally {
      clearTimeout(timer);
      if (dxAbort === local) dxAbort = null;
    }
  }

  async function sendBusiness(operation) {
    const gate = businessGateReason(currentSnapshot, operation);
    if (gate || !currentSnapshot) { updateBusinessControls(); return; }
    const labels = {"start-cq": "启动 CQ", "start-auto-call": "启用 AutoSeq", "stop-auto-call": "停止 CQ/AutoSeq"};
    const confirmation = operation === "start-auto-call"
      ? "确认启用 AutoSeq？它会等待现有实时解码驱动自动呼叫，不会凭空生成目标或立即证明已呼叫。"
      : "确认" + labels[operation] + "？页面只提交命令，完成必须等待主程序业务状态回读。";
    if (!await requestConfirmation(labels[operation], confirmation)) return;
    let requestId;
    try { requestId = secureRequestId(); } catch (_) {
      businessStatus("浏览器没有可用的安全随机源，无法发送命令。", "error");
      return;
    }
    const request = {requestId, operation, epoch: currentSnapshot.server_epoch, session: connectionSession,
      preserveUnknown: operation === "stop-auto-call" && businessUnknown};
    businessRequest = request;
    businessUnknown = request.preserveUnknown === true;
    businessStatus("正在发送" + labels[operation] + "…", "processing");
    updateBusinessControls();
    const local = new AbortController();
    businessAbort = local;
    const timer = setTimeout(() => local.abort(), 5000);
    try {
      const response = await fetch("/api/v1/control/" + operation, {
        method: "POST",
        headers: {"Content-Type": "application/json", "Accept": "application/json"},
        body: JSON.stringify({request_id: request.requestId, server_epoch: request.epoch,
          state_revision: integerValue(currentSnapshot.state_revision), confirm: true}),
        cache: "no-store", signal: local.signal
      });
      if (request.session !== connectionSession || businessRequest !== request) return;
      let payload = null;
      try { payload = await response.json(); } catch (_) { payload = null; }
      if (!responseIdentityMatches(payload, request)) {
        businessUnknown = true;
        businessStatus("响应无法与本次命令安全匹配，结果未知。", "warning");
      } else if (["received", "accepted", "pending"].includes(payload.status)) {
        businessStatus("命令已登记，等待业务状态回读；HTTP 响应不代表完成。", "processing");
      } else if (payload.status === "completed" && response.ok && readbackMatchesBusiness(payload, request)) {
        businessRequest = null;
        businessUnknown = request.preserveUnknown === true;
        businessStatus(request.preserveUnknown
          ? "停止已由业务状态回读确认；服务端仍保留旧操作的未确认锁。"
          : request.operation === "start-auto-call"
            ? "AutoSeq 已启用，等待实时解码驱动自动呼叫；未表示已开始具体呼叫。"
            : "操作已由业务状态回读确认。", request.preserveUnknown ? "warning" : "success");
      } else if (payload.status === "timeout") {
        businessRequest = null;
        businessUnknown = true;
        businessStatus("CQ/AutoSeq 请求结果未知，服务端保留未确认锁："
          + responseReason(payload, "feedback_timeout"), "warning");
      } else if (payload.status === "failed" || payload.status === "rejected") {
        const reason = responseReason(payload, "服务未提供原因");
        businessRequest = null;
        businessUnknown = request.preserveUnknown === true || reason === "unconfirmed_feedback";
        businessStatus(businessUnknown
          ? "服务端保留未确认锁：" + reason
          : "命令未完成：" + reason,
          businessUnknown ? "warning" : "error");
      } else {
        businessUnknown = true;
        businessStatus("完成响应缺少匹配业务回读，结果未知。", "warning");
      }
      updateBusinessControls();
    } catch (error) {
      if (request.session !== connectionSession || businessRequest !== request) return;
      businessUnknown = true;
      businessStatus(error && error.name === "AbortError" ? "命令超时，结果未知。" : "命令传输异常，结果未知。", "warning");
      updateBusinessControls();
    } finally {
      clearTimeout(timer);
      if (businessAbort === local) businessAbort = null;
    }
  }

  function qsoDraftFromDom() {
    const qso = {};
    ["call", "grid", "mode", "report_sent", "report_received", "name", "start", "end",
      "tx_power", "comments", "eqsl_comments"].forEach((key) => { qso[key] = (el("qso_" + key)?.value || "").trim(); });
    qso.frequency_hz = integerValue(el("qso_frequency_hz")?.value || "");
    if (!qso.call || !qso.mode || !qso.start || !qso.end || qso.frequency_hz == null) return null;
    return qso;
  }

  async function sendRadio(action, value, index, message, qso) {
    const gate = radioGateReason(currentSnapshot, action);
    if (gate || !currentSnapshot) { updateRadioControls(); return; }
    const labels = {"enable-tx": "启用发射", "stop-tx": "终止发射", "log-qso": "打开记录草稿",
      "log-qso-confirm": "提交记录通联", "log-qso-cancel": "取消记录草稿",
      "clear-windows": "清空窗口", "sync": "同步", "multi-decode": "多次解码",
      "agc-compensation": "AGC 补偿", "narrow": "窄频", "decode": "解码",
      "clear-dx": "清除 DX", "generate-message": "生成消息", "cq": "CQ",
      "skip-tx1": "跳过 Tx1", "select-tx": "选择 Tx", "set-tx-message": "编辑消息"};
    const dangerous = ["enable-tx", "stop-tx", "log-qso-confirm", "cq"].includes(action);
    if (action === "log-qso-confirm" && !qso) qso = qsoDraftFromDom();
    if (action === "log-qso-confirm" && !qso) { radioStatus("记录草稿缺少必填字段。", "error"); return; }
    if (dangerous && !await requestConfirmation(labels[action], action === "log-qso-confirm"
        ? "确认提交这条 QSO？提交后将按主程序记录模型写入 ADIF，重复提交会被拦截。"
        : "确认" + labels[action] + "？完成必须等待主程序实际状态回读。")) return;
    let requestId;
    try { requestId = secureRequestId(); } catch (_) { radioStatus("浏览器没有可用的安全随机源，无法发送操作。", "error"); return; }
    const request = {requestId, epoch: currentSnapshot.server_epoch, action, value: value === true,
      index: integerValue(index) || 0, message: typeof message === "string" ? message : "",
      qso: qso || {}, qsoGeneration: integerValue(currentSnapshot.radio_controls?.qso_generation) || 0,
      session: connectionSession};
    radioRequest = request;
    radioUnknown = false;
    radioStatus("正在发送" + labels[action] + "…", "processing");
    updateRadioControls();
    const local = new AbortController();
    radioAbort = local;
    const timer = setTimeout(() => local.abort(), 5000);
    try {
      const response = await fetch("/api/v1/control/radio", {
        method: "POST", headers: {"Content-Type": "application/json", "Accept": "application/json"},
        body: JSON.stringify({request_id: request.requestId, server_epoch: request.epoch,
          state_revision: integerValue(currentSnapshot.state_revision), action: request.action,
          value: request.value, tx_index: request.index, text: request.message, confirm: dangerous, qso: request.qso}),
        cache: "no-store", signal: local.signal
      });
      if (request.session !== connectionSession || radioRequest !== request) return;
      let payload = null; try { payload = await response.json(); } catch (_) {}
      if (!responseIdentityMatches(payload, request)) {
        radioUnknown = true; radioStatus("响应无法与本次电台操作安全匹配，结果未知。", "warning");
      } else if (["received", "accepted", "pending"].includes(payload.status)) {
        radioStatus("操作已登记，等待主程序状态回读。", "processing");
      } else if (payload.status === "completed" && response.ok && readbackMatchesRadio(payload, request)) {
        radioRequest = null; radioUnknown = false; radioStatus("电台操作已由实际状态回读确认。", "success");
      } else if (["failed", "rejected", "timeout"].includes(payload.status)) {
        radioUnknown = payload.status === "timeout";
        radioStatus("电台操作" + (radioUnknown ? "结果未知" : "未完成") + "："
          + responseReason(payload, "服务未提供原因"), radioUnknown ? "warning" : "error");
        radioRequest = null;
      } else {
        radioUnknown = true; radioStatus("完成响应缺少匹配回读，结果未知。", "warning");
      }
      updateRadioControls();
    } catch (error) {
      if (request.session !== connectionSession || radioRequest !== request) return;
      radioUnknown = true;
      radioStatus(error && error.name === "AbortError" ? "电台操作超时，结果未知。" : "电台操作传输异常，结果未知。", "warning");
      updateRadioControls();
    } finally {
      clearTimeout(timer); if (radioAbort === local) radioAbort = null;
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
        const headers = {};
        if (lastId) headers["Last-Event-ID"] = lastId;
        const response = await fetch("/api/v1/events", {
          headers, cache: "no-store", signal: local.signal
        });
        if (run !== runId) throw new Error("superseded");
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

  function startConnection() {
    runId++;
    connectionSession++;
    running = false;
    if (controller) controller.abort();
    if (frequencyAbort) frequencyAbort.abort();
    if (dxAbort) dxAbort.abort();
    if (businessAbort) businessAbort.abort();
    if (radioAbort) radioAbort.abort();
    if (frequencyRequest) {
      frequencyUnknown = true;
      frequencyStatus("会话已更换，旧频率请求结果未知；等待匹配回读或新状态。", "warning");
    } else {
      frequencyUnknown = false;
    }
    if (dxRequest) {
      dxUnknown = true;
      dxStatus("会话已更换，旧 DX 请求结果未知；等待匹配回读或新状态。", "warning");
    } else {
      dxUnknown = false;
    }
    if (businessRequest) {
      businessUnknown = true;
      businessStatus("会话已更换，旧 CQ/AutoSeq 请求结果未知。", "warning");
    } else {
      businessUnknown = false;
    }
    if (radioRequest) {
      radioUnknown = true;
      radioStatus("会话已更换，旧电台操作结果未知。", "warning");
    } else {
      radioUnknown = false;
    }
    clearRenderedSnapshot("等待当前会话的状态快照");
    setConnected(false);
    running = true;
    stream(runId);
  }

  el("frequency_send").addEventListener("click", sendFrequency);
  el("confirm_cancel").addEventListener("click", () => finishConfirmation(false));
  el("confirm_accept").addEventListener("click", () => finishConfirmation(true));
  el("confirm_dialog").addEventListener("keydown", handleConfirmationKeydown);
  el("business_start_cq").addEventListener("click", () => sendBusiness("start-cq"));
  el("business_start_auto").addEventListener("click", () => sendBusiness("start-auto-call"));
  el("business_stop").addEventListener("click", () => sendBusiness("stop-auto-call"));
  const radioButtons = {
    radio_enable_tx: ["enable-tx", true], radio_stop_tx: ["stop-tx", false],
    radio_log_qso: ["log-qso", false], radio_clear_windows: ["clear-windows", false],
    radio_sync: ["sync", true], radio_multi_decode: ["multi-decode", true],
    radio_agc: ["agc-compensation", true], radio_narrow: ["narrow", true],
    radio_decode: ["decode", false], radio_clear_dx: ["clear-dx", false],
    radio_generate: ["generate-message", false], radio_cq: ["cq", false],
    radio_skip_tx1: ["skip-tx1", true]
  };
  Object.entries(radioButtons).forEach(([id, config]) => {
    el(id).addEventListener("click", () => {
      const state = currentSnapshot && currentSnapshot.radio_controls ? currentSnapshot.radio_controls : {};
      const toggles = ["sync", "multi-decode", "agc-compensation", "narrow", "skip-tx1"];
      const value = toggles.includes(config[0]) ? !(state[config[0].replace("agc-compensation", "agc_compensation").replace("multi-decode", "multi_decode").replace("skip-tx1", "skip_tx1")] === true) : config[1];
      sendRadio(config[0], value, 0, "");
    });
  });
  document.querySelectorAll("input[name='web_tx_index']").forEach((input) => {
    input.addEventListener("change", () => sendRadio("select-tx", true, integerValue(input.value), ""));
  });
  for (let index = 1; index <= 6; index++) {
    const input = el("radio_tx_" + index);
    input.addEventListener("input", () => {
      input.dataset.webDirty = "1";
      clearTimeout(txEditTimers[index]);
      txEditTimers[index] = setTimeout(() => {
        txEditTimers[index] = null;
        sendRadio("set-tx-message", false, index, input.value);
      }, 200);
    });
  }
  el("radio_qso_confirm").addEventListener("click", () => sendRadio("log-qso-confirm", false, 0, "", qsoDraftFromDom()));
  el("radio_qso_cancel").addEventListener("click", () => sendRadio("log-qso-cancel", false, 0, "", null));
  ["call", "grid", "mode", "report_sent", "report_received", "name", "start", "end",
    "frequency_hz", "tx_power", "comments", "eqsl_comments"].forEach((key) => {
    el("qso_" + key).addEventListener("input", () => { qsoDraftDirty = true; });
  });
  el("radio_cq_text").addEventListener("change", () => {
    const value = el("radio_cq_text").value.trim();
    if (value) sendRadio("generate-message", false, 0, value);
  });
  el("frequency_band").addEventListener("change", () => {
    el("frequency_preset").value = "";
    updateFrequencyChoices(currentSnapshot);
  });
  el("frequency_preset").addEventListener("change", () => {
    const selected = canonicalHz(el("frequency_preset").value);
    if (selected) {
      const value = frequencyHzToMhz(selected);
      if (value) el("frequency_input").value = value;
    }
    updateFrequencyForm();
  });
  el("frequency_input").addEventListener("input", () => {
    el("frequency_preset").value = "";
    updateFrequencyForm();
  });

  setInterval(() => {
    if (lastUpdate && Date.now() - lastUpdate > 15000) setConnected(false);
    updateFrequencyForm();
  }, 1000);
  updateFrequencyChoices(null);
  updateFrequencyForm();
  updateBusinessControls();
  updateRadioControls();
  startConnection();
}());
