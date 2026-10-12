#!/usr/bin/env python3
"""Extract current MainWindow stop methods and decisive production branches."""

from pathlib import Path
import hashlib
import re
import sys


def brace_tokens(source):
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
                raise ValueError("unterminated block comment")
            index = end + 2
            continue
        raw = re.match(r'(?:u8|u|U|L)?R"([^ ()\\\t\r\n]{0,16})\(', source[index:])
        if raw:
            terminator = ")" + raw.group(1) + '"'
            end = source.find(terminator, index + raw.end())
            if end < 0:
                raise ValueError("unterminated raw string")
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


def matching_close(source, opening):
    depth = 0
    for kind, relative in brace_tokens(source[opening:]):
        if kind == "{":
            depth += 1
        else:
            depth -= 1
            if depth == 0:
                return opening + relative
    raise ValueError("matching closing brace not found")


def extract_method(source, name):
    pattern = re.compile(
        r"\bvoid\s+MainWindow::" + re.escape(name) + r"\s*\([^;{}]*\)"
    )
    matches = list(pattern.finditer(source))
    if len(matches) != 1:
        raise ValueError("expected one definition of {}, found {}".format(
            name, len(matches)))
    start = matches[0].start()
    opening = matches[0].end()
    while opening < len(source):
        if source[opening].isspace():
            opening += 1
        elif source.startswith("//", opening):
            newline = source.find("\n", opening + 2)
            opening = len(source) if newline < 0 else newline + 1
        elif source.startswith("/*", opening):
            end = source.find("*/", opening + 2)
            if end < 0:
                raise ValueError("unterminated comment after {}".format(name))
            opening = end + 2
        else:
            break
    if opening >= len(source) or source[opening] != "{":
        raise ValueError("body opening brace not found for {}".format(name))
    return source[start:matching_close(source, opening) + 1]


def extract_block(source, anchor):
    if source.count(anchor) != 1:
        raise ValueError("expected one occurrence of {!r}".format(anchor))
    start = source.index(anchor)
    opening = source.find("{", start + len(anchor))
    if opening < 0:
        raise ValueError("opening brace not found after {!r}".format(anchor))
    return source[start:matching_close(source, opening) + 1]


def extract_statement(source, anchor):
    if source.count(anchor) != 1:
        raise ValueError("expected one occurrence of {!r}".format(anchor))
    start = source.index(anchor)
    end = source.find(";", start + len(anchor))
    if end < 0:
        raise ValueError("statement terminator not found after {!r}".format(anchor))
    return source[start:end + 1]


def extract(source_path, output_path):
    source_path = Path(source_path).resolve()
    raw = source_path.read_bytes()
    source_hash = hashlib.sha256(raw).hexdigest()
    source = raw.decode("utf-8")

    methods = [
        "on_stopTxButton_clicked",
        "stop_tuning",
        "on_tuneButton_clicked",
        "haltTx",
        "haltTxTuneTimer",
        "stopTx",
        "stopTx2",
    ]
    extracted = [extract_method(source, name) for name in methods]
    gui_stop = extract_block(
        source,
        "if((!m_btxok && btxok0 && g_iptt==1) || m_haltTrans)",
    )
    slot_end_stop = extract_statement(source, "bool const slotEndStop =")
    slot_end_start = source.index(slot_end_stop)
    gui_stop_start = source.index(
        "if((!m_btxok && btxok0 && g_iptt==1) || m_haltTrans)")
    if slot_end_start >= gui_stop_start:
        raise ValueError("slotEndStop production declaration must precede its stop block")
    observation_start = "auto pttObservation = PttStartPolicy::Observation::stale;"
    observation_end = "bool const pttWentOffAfterConfirmedOn ="
    if source.count(observation_start) != 1 or source.count(observation_end) != 1:
        raise ValueError("fresh rig PTT observation anchors changed")
    obs_start = source.index(observation_start)
    obs_end_start = source.index(observation_end, obs_start)
    obs_end = source.find(";", obs_end_start)
    if obs_end < 0:
        raise ValueError("fresh rig PTT observation terminator missing")
    observation = source[obs_start:obs_end + 1]

    rig_anchor = "if ((s.ptt_readback_required () ? pttWentOffAfterConfirmedOn"
    if source.count(rig_anchor) != 1:
        raise ValueError("fresh rig OFF stop predicate anchor changed")
    rig_start = source.index(rig_anchor)
    halt_end = source.find('haltTx ("Halt Tx from rig detected ");', rig_start)
    if halt_end < 0:
        raise ValueError("fresh rig OFF halt call missing")
    rig_stop = source[rig_start:halt_end + len(
        'haltTx ("Halt Tx from rig detected ");')]

    pieces = [
        "// Generated from current production mainwindow.cpp",
        "// Source raw SHA256={}".format(source_hash),
        '#define P0_TX_STOP_SOURCE_SHA256 "{}"'.format(source_hash),
        *extracted,
        "void MainWindow::production_gui_stop_edge (bool btxok0)",
        "{",
        slot_end_stop,
        gui_stop,
        "}",
        "bool MainWindow::production_fresh_rig_off (",
        "  RigObservation const& s, bool pttReadbackAdvanced)",
        "{",
        "  auto const rigGeneration = m_config.transceiver_session_generation ();",
        "  bool const pttReadbackKnown = s.ptt_readback_known ();",
        "  bool const tokenIsCurrent = m_pttStartPolicy.matches (",
        "    m_pttRequestToken, rigGeneration);",
        "  bool const oldStateIsCurrentGeneration = true;",
        "  auto const old_state = m_rigState;",
        "  bool const pttJustTurnedOn = false;",
        "  int const haltCountBefore = m_haltTxRequestedCount;",
        observation,
        rig_stop,
        "  return m_haltTxRequestedCount > haltCountBefore;",
        "}",
        "",
    ]
    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    temporary = output_path.with_suffix(output_path.suffix + ".tmp")
    temporary.write_text("\n".join(pieces), encoding="utf-8", newline="\n")
    temporary.replace(output_path)
    print("source_sha256={}".format(source_hash))
    print("generated_sha256={}".format(
        hashlib.sha256(output_path.read_bytes()).hexdigest()))
    print("output={}".format(output_path))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        raise SystemExit("usage: extractor mainwindow.cpp output.inc")
    try:
        extract(*sys.argv[1:])
    except Exception as error:
        print("extraction failed: {}".format(error), file=sys.stderr)
        raise SystemExit(2)
