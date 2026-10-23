#!/usr/bin/env python3
"""Render the subset of mermaid used by the Owl documentation into plain SVG.

The in-editor help panel rasterises SVG with lunasvg, which has neither a browser nor `foreignObject`
support: the output is plain shapes and `<text>` only (the editor registers a font face for the text).
Supported diagrams: `flowchart` / `graph` (nodes, subgraphs, edge labels, `classDef`), `stateDiagram-v2`
(composite states are drawn as a group), `sequenceDiagram` (participants, messages, notes, loop / alt /
opt / par blocks) and `classDiagram` (members, relations). Anything else raises `MermaidError`, so a
diagram the renderer cannot draw fails the help bundle instead of silently disappearing.

Usage:
    mermaid_svg.py render <input.mmd> <output.svg>
    mermaid_svg.py bundle <help_dir>      # rewrites every ```mermaid fence of <help_dir>/*.md in place
"""

from __future__ import annotations

import argparse
import hashlib
import html
import re
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

FONT_FAMILY = "Inter, Open Sans, Roboto, sans-serif"
FONT_SIZE = 13.0
LINE_HEIGHT = 16.0
MAX_LABEL_PX = 190.0
MAX_NODE_PX = 230.0

# Palette tuned for the dark editor theme; the diagram carries its own background so it also reads on a light one.
BG = "#1b1f24"
NODE_FILL = "#2d333b"
NODE_STROKE = "#7d8590"
TEXT = "#e6edf3"
MUTED = "#9da7b3"
EDGE = "#9da7b3"
GROUP_FILL = "#22272e"
GROUP_STROKE = "#545d68"


class MermaidError(ValueError):
    """Raised when a diagram uses syntax outside the supported subset."""


# --------------------------------------------------------------------------------------------------------
# Text helpers
# --------------------------------------------------------------------------------------------------------


def char_width(ch: str, size: float = FONT_SIZE) -> float:
    """Approximate advance of one character of a proportional sans font."""
    if ch in "il.,'|:;!`":
        return size * 0.30
    if ch in "ftjr()[]{} /\\-":
        return size * 0.40
    if ch in "mwMW@":
        return size * 0.88
    if ch.isupper() or ch.isdigit():
        return size * 0.64
    return size * 0.57


def text_width(text: str, size: float = FONT_SIZE) -> float:
    """Approximate rendered width of one line."""
    return sum(char_width(c, size) for c in text)


def clean_label(raw: str) -> list[str]:
    """Turn a mermaid label into display lines (line breaks, entities and simple tags handled)."""
    text = raw.strip()
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        text = text[1:-1]
    text = re.sub(r"<br\s*/?>", "\n", text, flags=re.IGNORECASE)
    text = re.sub(r"</?[a-zA-Z][^>]*>", "", text)
    text = html.unescape(text)
    return [line.strip() for line in text.split("\n")]


def wrap_lines(lines: list[str], max_px: float) -> list[str]:
    """Wrap every line at word boundaries so none exceeds `max_px` (long words stay whole)."""
    out: list[str] = []
    for line in lines:
        if text_width(line) <= max_px:
            out.append(line)
            continue
        current = ""
        for word in line.split(" "):
            candidate = word if not current else current + " " + word
            if current and text_width(candidate) > max_px:
                out.append(current)
                current = word
            else:
                current = candidate
        out.append(current)
    return out


def esc(text: str) -> str:
    """XML-escape text content."""
    return html.escape(text, quote=False)


def svg_text(
    x: float,
    cy: float,
    lines: list[str],
    *,
    fill: str = TEXT,
    anchor: str = "middle",
    weight: str = "",
    size: float = FONT_SIZE,
) -> str:
    """Emit a block of text lines vertically centred on `cy`."""
    parts: list[str] = []
    count = len(lines)
    extra = f' font-weight="{weight}"' if weight else ""
    for index, line in enumerate(lines):
        if not line:
            continue
        y = cy + (index - (count - 1) / 2.0) * LINE_HEIGHT + size * 0.35
        parts.append(
            f'<text x="{x:.1f}" y="{y:.1f}" text-anchor="{anchor}" font-family="{FONT_FAMILY}" '
            f'font-size="{size:g}" fill="{fill}"{extra}>{esc(line)}</text>'
        )
    return "".join(parts)


class Bounds:
    """Running bounding box of everything drawn."""

    def __init__(self) -> None:
        self.min_x = float("inf")
        self.min_y = float("inf")
        self.max_x = float("-inf")
        self.max_y = float("-inf")

    def add(self, x0: float, y0: float, x1: float, y1: float) -> None:
        """Grow the box to include the rectangle."""
        self.min_x = min(self.min_x, x0, x1)
        self.min_y = min(self.min_y, y0, y1)
        self.max_x = max(self.max_x, x0, x1)
        self.max_y = max(self.max_y, y0, y1)


def finish_svg(body: list[str], bounds: Bounds, margin: float = 14.0) -> str:
    """Wrap drawn elements into a complete SVG document sized on `bounds`."""
    if bounds.min_x == float("inf"):
        bounds.add(0, 0, 40, 20)
    width = bounds.max_x - bounds.min_x + 2 * margin
    height = bounds.max_y - bounds.min_y + 2 * margin
    tx = margin - bounds.min_x
    ty = margin - bounds.min_y
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width:.0f}" height="{height:.0f}" '
        f'viewBox="0 0 {width:.0f} {height:.0f}">'
        f'<rect width="{width:.0f}" height="{height:.0f}" rx="8" fill="{BG}"/>'
        f'<g transform="translate({tx:.1f} {ty:.1f})">{"".join(body)}</g></svg>\n'
    )


# --------------------------------------------------------------------------------------------------------
# Graph model shared by flowchart, state and class diagrams
# --------------------------------------------------------------------------------------------------------


@dataclass
class Node:
    """One node of a graph-like diagram; `x` / `y` are the centre once laid out."""

    id: str
    lines: list[str]
    shape: str = "rect"  # rect, round, diamond, start, end, class
    members: list[str] = field(default_factory=list)
    style: dict[str, str] = field(default_factory=dict)
    group: str = ""
    w: float = 0.0
    h: float = 0.0
    x: float = 0.0
    y: float = 0.0


@dataclass
class Edge:
    """One directed connection."""

    src: str
    dst: str
    label: list[str] = field(default_factory=list)
    dashed: bool = False
    thick: bool = False
    head: str = "arrow"  # arrow, none, hollow


@dataclass
class Group:
    """A subgraph or composite state."""

    id: str
    title: list[str]
    parent: str = ""


@dataclass
class Graph:
    """Parsed flowchart, state or class diagram."""

    direction: str = "LR"
    nodes: dict[str, Node] = field(default_factory=dict)
    edges: list[Edge] = field(default_factory=list)
    groups: dict[str, Group] = field(default_factory=dict)
    classes: dict[str, dict[str, str]] = field(default_factory=dict)

    def ensure(self, node_id: str, group: str) -> Node:
        """Get a node, creating it (labelled by its id) when unknown."""
        node = self.nodes.get(node_id)
        if node is None:
            node = Node(node_id, [node_id], group=group)
            self.nodes[node_id] = node
        return node


# --------------------------------------------------------------------------------------------------------
# Flowchart parser
# --------------------------------------------------------------------------------------------------------

_ID_RE = re.compile(r"[A-Za-z_]\w*")
_EDGE_TEXT_RE = re.compile(r"(--|==)\s+([^|>]+?)\s+(-->|---|==>)")
_EDGE_RE = re.compile(r"(-\.+->|-\.+-|=+>|-+>|-{3,}|={3,})")
_CLOSERS = {"[": "]", "(": ")", "{": "}"}


def _read_shape(text: str, pos: int) -> tuple[str, list[str], int]:
    """Read an optional `[..]`, `(..)` or `{..}` label at `pos`; returns shape, label lines, new position."""
    if pos >= len(text) or text[pos] not in _CLOSERS:
        return "", [], pos
    opener = text[pos]
    closer = _CLOSERS[opener]
    start = pos + 1
    if text.startswith('"', start):
        end_quote = text.find('"', start + 1)
        if end_quote < 0:
            raise MermaidError(f"unterminated quoted label in: {text!r}")
        label = text[start : end_quote + 1]
        end = end_quote + 1
        if end >= len(text) or text[end] != closer:
            raise MermaidError(f"label not closed by {closer!r} in: {text!r}")
    else:
        end = text.find(closer, start)
        if end < 0:
            raise MermaidError(f"label not closed by {closer!r} in: {text!r}")
        label = text[start:end]
    shape = {"[": "rect", "(": "round", "{": "diamond"}[opener]
    return shape, clean_label(label), end + 1


def _skip_ws(text: str, pos: int) -> int:
    while pos < len(text) and text[pos] in " \t":
        pos += 1
    return pos


class FlowParser:
    """Parses `flowchart` / `graph` bodies into a `Graph`."""

    def __init__(self, graph: Graph) -> None:
        self.g = graph
        self.stack: list[str] = []

    @property
    def scope(self) -> str:
        """Innermost open subgraph id (empty at top level)."""
        return self.stack[-1] if self.stack else ""

    def parse_node(self, text: str, pos: int) -> tuple[str, int]:
        """Parse `id`, `id[label]`, `id{label}`... at `pos`."""
        match = _ID_RE.match(text, pos)
        if not match:
            raise MermaidError(f"node expected at: {text[pos:]!r}")
        node_id = match.group(0)
        pos = match.end()
        node = self.g.ensure(node_id, self.scope)
        shape, label, pos = _read_shape(text, pos)
        if shape:
            node.shape = shape
            node.lines = label
        if text.startswith(":::", pos):
            class_match = _ID_RE.match(text, pos + 3)
            if class_match:
                node.style.update(self.g.classes.get(class_match.group(0), {}))
                pos = class_match.end()
        return node_id, pos

    def parse_group(self, text: str, pos: int) -> tuple[list[str], int]:
        """Parse `A & B & C`."""
        ids: list[str] = []
        node_id, pos = self.parse_node(text, pos)
        ids.append(node_id)
        while True:
            probe = _skip_ws(text, pos)
            if probe < len(text) and text[probe] == "&":
                node_id, pos = self.parse_node(text, _skip_ws(text, probe + 1))
                ids.append(node_id)
            else:
                return ids, pos

    def parse_edge(self, text: str, pos: int) -> tuple[Edge, int] | None:
        """Parse an edge operator with its optional label; None when none starts at `pos`."""
        pos = _skip_ws(text, pos)
        label: list[str] = []
        text_match = _EDGE_TEXT_RE.match(text, pos)
        if text_match:
            op = text_match.group(3)
            label = clean_label(text_match.group(2))
            pos = text_match.end()
        else:
            op_match = _EDGE_RE.match(text, pos)
            if not op_match:
                return None
            op = op_match.group(0)
            pos = op_match.end()
        pos = _skip_ws(text, pos)
        if pos < len(text) and text[pos] == "|":
            end = text.find("|", pos + 1)
            if end < 0:
                raise MermaidError(f"edge label not closed in: {text!r}")
            label = clean_label(text[pos + 1 : end])
            pos = end + 1
        head = "arrow" if op.endswith(">") else "none"
        return Edge("", "", label, dashed="." in op, thick=op.startswith("="), head=head), pos

    def parse_statement(self, text: str) -> None:
        """Parse a node or an edge chain."""
        ids, pos = self.parse_group(text, 0)
        while True:
            parsed = self.parse_edge(text, pos)
            if parsed is None:
                break
            edge, pos = parsed
            pos = _skip_ws(text, pos)
            nxt, pos = self.parse_group(text, pos)
            for a in ids:
                for b in nxt:
                    self.g.edges.append(Edge(a, b, edge.label, edge.dashed, edge.thick, edge.head))
            ids = nxt
        if _skip_ws(text, pos) < len(text):
            raise MermaidError(f"unexpected text {text[pos:]!r} in statement {text!r}")

    def parse_line(self, line: str) -> None:
        """Parse one non-empty line."""
        if line.startswith("subgraph"):
            rest = line[len("subgraph") :].strip()
            match = re.match(r"([A-Za-z_]\w*)\s*\[(.*)\]$", rest)
            if match:
                gid, title = match.group(1), clean_label(match.group(2))
            else:
                gid, title = re.sub(r"\W+", "_", rest), clean_label(rest)
            self.g.groups[gid] = Group(gid, title, self.scope)
            self.stack.append(gid)
        elif line == "end":
            if not self.stack:
                raise MermaidError("`end` without matching `subgraph`")
            self.stack.pop()
        elif line.startswith("classDef "):
            parts = line.split(None, 2)
            style: dict[str, str] = {}
            for item in (parts[2] if len(parts) > 2 else "").split(","):
                if ":" in item:
                    key, value = item.split(":", 1)
                    style[key.strip()] = value.strip()
            self.g.classes[parts[1]] = style
        elif re.match(r"class\s+\S+\s+\w+$", line):
            _, ids, name = line.split()
            for node_id in ids.split(","):
                self.g.ensure(node_id, self.scope).style.update(self.g.classes.get(name, {}))
        elif line.startswith(("direction ", "style ", "linkStyle ", "click ")):
            return
        else:
            self.parse_statement(line)


def split_statements(body: str) -> list[str]:
    """Strip comments and blank lines."""
    out = []
    for raw in body.split("\n"):
        line = raw.strip()
        if not line or line.startswith("%%"):
            continue
        out.append(line)
    return out


def parse_flowchart(lines: list[str]) -> Graph:
    """Parse a `flowchart` / `graph` diagram."""
    head = lines[0].split()
    direction = head[1] if len(head) > 1 else "TD"
    direction = "TD" if direction == "TB" else direction
    if direction not in ("TD", "LR", "RL", "BT"):
        raise MermaidError(f"unsupported flowchart direction {direction!r}")
    graph = Graph(direction=direction)
    parser = FlowParser(graph)
    for line in lines[1:]:
        parser.parse_line(line)
    if parser.stack:
        raise MermaidError("`subgraph` without `end`")
    return graph


# --------------------------------------------------------------------------------------------------------
# State diagram parser
# --------------------------------------------------------------------------------------------------------


def parse_state(lines: list[str]) -> Graph:
    """Parse a `stateDiagram-v2`: transitions, `[*]` pseudo states and composite `state X { }` blocks."""
    graph = Graph(direction="TD")
    stack: list[str] = []

    def scope() -> str:
        return stack[-1] if stack else ""

    def endpoint(name: str, is_target: bool) -> str:
        if name == "[*]":
            kind = "end" if is_target else "start"
            node_id = f"__{kind}_{scope() or 'root'}"
            node = graph.ensure(node_id, scope())
            node.shape = kind
            node.lines = []
            return node_id
        graph.ensure(name, scope()).shape = "round"
        return name

    for line in lines[1:]:
        if line == "}":
            if not stack:
                raise MermaidError("`}` without matching composite state")
            stack.pop()
            continue
        match = re.match(r"state\s+(\w+)\s*\{$", line)
        if match:
            graph.ensure(match.group(1), scope()).shape = "round"
            graph.groups[match.group(1)] = Group(match.group(1), [match.group(1)], scope())
            stack.append(match.group(1))
            continue
        match = re.match(r"(\[\*\]|\w+)\s*-->\s*(\[\*\]|\w+)\s*(?::\s*(.*))?$", line)
        if match:
            src = endpoint(match.group(1), False)
            dst = endpoint(match.group(2), True)
            label = wrap_lines(clean_label(match.group(3)), MAX_LABEL_PX) if match.group(3) else []
            graph.edges.append(Edge(src, dst, label))
            continue
        raise MermaidError(f"unsupported stateDiagram line: {line!r}")
    if stack:
        raise MermaidError("composite state not closed")
    return graph


# --------------------------------------------------------------------------------------------------------
# Class diagram parser
# --------------------------------------------------------------------------------------------------------

_CLASS_REL_RE = re.compile(r"(\w+)\s*(<\|--|--\|>|<--|-->|\.\.>|<\.\.|\.\.\|>|<\|\.\.|--)\s*(\w+)\s*(?::\s*(.*))?$")


def parse_class(lines: list[str]) -> Graph:
    """Parse a `classDiagram`: classes with members and relations."""
    graph = Graph(direction="TD")
    current: Node | None = None
    for line in lines[1:]:
        if current is not None:
            if line == "}":
                current = None
            else:
                current.members.append(re.sub(r"~([^~]*)~", r"<\1>", line))
            continue
        match = re.match(r"class\s+(\w+)\s*\{$", line)
        if match:
            current = graph.ensure(match.group(1), "")
            current.shape = "class"
            continue
        match = re.match(r"class\s+(\w+)$", line)
        if match:
            graph.ensure(match.group(1), "").shape = "class"
            continue
        match = _CLASS_REL_RE.match(line)
        if match:
            left, op, right, label = match.groups()
            graph.ensure(left, "").shape = "class"
            graph.ensure(right, "").shape = "class"
            head = "hollow" if "|" in op else ("none" if op == "--" else "arrow")
            src, dst = (right, left) if op.startswith("<") else (left, right)
            text = wrap_lines(clean_label(label), MAX_LABEL_PX) if label else []
            graph.edges.append(Edge(src, dst, text, ".." in op, False, head))
            continue
        raise MermaidError(f"unsupported classDiagram line: {line!r}")
    if current is not None:
        raise MermaidError("class body not closed")
    return graph


# --------------------------------------------------------------------------------------------------------
# Layered layout
# --------------------------------------------------------------------------------------------------------

NODE_PAD_X = 14.0
NODE_PAD_Y = 9.0
LAYER_GAP = 52.0
CROSS_GAP = 26.0


def size_nodes(graph: Graph) -> None:
    """Compute the width and height of every node from its text."""
    for node in graph.nodes.values():
        if node.shape in ("start", "end"):
            node.w = node.h = 18.0
            continue
        if node.shape == "class":
            title_w = text_width(node.id, FONT_SIZE + 1) + 2 * NODE_PAD_X
            member_w = max((text_width(m, 12.0) for m in node.members), default=0.0) + 2 * NODE_PAD_X
            node.w = max(title_w, member_w, 90.0)
            node.h = LINE_HEIGHT + 2 * NODE_PAD_Y
            if node.members:
                node.h += len(node.members) * LINE_HEIGHT + NODE_PAD_Y
            continue
        node.lines = wrap_lines(node.lines, MAX_NODE_PX)
        tw = max((text_width(line) for line in node.lines), default=0.0)
        th = max(len(node.lines), 1) * LINE_HEIGHT
        if node.shape == "diamond":
            node.w = tw * 1.45 + 2 * NODE_PAD_X
            node.h = th * 1.6 + 2 * NODE_PAD_Y
        else:
            node.w = max(tw + 2 * NODE_PAD_X, 44.0)
            node.h = th + 2 * NODE_PAD_Y


@dataclass
class Item:
    """One slot of a layer: a real node or an edge waypoint."""

    key: str
    cross_size: float
    layer_size: float
    node: Node | None = None
    layer: int = 0
    cross: float = 0.0
    along: float = 0.0


@dataclass
class Route:
    """An edge with the layer slots it crosses (forward orientation, low layer to high layer)."""

    edge: Edge
    items: list[Item]
    reversed: bool
    label_seg: int = 0


def _break_cycles(graph: Graph) -> set[int]:
    """Indices of the edges to flip so the graph becomes acyclic (back edges of a DFS)."""
    adjacency: dict[str, list[int]] = {i: [] for i in graph.nodes}
    for index, edge in enumerate(graph.edges):
        if edge.src != edge.dst:
            adjacency[edge.src].append(index)
    flipped: set[int] = set()
    state: dict[str, int] = {}
    for root in graph.nodes:
        if root in state:
            continue
        state[root] = 1
        work: list[tuple[str, int]] = [(root, 0)]
        while work:
            node_id, pos = work.pop()
            if pos >= len(adjacency[node_id]):
                state[node_id] = 2
                continue
            work.append((node_id, pos + 1))
            edge_index = adjacency[node_id][pos]
            target = graph.edges[edge_index].dst
            if state.get(target) == 1:
                flipped.add(edge_index)
            elif target not in state:
                state[target] = 1
                work.append((target, 0))
    return flipped


def layout(graph: Graph) -> list[Route]:
    """Assign layers, order them, place nodes; returns the routed edges. Sets Node.x / Node.y (centres)."""
    horizontal = graph.direction in ("LR", "RL")
    size_nodes(graph)
    ids = list(graph.nodes)
    flipped = _break_cycles(graph)

    oriented: list[tuple[int, str, str]] = []
    for index, edge in enumerate(graph.edges):
        if edge.src == edge.dst:
            continue
        a, b = (edge.dst, edge.src) if index in flipped else (edge.src, edge.dst)
        oriented.append((index, a, b))

    # Longest-path layering (a DAG once the flipped edges are reversed).
    layer: dict[str, int] = dict.fromkeys(ids, 0)
    for _ in range(len(ids) + 1):
        changed = False
        for _, a, b in oriented:
            if layer[b] < layer[a] + 1:
                layer[b] = layer[a] + 1
                changed = True
        if not changed:
            break

    items: dict[str, Item] = {}
    for node_id, node in graph.nodes.items():
        along = node.w if horizontal else node.h
        cross = node.h if horizontal else node.w
        items[node_id] = Item(node_id, cross, along, node, layer[node_id])

    routes: list[Route] = []
    for index, a, b in oriented:
        chain = [items[a]]
        for lv in range(layer[a] + 1, layer[b]):
            chain.append(Item(f"~{index}.{lv}", 2.0, 0.0, None, lv))
        chain.append(items[b])
        routes.append(Route(graph.edges[index], chain, index in flipped, label_seg=(len(chain) - 1) // 2))

    layer_count = max(layer.values(), default=0) + 1
    layers: list[list[Item]] = [[] for _ in range(layer_count)]
    for node_id in ids:
        layers[layer[node_id]].append(items[node_id])
    for route in routes:
        for item in route.items[1:-1]:
            layers[item.layer].append(item)

    up: dict[str, list[Item]] = {}
    down: dict[str, list[Item]] = {}
    for route in routes:
        for left, right in zip(route.items, route.items[1:], strict=False):
            down.setdefault(left.key, []).append(right)
            up.setdefault(right.key, []).append(left)

    def sweep(order: list[int], neighbours: dict[str, list[Item]]) -> None:
        rank = {it.key: i for lyr in layers for i, it in enumerate(lyr)}
        for lv in order:
            current = layers[lv]
            own = {it.key: i for i, it in enumerate(current)}

            def bary(item: Item, own: dict[str, int] = own) -> float:
                near = neighbours.get(item.key, [])
                if not near:
                    return float(own[item.key])
                return sum(rank[n.key] for n in near) / len(near)

            current.sort(key=bary)
            rank.update({it.key: i for i, it in enumerate(current)})

    for _ in range(6):
        sweep(list(range(1, layer_count)), up)
        sweep(list(range(layer_count - 2, -1, -1)), down)

    # The gap between two layers grows with the edge label sitting in it.
    gap_extra = [0.0] * max(layer_count - 1, 1)
    for route in routes:
        if route.edge.label:
            lv = route.items[route.label_seg].layer
            wl = max(text_width(line, 12.0) for line in route.edge.label) + 12
            hl = len(route.edge.label) * LINE_HEIGHT + 6
            gap_extra[lv] = max(gap_extra[lv], (wl if horizontal else hl) + 40 - LAYER_GAP)

    extent = [max((it.layer_size for it in lyr), default=0.0) for lyr in layers]
    layer_pos: list[float] = []
    cursor = 0.0
    for lv in range(layer_count):
        cursor += extent[lv] / 2
        layer_pos.append(cursor)
        cursor += extent[lv] / 2 + LAYER_GAP + (gap_extra[lv] if lv < layer_count - 1 else 0.0)

    def pack(order: list[Item], desired: list[float]) -> None:
        pos: list[float] = []
        for i, item in enumerate(order):
            want = desired[i]
            if i > 0:
                want = max(want, pos[-1] + (order[i - 1].cross_size + item.cross_size) / 2 + CROSS_GAP)
            pos.append(want)
        shift = sum(d - p for d, p in zip(desired, pos, strict=True)) / max(len(pos), 1)
        for item, p in zip(order, pos, strict=True):
            item.cross = p + shift

    for lyr in layers:
        total = sum(it.cross_size for it in lyr) + CROSS_GAP * max(len(lyr) - 1, 0)
        cur = -total / 2
        for it in lyr:
            it.cross = cur + it.cross_size / 2
            cur += it.cross_size + CROSS_GAP

    def mean_cross(item: Item, table: dict[str, list[Item]]) -> float:
        near = table.get(item.key)
        return sum(n.cross for n in near) / len(near) if near else item.cross

    for _ in range(4):
        for lv in range(1, layer_count):
            pack(layers[lv], [mean_cross(it, up) for it in layers[lv]])
        for lv in range(layer_count - 2, -1, -1):
            pack(layers[lv], [mean_cross(it, down) for it in layers[lv]])

    for lv, lyr in enumerate(layers):
        for it in lyr:
            it.along = layer_pos[lv]
            if it.node is not None:
                it.node.x, it.node.y = (it.along, it.cross) if horizontal else (it.cross, it.along)
    if graph.direction == "RL":
        for node in graph.nodes.values():
            node.x = -node.x
    if graph.direction == "BT":
        for node in graph.nodes.values():
            node.y = -node.y
    return routes


# --------------------------------------------------------------------------------------------------------
# Graph drawing
# --------------------------------------------------------------------------------------------------------


def _arrow_head(x: float, y: float, dx: float, dy: float, hollow: bool, stroke: str) -> str:
    """Triangle whose tip is at (x, y) pointing along (dx, dy)."""
    norm = (dx * dx + dy * dy) ** 0.5 or 1.0
    dx, dy = dx / norm, dy / norm
    size = 12.0 if hollow else 9.0
    bx, by = x - dx * size, y - dy * size
    px, py = -dy * size * 0.4, dx * size * 0.4
    fill = BG if hollow else stroke
    return (
        f'<path d="M{x:.1f} {y:.1f} L{bx + px:.1f} {by + py:.1f} L{bx - px:.1f} {by - py:.1f} Z" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="1.2" stroke-linejoin="round"/>'
    )


def _node_edge_point(node: Node, towards_x: float, towards_y: float) -> tuple[float, float]:
    """Point of the node outline on the side facing the target (side midpoints)."""
    dx, dy = towards_x - node.x, towards_y - node.y
    if node.shape in ("start", "end"):
        norm = (dx * dx + dy * dy) ** 0.5 or 1.0
        return node.x + dx / norm * node.w / 2, node.y + dy / norm * node.w / 2
    if abs(dx) * node.h >= abs(dy) * node.w:
        return node.x + (node.w / 2 if dx >= 0 else -node.w / 2), node.y
    return node.x, node.y + (node.h / 2 if dy >= 0 else -node.h / 2)


def _label_box(out: list[str], bounds: Bounds, label: list[str], cx: float, cy: float) -> None:
    w = max(text_width(line, 12.0) for line in label) + 12
    h = len(label) * LINE_HEIGHT + 6
    out.append(
        f'<rect x="{cx - w / 2:.1f}" y="{cy - h / 2:.1f}" width="{w:.1f}" height="{h:.1f}" rx="4" '
        f'fill="{BG}" fill-opacity="0.92"/>'
    )
    out.append(svg_text(cx, cy, label, fill=MUTED, size=12.0))
    bounds.add(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)


def _draw_node(node: Node) -> str:
    fill = node.style.get("fill", NODE_FILL)
    stroke = node.style.get("stroke", NODE_STROKE)
    color = node.style.get("color", TEXT)
    x0, y0 = node.x - node.w / 2, node.y - node.h / 2
    if node.shape == "start":
        return f'<circle cx="{node.x:.1f}" cy="{node.y:.1f}" r="{node.w / 2:.1f}" fill="{EDGE}"/>'
    if node.shape == "end":
        return (
            f'<circle cx="{node.x:.1f}" cy="{node.y:.1f}" r="{node.w / 2:.1f}" fill="{BG}" stroke="{EDGE}" '
            f'stroke-width="1.6"/><circle cx="{node.x:.1f}" cy="{node.y:.1f}" r="{node.w / 4:.1f}" fill="{EDGE}"/>'
        )
    if node.shape == "diamond":
        pts = (
            f"{node.x:.1f},{y0:.1f} {x0 + node.w:.1f},{node.y:.1f} {node.x:.1f},{y0 + node.h:.1f} {x0:.1f},{node.y:.1f}"
        )
        shape = f'<polygon points="{pts}" fill="{fill}" stroke="{stroke}" stroke-width="1.3" stroke-linejoin="round"/>'
        return shape + svg_text(node.x, node.y, node.lines, fill=color)
    if node.shape == "class":
        title_h = LINE_HEIGHT + 2 * NODE_PAD_Y
        out = (
            f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{node.w:.1f}" height="{node.h:.1f}" rx="4" fill="{fill}" '
            f'stroke="{stroke}" stroke-width="1.3"/>'
        )
        out += svg_text(node.x, y0 + title_h / 2, [node.id], fill=color, weight="bold", size=FONT_SIZE + 1)
        if node.members:
            out += (
                f'<line x1="{x0:.1f}" y1="{y0 + title_h:.1f}" x2="{x0 + node.w:.1f}" y2="{y0 + title_h:.1f}" '
                f'stroke="{stroke}" stroke-width="1"/>'
            )
            for i, member in enumerate(node.members):
                cy = y0 + title_h + NODE_PAD_Y / 2 + LINE_HEIGHT * (i + 0.5) + 1
                out += svg_text(x0 + NODE_PAD_X, cy, [member], fill=color, anchor="start", size=12.0)
        return out
    radius = 5 if node.shape == "rect" else min(node.h / 2, 16)
    return (
        f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{node.w:.1f}" height="{node.h:.1f}" rx="{radius:g}" '
        f'fill="{fill}" stroke="{stroke}" stroke-width="1.3"/>' + svg_text(node.x, node.y, node.lines, fill=color)
    )


def _draw_groups(graph: Graph, bounds: Bounds) -> list[str]:
    def members_of(gid: str) -> list[Node]:
        found = [n for n in graph.nodes.values() if n.group == gid]
        for child in graph.groups.values():
            if child.parent == gid:
                found.extend(members_of(child.id))
        return found

    def depth(gid: str) -> int:
        level = 0
        while graph.groups[gid].parent:
            gid = graph.groups[gid].parent
            level += 1
        return level

    out: list[str] = []
    for gid in sorted(graph.groups, key=depth):
        members = members_of(gid)
        if not members:
            continue
        group = graph.groups[gid]
        pad = 16.0
        title_h = len(group.title) * LINE_HEIGHT + 6
        x0 = min(n.x - n.w / 2 for n in members) - pad
        x1 = max(n.x + n.w / 2 for n in members) + pad
        y0 = min(n.y - n.h / 2 for n in members) - pad - title_h
        y1 = max(n.y + n.h / 2 for n in members) + pad
        x1 = max(x1, x0 + max(text_width(t) for t in group.title) + 2 * pad)
        bounds.add(x0, y0, x1, y1)
        out.append(
            f'<rect x="{x0:.1f}" y="{y0:.1f}" width="{x1 - x0:.1f}" height="{y1 - y0:.1f}" rx="8" '
            f'fill="{GROUP_FILL}" stroke="{GROUP_STROKE}" stroke-width="1" stroke-dasharray="5 4"/>'
        )
        out.append(svg_text(x0 + 10, y0 + title_h / 2 + 3, group.title, fill=MUTED, anchor="start"))
    return out


def _route_points(route: Route, horizontal: bool, flow_sign: float) -> list[tuple[float, float]]:
    """Polyline anchors of a route from the real source to the real target."""
    first, last = route.items[0].node, route.items[-1].node
    assert first is not None and last is not None
    way = [
        ((it.along * flow_sign, it.cross) if horizontal else (it.cross, it.along * flow_sign))
        for it in route.items[1:-1]
    ]
    p_start = _node_edge_point(first, *(way[0] if way else (last.x, last.y)))
    p_end = _node_edge_point(last, *(way[-1] if way else (first.x, first.y)))
    points = [p_start, *way, p_end]
    if route.reversed:
        points.reverse()
    return points


def draw_graph(graph: Graph, routes: list[Route]) -> str:
    """Draw a laid-out graph."""
    horizontal = graph.direction in ("LR", "RL")
    flow_sign = -1.0 if graph.direction in ("RL", "BT") else 1.0
    bounds = Bounds()
    edges_svg: list[str] = []
    labels_svg: list[str] = []

    for node in graph.nodes.values():
        bounds.add(node.x - node.w / 2, node.y - node.h / 2, node.x + node.w / 2, node.y + node.h / 2)
    groups_svg = _draw_groups(graph, bounds)

    for route in routes:
        edge = route.edge
        points = _route_points(route, horizontal, flow_sign)
        d = f"M{points[0][0]:.1f} {points[0][1]:.1f}"
        for (ax, ay), (bx, by) in zip(points, points[1:], strict=False):
            if horizontal:
                mx = (ax + bx) / 2
                d += f" C{mx:.1f} {ay:.1f} {mx:.1f} {by:.1f} {bx:.1f} {by:.1f}"
            else:
                my = (ay + by) / 2
                d += f" C{ax:.1f} {my:.1f} {bx:.1f} {my:.1f} {bx:.1f} {by:.1f}"
        dash = ' stroke-dasharray="6 4"' if edge.dashed else ""
        width = 2.4 if edge.thick else 1.4
        edges_svg.append(f'<path d="{d}" fill="none" stroke="{EDGE}" stroke-width="{width}"{dash}/>')
        (ax, ay), (bx, by) = points[-2], points[-1]
        tip = (bx - ax, 0.0) if horizontal else (0.0, by - ay)
        if tip == (0.0, 0.0):
            tip = (1.0, 0.0) if horizontal else (0.0, 1.0)
        if edge.head != "none":
            edges_svg.append(_arrow_head(bx, by, tip[0], tip[1], edge.head == "hollow", EDGE))
        if edge.label:
            seg = len(points) - 2 - route.label_seg if route.reversed else route.label_seg
            (ax, ay), (bx, by) = points[seg], points[seg + 1]
            _label_box(labels_svg, bounds, edge.label, (ax + bx) / 2, (ay + by) / 2)

    for edge in graph.edges:
        if edge.src != edge.dst:
            continue
        node = graph.nodes[edge.src]
        x0, x1 = node.x - node.w * 0.2, node.x + node.w * 0.2
        top = node.y - node.h / 2
        rise = 30.0
        edges_svg.append(
            f'<path d="M{x1:.1f} {top:.1f} C{x1 + 8:.1f} {top - rise:.1f} {x0 - 8:.1f} {top - rise:.1f} '
            f'{x0:.1f} {top:.1f}" fill="none" stroke="{EDGE}" stroke-width="1.4"/>'
        )
        edges_svg.append(_arrow_head(x0, top, 0.0, 1.0, False, EDGE))
        bounds.add(x0 - 10, top - rise - 4, x1 + 10, top)
        if edge.label:
            _label_box(labels_svg, bounds, edge.label, node.x, top - rise - len(edge.label) * LINE_HEIGHT / 2 + 2)

    nodes_svg = [_draw_node(node) for node in graph.nodes.values()]
    return finish_svg(groups_svg + edges_svg + nodes_svg + labels_svg, bounds)


# --------------------------------------------------------------------------------------------------------
# Sequence diagram
# --------------------------------------------------------------------------------------------------------

_SEQ_ARROW_RE = re.compile(
    r"^(?P<a>.+?)\s*(?P<arr>-->>|->>|--x|-x|-->|->|--\)|-\))\s*[+-]?(?P<b>[^:]+?)\s*:\s*(?P<t>.*)$"
)
_SEQ_NOTE_RE = re.compile(r"^[Nn]ote\s+(?P<pos>right of|left of|over)\s+(?P<who>[^:]+?)\s*:\s*(?P<t>.*)$")
_SEQ_BLOCKS = ("loop", "alt", "opt", "par", "critical", "break")


@dataclass
class SeqEvent:
    """One step of a sequence diagram."""

    kind: str  # message, note, block_start, block_else, block_end
    a: int = 0
    b: int = 0
    text: list[str] = field(default_factory=list)
    arrow: str = "->>"
    where: str = ""
    block: str = ""


def parse_sequence(lines: list[str]) -> tuple[list[tuple[str, list[str]]], list[SeqEvent]]:
    """Parse a `sequenceDiagram` into participants (id, title lines) and ordered events."""
    parts: list[tuple[str, list[str]]] = []
    index: dict[str, int] = {}
    events: list[SeqEvent] = []
    depth = 0

    def participant(pid: str, title: str | None = None) -> int:
        pid = pid.strip()
        if pid not in index:
            index[pid] = len(parts)
            parts.append((pid, [pid]))
        if title is not None:
            parts[index[pid]] = (pid, clean_label(title))
        return index[pid]

    for line in lines[1:]:
        match = re.match(r"(?:participant|actor)\s+(.+?)(?:\s+as\s+(.+))?$", line)
        if match:
            participant(match.group(1), match.group(2))
            continue
        if line == "autonumber" or re.match(r"(activate|deactivate)\s", line):
            continue
        match = _SEQ_NOTE_RE.match(line)
        if match:
            who = [w.strip() for w in match.group("who").split(",")]
            a = participant(who[0])
            b = participant(who[1]) if len(who) > 1 else a
            events.append(SeqEvent("note", a, b, clean_label(match.group("t")), where=match.group("pos")))
            continue
        match = re.match(rf"({'|'.join(_SEQ_BLOCKS)})\b\s*(.*)$", line)
        if match:
            depth += 1
            events.append(SeqEvent("block_start", block=match.group(1), text=clean_label(match.group(2))))
            continue
        match = re.match(r"(else|and)\b\s*(.*)$", line)
        if match:
            events.append(SeqEvent("block_else", block=match.group(1), text=clean_label(match.group(2))))
            continue
        if line == "end":
            if depth == 0:
                raise MermaidError("`end` without matching block")
            depth -= 1
            events.append(SeqEvent("block_end"))
            continue
        match = _SEQ_ARROW_RE.match(line)
        if match:
            a = participant(match.group("a"))
            b = participant(match.group("b"))
            events.append(SeqEvent("message", a, b, clean_label(match.group("t")), arrow=match.group("arr")))
            continue
        raise MermaidError(f"unsupported sequenceDiagram line: {line!r}")
    if depth:
        raise MermaidError("block not closed with `end`")
    return parts, events


def _sequence_columns(
    parts: list[tuple[str, list[str]]], events: list[SeqEvent], head_w: list[float]
) -> tuple[list[float], float, float]:
    """Participant x centres, plus the room needed left of the first and right of the last column."""
    n = len(parts)
    gap = [(head_w[i] + head_w[i + 1]) / 2 + 18 for i in range(n - 1)]
    right_margin = 0.0
    left_margin = 0.0
    spans: list[tuple[int, int, float]] = []
    for ev in events:
        if ev.kind == "message":
            ev.text = wrap_lines(ev.text, 230.0)
            tw = max((text_width(t) for t in ev.text), default=0.0)
            if ev.a != ev.b:
                spans.append((min(ev.a, ev.b), max(ev.a, ev.b), tw + 40))
            elif ev.a < n - 1:
                spans.append((ev.a, ev.a + 1, tw + 56))
            else:
                right_margin = max(right_margin, tw + 56)
        elif ev.kind == "note":
            ev.text = wrap_lines(ev.text, 220.0)
            tw = max((text_width(t) for t in ev.text), default=0.0) + 20
            if ev.where == "right of":
                if ev.a < n - 1:
                    spans.append((ev.a, ev.a + 1, tw + 20))
                else:
                    right_margin = max(right_margin, tw + 14)
            elif ev.where == "left of":
                if ev.a > 0:
                    spans.append((ev.a - 1, ev.a, tw + 20))
                else:
                    left_margin = max(left_margin, tw + 14)
            elif ev.a != ev.b:
                spans.append((min(ev.a, ev.b), max(ev.a, ev.b), tw))
            else:
                left_margin = max(left_margin, tw / 2 - head_w[ev.a] / 2)
                right_margin = max(right_margin, tw / 2 - head_w[ev.a] / 2)
    for lo, hi, need in sorted(spans, key=lambda s: s[1] - s[0]):
        have = sum(gap[lo:hi])
        if have < need:
            extra = (need - have) / (hi - lo)
            for k in range(lo, hi):
                gap[k] += extra
    xs = [left_margin + head_w[0] / 2]
    for k in range(n - 1):
        xs.append(xs[-1] + gap[k])
    return xs, left_margin, right_margin


def _seq_arrow_head(x: float, y: float, direction: float, kind: str) -> str:
    if kind.endswith("x"):
        return (
            f'<path d="M{x - 5:.1f} {y - 5:.1f} L{x + 5:.1f} {y + 5:.1f} M{x + 5:.1f} {y - 5:.1f} '
            f'L{x - 5:.1f} {y + 5:.1f}" stroke="{EDGE}" stroke-width="1.6"/>'
        )
    return _arrow_head(x, y, direction, 0.0, False, EDGE)


def draw_sequence(parts: list[tuple[str, list[str]]], events: list[SeqEvent]) -> str:
    """Draw a parsed sequence diagram."""
    n = len(parts)
    if n == 0:
        raise MermaidError("sequenceDiagram without participants")
    head_lines = [wrap_lines(lines, 150.0) for _, lines in parts]
    head_w = [max(text_width(line) for line in lines) + 24 for lines in head_lines]
    head_h = max(len(lines) for lines in head_lines) * LINE_HEIGHT + 16
    xs, _, right_margin = _sequence_columns(parts, events, head_w)

    bounds = Bounds()
    body: list[str] = []
    blocks_svg: list[str] = []
    y = head_h + 16.0
    stack: list[tuple[str, list[str], float]] = []
    x_left = xs[0] - head_w[0] / 2 - 6
    x_right = xs[-1] + head_w[-1] / 2 + 6 + right_margin

    for ev in events:
        if ev.kind == "message":
            lines = ev.text
            th = len(lines) * LINE_HEIGHT
            dashed = ' stroke-dasharray="6 4"' if ev.arrow.startswith("--") else ""
            if ev.a == ev.b:
                x0 = xs[ev.a]
                top = y + 4
                body.append(
                    f'<path d="M{x0:.1f} {top:.1f} H{x0 + 34:.1f} V{top + 20:.1f} H{x0 + 2:.1f}" fill="none" '
                    f'stroke="{EDGE}" stroke-width="1.4"{dashed}/>'
                )
                body.append(_seq_arrow_head(x0, top + 20, -1.0, ev.arrow))
                body.append(svg_text(x0 + 42, top + 10, lines, anchor="start"))
                tw = max((text_width(t) for t in lines), default=0.0)
                bounds.add(x0, top, x0 + 42 + tw, top + max(20.0, th))
                y += max(th, 20.0) + 22
            else:
                ya = y + th + 6
                xa, xb = xs[ev.a], xs[ev.b]
                direction = 1.0 if xb > xa else -1.0
                body.append(
                    f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb - direction:.1f}" y2="{ya:.1f}" '
                    f'stroke="{EDGE}" stroke-width="1.4"{dashed}/>'
                )
                body.append(_seq_arrow_head(xb, ya, direction, ev.arrow))
                body.append(svg_text((xa + xb) / 2, y + th / 2 + 1, lines))
                y = ya + 16
        elif ev.kind == "note":
            lines = ev.text
            tw = max((text_width(t) for t in lines), default=0.0) + 20
            th = len(lines) * LINE_HEIGHT + 12
            if ev.where == "right of":
                nx = xs[ev.a] + 10
            elif ev.where == "left of":
                nx = xs[ev.a] - 10 - tw
            else:
                tw = max(tw, abs(xs[ev.b] - xs[ev.a]) + 20)
                nx = (xs[ev.a] + xs[ev.b]) / 2 - tw / 2
            body.append(
                f'<rect x="{nx:.1f}" y="{y:.1f}" width="{tw:.1f}" height="{th:.1f}" rx="3" fill="#3b3a2a" '
                f'stroke="#8a7f3c" stroke-width="1"/>'
            )
            body.append(svg_text(nx + tw / 2, y + th / 2, lines, fill="#e9e3b4"))
            bounds.add(nx, y, nx + tw, y + th)
            y += th + 12
        elif ev.kind == "block_start":
            stack.append((ev.block, ev.text, y))
            y += 30
        elif ev.kind == "block_else":
            inset = 8 * len(stack)
            body.append(
                f'<line x1="{x_left + inset:.1f}" y1="{y:.1f}" x2="{x_right - inset:.1f}" y2="{y:.1f}" '
                f'stroke="{GROUP_STROKE}" stroke-width="1" stroke-dasharray="5 4"/>'
            )
            if ev.text and ev.text[0]:
                body.append(
                    svg_text(x_left + inset + 8, y + 10, [f"[{ev.text[0]}]"], fill=MUTED, anchor="start", size=12.0)
                )
            y += 22
        elif ev.kind == "block_end":
            kind, title, y0 = stack.pop()
            inset = 8 * len(stack)
            bx0, bx1, y1 = x_left + inset, x_right - inset, y + 2
            tab_w = text_width(kind) + 16
            blocks_svg.append(
                f'<rect x="{bx0:.1f}" y="{y0:.1f}" width="{bx1 - bx0:.1f}" height="{y1 - y0:.1f}" rx="4" '
                f'fill="none" stroke="{GROUP_STROKE}" stroke-width="1.2"/>'
            )
            blocks_svg.append(
                f'<rect x="{bx0:.1f}" y="{y0:.1f}" width="{tab_w:.1f}" height="18" rx="3" '
                f'fill="{GROUP_FILL}" stroke="{GROUP_STROKE}" stroke-width="1.2"/>'
            )
            blocks_svg.append(svg_text(bx0 + tab_w / 2, y0 + 9, [kind], size=12.0, weight="bold"))
            if title and title[0]:
                blocks_svg.append(
                    svg_text(bx0 + tab_w + 8, y0 + 9, [f"[{title[0]}]"], fill=MUTED, anchor="start", size=12.0)
                )
            bounds.add(bx0, y0, bx1, y1)
            y += 12
    if stack:
        raise MermaidError("block not closed with `end`")

    y_end = y + 6
    life: list[str] = []
    boxes: list[str] = []
    for i, x in enumerate(xs):
        life.append(
            f'<line x1="{x:.1f}" y1="{head_h:.1f}" x2="{x:.1f}" y2="{y_end:.1f}" '
            f'stroke="{GROUP_STROKE}" stroke-width="1" stroke-dasharray="4 4"/>'
        )
        for top in (0.0, y_end):
            boxes.append(
                f'<rect x="{x - head_w[i] / 2:.1f}" y="{top:.1f}" width="{head_w[i]:.1f}" height="{head_h:.1f}" '
                f'rx="5" fill="{NODE_FILL}" stroke="{NODE_STROKE}" stroke-width="1.3"/>'
            )
            boxes.append(svg_text(x, top + head_h / 2, head_lines[i]))
        bounds.add(x - head_w[i] / 2, 0, x + head_w[i] / 2, y_end + head_h)
    bounds.add(x_left, 0, x_right, y_end + head_h)
    return finish_svg(life + blocks_svg + body + boxes, bounds)


# --------------------------------------------------------------------------------------------------------
# Entry points
# --------------------------------------------------------------------------------------------------------


def render(source: str) -> str:
    """Render one mermaid diagram source to an SVG document."""
    lines = split_statements(source)
    if not lines:
        raise MermaidError("empty diagram")
    kind = lines[0].split()[0]
    parse: Callable[[list[str]], Graph]
    if kind in ("flowchart", "graph"):
        parse = parse_flowchart
    elif kind in ("stateDiagram-v2", "stateDiagram"):
        parse = parse_state
    elif kind == "classDiagram":
        parse = parse_class
    elif kind == "sequenceDiagram":
        parts, events = parse_sequence(lines)
        return draw_sequence(parts, events)
    else:
        raise MermaidError(f"unsupported diagram type {kind!r}")
    graph = parse(lines)
    return draw_graph(graph, layout(graph))


def diagram_kind(source: str) -> str:
    """Human name of a diagram, used as alt text."""
    first = (split_statements(source) or [""])[0].split()[0]
    return {
        "flowchart": "Flowchart",
        "graph": "Flowchart",
        "stateDiagram-v2": "State diagram",
        "stateDiagram": "State diagram",
        "sequenceDiagram": "Sequence diagram",
        "classDiagram": "Class diagram",
    }.get(first, "Diagram")


def diagram_hash(source: str) -> str:
    """Stable file stem of a diagram (content hash)."""
    return hashlib.sha256(source.strip().encode("utf-8")).hexdigest()[:16]


_FENCE_RE = re.compile(
    r"^(?P<indent>[ \t]*)```mermaid[ \t]*\r?\n(?P<body>.*?)^(?P=indent)```[ \t]*$", re.DOTALL | re.MULTILINE
)


def rewrite_markdown(text: str, out_dir: Path, prefix: str = "images/mermaid") -> str:
    """Replace every mermaid fence by an image reference and write the SVG files into `out_dir`."""
    out_dir.mkdir(parents=True, exist_ok=True)

    def replace(match: re.Match[str]) -> str:
        indent = match.group("indent")
        body = "\n".join(
            line[len(indent) :] if line.startswith(indent) else line
            for line in match.group("body").replace("\r\n", "\n").split("\n")
        )
        try:
            svg = render(body)
        except MermaidError as error:
            raise MermaidError(f"{error}\n--- diagram ---\n{body}") from error
        stem = diagram_hash(body)
        (out_dir / f"{stem}.svg").write_text(svg, encoding="utf-8")
        return f"{indent}![{diagram_kind(body)}]({prefix}/{stem}.svg)"

    return _FENCE_RE.sub(replace, text)


def bundle(help_dir: Path) -> int:
    """Rewrite the mermaid fences of every `*.md` of a help bundle; returns the number of diagrams."""
    out_dir = help_dir / "images" / "mermaid"
    if out_dir.is_dir():
        for stale in out_dir.glob("*.svg"):
            stale.unlink()
    count = 0
    for page in sorted(help_dir.glob("*.md")):
        original = page.read_text(encoding="utf-8")
        if "```mermaid" not in original:
            continue
        try:
            rewritten = rewrite_markdown(original, out_dir)
        except MermaidError as error:
            raise MermaidError(f"{page.name}: {error}") from error
        count += len(_FENCE_RE.findall(original))
        page.write_text(rewritten, encoding="utf-8")
    return count


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    one = sub.add_parser("render", help="render one .mmd file")
    one.add_argument("input", type=Path)
    one.add_argument("output", type=Path)
    many = sub.add_parser("bundle", help="rewrite the mermaid fences of a help bundle in place")
    many.add_argument("help_dir", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "render":
            args.output.write_text(render(args.input.read_text(encoding="utf-8")), encoding="utf-8")
        else:
            print(f"mermaid_svg: {bundle(args.help_dir)} diagram(s) rendered")
    except MermaidError as error:
        print(f"mermaid_svg: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
