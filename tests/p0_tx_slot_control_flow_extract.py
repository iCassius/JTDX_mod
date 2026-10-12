#!/usr/bin/env python3
"""Extract and compile the TX slot/control-flow statements from mainwindow.cpp."""

from pathlib import Path
import hashlib
import re
import sys


def brace_tokens(source):
    """Yield C++ brace tokens while ignoring comments and quoted strings."""
    index = 0
    length = len(source)
    while index < length:
        if source.startswith("//", index):
            end = source.find("\n", index + 2)
            index = length if end < 0 else end + 1
            continue
        if source.startswith("/*", index):
            end = source.find("*/", index + 2)
            if end < 0:
                raise ValueError("unterminated C++ block comment")
            index = end + 2
            continue
        raw = re.match(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(', source[index:])
        if raw:
            terminator = ")" + raw.group(1) + '"'
            end = source.find(terminator, index + raw.end())
            if end < 0:
                raise ValueError("unterminated C++ raw string")
            index = end + len(terminator)
            continue
        if source[index] in ('"', "'"):
            quote = source[index]
            index += 1
            while index < length:
                if source[index] == "\\":
                    index += 2
                elif source[index] == quote:
                    index += 1
                    break
                else:
                    index += 1
            continue
        if source[index] in "{}":
            yield source[index], index
        index += 1


def extract(source_path, baseline_fixture, output_dir):
    source_path = Path(source_path).resolve()
    raw = source_path.read_bytes()
    source_hash = hashlib.sha256(raw).hexdigest()
    source = raw.decode("utf-8")
    gui_start = source.find("void MainWindow::guiUpdate()")
    if gui_start < 0:
        raise ValueError("guiUpdate() entry not found")
    gui_end_marker = source.find("}               //End of GUIupdate", gui_start)
    if gui_end_marker < 0:
        raise ValueError("guiUpdate() end marker not found")
    gui_end = gui_end_marker + 1
    function = source[gui_start:gui_end]

    stack = []
    pairs = {}
    opens = []
    for kind, relative in brace_tokens(function):
        position = gui_start + relative
        if kind == "{":
            stack.append(position)
            opens.append(position)
        elif not stack:
            raise ValueError("unmatched closing brace inside guiUpdate()")
        else:
            pairs[stack.pop()] = position
    if stack:
        raise ValueError("unmatched opening brace inside guiUpdate()")

    def find_unique(needle, start=gui_start):
        positions = []
        next_position = start
        while True:
            found = source.find(needle, next_position, gui_end)
            if found < 0:
                break
            positions.append(found)
            next_position = found + 1
        if len(positions) != 1:
            raise ValueError("expected one occurrence of {!r}, found {}".format(
                needle, len(positions)))
        return positions[0]

    def find_unique_between(needle, start, end):
        positions = []
        next_position = start
        while True:
            found = source.find(needle, next_position, end)
            if found < 0:
                break
            positions.append(found)
            next_position = found + 1
        if len(positions) != 1:
            raise ValueError("expected one occurrence of {!r} in production slot range, found {}".format(
                needle, len(positions)))
        return positions[0]

    def opening_after(position):
        for opening in opens:
            if opening >= position:
                return opening
        raise ValueError("opening brace not found after source anchor")

    def closing(opening):
        if opening not in pairs:
            raise ValueError("matching closing brace not found")
        return pairs[opening]

    tx_period = find_unique("if(m_TRperiod==0.0) m_TRperiod=60.0;")
    tx_one = find_unique("double tx1=0.0;", tx_period)
    milliseconds = find_unique(
        "qint64 ms = m_jtdxtime->currentMSecsSinceEpoch2() % 86400000;",
        tx_one)
    nseq_line = "m_nseq = fmod(double(nsec),m_TRperiod);"
    nseq_end = find_unique(nseq_line, milliseconds) + len(nseq_line)
    tune_line = "if(m_tune) m_bTxTime=true;"
    tune_start = find_unique(tune_line, nseq_end)
    wspr = find_unique_between(
        'if(m_mode.left(4)=="WSPR") {', nseq_end, tune_start)
    wspr_open = opening_after(wspr)
    wspr_close = closing(wspr_open)
    else_match = re.match(r"\s*else\s*\{", source[wspr_close + 1:gui_end])
    if not else_match:
        raise ValueError("WSPR branch has no following production else block")
    else_start = wspr_close + 1 + else_match.start()
    else_open = opening_after(else_start)
    else_close = closing(else_open)
    if not else_close < tune_start:
        raise ValueError("Tune precedence line is not after the slot branch")
    slot_fraction = (
        "float fTR=float((ms%int(1000.0*m_TRperiod)))/(1000.0*m_TRperiod);"
    )
    fraction_start = find_unique(slot_fraction, tune_start + len(tune_line))
    slot = "\n".join([
        "// Extracted from current production mainwindow.cpp",
        "// Source raw SHA256={}".format(source_hash),
        source[tx_period:tx_one],
        source[tx_one:milliseconds],
        source[milliseconds:nseq_end],
        source[wspr:wspr_open + 1],
        source[wspr_close:wspr_close + 1],
        source[wspr_close + 1:else_open + 1],
        source[else_open + 1:else_close],
        source[else_close:else_close + 1],
        source[tune_start:tune_start + len(tune_line)],
        source[fraction_start:fraction_start + len(slot_fraction)],
        "",
    ])

    outer_anchor = find_unique("if(m_transmitting or m_enableTx or m_tune)")
    outer_open = opening_after(outer_anchor)
    outer_close = closing(outer_open)
    request_anchor = find_unique(
        "if(g_iptt==0 && PttStartPolicy::may_start_request", outer_open)
    request_open = opening_after(request_anchor)
    request_close = closing(request_open)
    clear_line = "if(!m_bTxTime and !m_tune) m_btxok=false;"
    clear_start = find_unique(clear_line, request_open)
    clear_end = clear_start + len(clear_line)
    stop_anchor = find_unique(
        "if((!m_btxok && btxok0 && g_iptt==1) || m_haltTrans)", clear_end)
    slot_end_start = find_unique_between(
        "bool const slotEndStop =", clear_end, stop_anchor)
    slot_end_semicolon = source.find(";", slot_end_start, stop_anchor)
    if slot_end_semicolon < 0:
        raise ValueError("slotEndStop production declaration has no terminator")
    slot_end_end = slot_end_semicolon + 1
    stop_open = opening_after(stop_anchor)
    stop_close = closing(stop_open)
    if not (outer_open < request_open < request_close < outer_close
            < slot_end_start < slot_end_end < stop_open):
        raise ValueError("TX request/clear/stop scope ordering changed unexpectedly")

    clear_inside_request = request_open < clear_start < request_close
    scope_parts = [
        "// Extracted from current production mainwindow.cpp",
        "// Source raw SHA256={}".format(source_hash),
        source[outer_anchor:outer_open + 1],
        source[request_anchor:request_open + 1],
    ]
    if clear_inside_request:
        scope_parts.append(source[clear_start:clear_end])
    scope_parts.append(source[request_close:request_close + 1])
    if not clear_inside_request:
        scope_parts.append(source[clear_start:clear_end])
    scope_parts.extend([
        source[outer_close:outer_close + 1],
        source[slot_end_start:slot_end_end],
        source[stop_anchor:stop_close + 1],
        "",
    ])
    scope = "\n".join(scope_parts)

    baseline_fixture = Path(baseline_fixture)
    fixture_text = baseline_fixture.read_text(encoding="utf-8")
    if "S0 source raw SHA256=cc5434d55d37f55b2843806594d09f908cc0cd6c3170027f596a9ce204f259de" not in fixture_text:
        raise ValueError("038 fixture lacks the expected S0 source provenance")
    output_dir = Path(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    outputs = {
        output_dir / "production_slot_current.inc": slot,
        output_dir / "production_scope_current.inc": scope,
    }
    for path, content in outputs.items():
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(content, encoding="utf-8", newline="\n")
        temporary.replace(path)
        print("{} sha256={}".format(
            path, hashlib.sha256(path.read_bytes()).hexdigest()))
    print("source_sha256={} clear_inside_request_gate={}".format(
        source_hash, clear_inside_request))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit(
            "usage: extractor mainwindow.cpp baseline-fixture output-dir")
    try:
        extract(*sys.argv[1:])
    except Exception as error:
        print("extraction failed: {}".format(error), file=sys.stderr)
        raise SystemExit(2)
