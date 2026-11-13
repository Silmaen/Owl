"""
Builder of visual scripts (``.owlvs``) for the generators of the sample games: nodes, links, variables, functions and
custom nodes written in the YAML layout Owl Nest saves, laid out event by event so that the graph reads left to right
when it is opened in the editor.

A node is made with ``Graph.node(type, **values)``; a value that is a pin of another node (``node["Result"]``) links
that pin to the input instead of typing a constant. ``Graph.chain`` links execution pins one after the other.
"""

from __future__ import annotations

from collections.abc import Iterable, MutableSet
from dataclasses import dataclass, field

Value = object


@dataclass(frozen=True)
class Pin:
    """A pin of a node, by name."""

    node: Node
    name: str


@dataclass(eq=False)
class Node:
    """A node of the graph: its type, its typed values and its place."""

    uid: int
    type: str
    values: dict[str, str] = field(default_factory=dict)
    position: tuple[int, int] = (0, 0)
    pure: bool = False
    then: str = "Then"

    def __getitem__(self, name: str) -> Pin:
        return Pin(self, name)

    @property
    def out(self) -> Pin:
        """The usual value output: ``Result`` of an operator, ``Value`` of a variable read."""
        return Pin(self, "Value" if self.type == "var.get" else "Result")


def _text(value: Value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, float):
        text = f"{value:.5f}".rstrip("0").rstrip(".")
        return "0" if text in ("-0", "") else text
    if isinstance(value, (tuple, list)):
        return ", ".join(_text(v) for v in value)
    return str(value)


_PURE_PREFIXES = (
    "math.",
    "compare.",
    "logic.",
    "text.",
    "value.",
    "var.get",
    "matrix.",
    "flow.reroute",
    "table.get",
    "table.has_key",
    "table.length",
    "table.contains",
    "table.make_list",
    "table.break_list",
    "transform.get_transform",
    "transform.make",
    "transform.break",
    "custom.pure",
    "lua.pure",
    "func.pure",
)
"""Node types without execution pins (the engine readers are told with ``pure=True``)."""


class Graph:
    """A visual script being written."""

    def __init__(self, base: int) -> None:
        self.next_id = base
        self.variables: list[str] = []
        self.functions: list[str] = []
        self.custom: list[str] = []
        self.nodes: list[Node] = []
        self.links: list[tuple[int, str, int, str]] = []
        self.groups: list[tuple[str, tuple[int, int], tuple[int, int]]] = []
        self._band = 0
        self._column = 0
        self._by_id: dict[int, Node] = {}
        self._chained: set[int] = set()

    # ------------------------------------------------------------------------------------------------- declarations

    def var(
        self,
        name: str,
        kind: str,
        default: Value,
        exposed: bool = True,
        low: float | None = None,
        high: float | None = None,
    ) -> None:
        """A variable: exposed ones are script properties shown in the inspector."""
        text = _text(default)
        text = _yaml_scalar(text)
        bounds = ""
        if low is not None:
            bounds += f", min: {_text(float(low))}"
        if high is not None:
            bounds += f", max: {_text(float(high))}"
        hidden = "" if exposed else ", exposed: false"
        self.variables.append(f"    - {{name: {name}, type: {kind}, default: {text}{bounds}{hidden}}}")

    def function(
        self,
        name: str,
        inputs: Iterable[tuple[str, str]] = (),
        outputs: Iterable[tuple[str, str]] = (),
        exported: bool = False,
        pure: bool = False,
        category: str = "",
    ) -> None:
        """A function of the graph: its body starts at a ``func.entry`` node of the same name."""

        def params(items: Iterable[tuple[str, str]]) -> str:
            return "[" + ", ".join(f"{{name: {n}, type: {t}}}" for n, t in items) + "]"

        lines = [f"    - name: {name}", f"      inputs: {params(inputs)}", f"      outputs: {params(outputs)}"]
        if exported:
            lines.append("      exported: true")
        if category:
            lines.append(f"      category: {category}")
        if pure:
            lines.append("      pure: true")
        self.functions.append("\n".join(lines))

    def custom_node(
        self,
        name: str,
        title: str,
        inputs: Iterable[tuple[str, str]],
        outputs: Iterable[tuple[str, str]],
        code: str,
        pure: bool = False,
    ) -> None:
        """A custom node: Lua written in the graph, its inputs the parameters of the body."""

        def params(items: Iterable[tuple[str, str]]) -> str:
            return "[" + ", ".join(f"{{name: {n}, type: {t}}}" for n, t in items) + "]"

        body = "\n".join("        " + line if line else "" for line in code.strip("\n").splitlines())
        lines = [
            f"    - name: {name}",
            f"      title: {title}",
            f"      inputs: {params(inputs)}",
            f"      outputs: {params(outputs)}",
        ]
        if pure:
            lines.append("      pure: true")
        lines += ["      code: |", body]
        self.custom.append("\n".join(lines))

    # ------------------------------------------------------------------------------------------------------- nodes

    def node(self, node_type: str, /, **values: Value) -> Node:
        """A node; a value that is a ``Pin`` is linked to the input of that name.

        ``_pure`` tells whether the node has no execution pins (guessed from its type otherwise), ``_then`` names its
        execution exit."""
        self.next_id += 1
        pure = values.pop("_pure", None)
        is_pure = node_type.startswith(_PURE_PREFIXES) if pure is None else bool(pure)
        node = Node(self.next_id, node_type, pure=is_pure, then=str(values.pop("_then", "Then")))
        for name, value in values.items():
            key = name.replace("__", ".") if name.startswith("split__") else name
            if isinstance(value, Pin):
                self.links.append((value.node.uid, value.name, node.uid, key))
            elif value is not None:
                node.values[key] = _text(value)
        self.nodes.append(node)
        self._by_id[node.uid] = node
        if node_type.startswith(("event.", "func.entry")):
            self._chained.add(node.uid)
        if not is_pure:
            node.position = (self._column * 300, self._band)
            self._column += 1
        return node

    def event(self, hook: str, title: str = "") -> Node:
        """An event (``on_create``, ``on_update``...) or a custom event of a name (a callback), in a new band."""
        if self.nodes:
            self._band += 420
        self._column = 0
        if hook.startswith("on_") and hook in _ENGINE_EVENTS:
            node = self.node(f"event.{hook}", _pure=False)
        else:
            node = self.node("event.custom", _pure=False, function=hook)
        self.groups.append((title or hook, node.position, (0, 0)))
        return node

    def entry(self, function: str) -> Node:
        """The entry of a function body, in a new band."""
        if self.nodes:
            self._band += 420
        self._column = 0
        node = self.node("func.entry", _pure=False, function=function)
        self.groups.append((function, node.position, (0, 0)))
        return node

    def call(self, binding: str, /, **values: Value) -> Node:
        """A call of an engine binding (``ui.set_text``); getters are pure."""
        table, name = binding.split(".", 1)
        pure = name.startswith(("get_", "is_", "has_")) or binding in _PURE_READERS
        return self.node(f"call.{binding}", _pure=pure, **values)

    def lua(self, module: str, function: str, pure: bool = False, **values: Value) -> Node:
        """A call of an annotated function of a Lua module of the project."""
        return self.node("lua.pure" if pure else "lua.call", _pure=pure, module=module, function=function, **values)

    def get(self, variable: str) -> Pin:
        """The value of a variable."""
        return self.node("var.get", variable=variable)["Value"]

    def set(self, variable: str, value: Value) -> Node:
        """Set a variable."""
        return self.node("var.set", variable=variable, Value=value)

    def op(self, node_type: str, /, *operands: Value, **values: Value) -> Pin:
        """The result of an operator: positional operands fill ``A`` then ``B``."""
        for name, operand in zip(("A", "B"), operands, strict=False):
            values[name] = operand
        return self.node(node_type, **values)["Result"]

    def const(self, kind: str, value: Value) -> Pin:
        """A constant node (``number``, ``string``, ``boolean``...)."""
        return self.node(f"value.{kind}", Value=value)["Result"]

    def fmt(self, pattern: str, *args: Value) -> Pin:
        """``string.format`` of up to four values."""
        values: dict[str, Value] = {"Format": pattern}
        for name, arg in zip("ABCD", args, strict=False):
            values[name] = arg
        return self.node("text.format", **values)["Result"]

    def branch(self, condition: Value) -> Node:
        """A branch: its ``True`` and ``False`` exits."""
        return self.node("flow.branch", _then="True", Condition=condition)

    def sequence(self) -> Node:
        """A sequence: ``Then 1``, ``Then 2``, ``Then 3``."""
        return self.node("flow.sequence", _then="Then 1")

    # ------------------------------------------------------------------------------------------------------ links

    def link(self, source: Pin, target: Pin) -> None:
        """Link an output pin to an input pin."""
        self.links.append((source.node.uid, source.name, target.node.uid, target.name))

    def _producers(self, node: Node, seen: MutableSet[int]) -> list[Node]:
        """Execution nodes not yet run whose values ``node`` reads (directly or through pure nodes), in order."""
        out: list[Node] = []
        for source_id, _, target_id, target_pin in list(self.links):
            if target_id != node.uid or target_pin == "Exec" or source_id in seen:
                continue
            seen.add(source_id)
            source = self._by_id[source_id]
            if source.pure:
                out += self._producers(source, seen)
            elif source_id not in self._chained:
                out += self._producers(source, seen)
                out.append(source)
        return out

    def _run(self, previous: Pin, node: Node) -> Pin:
        """Link ``previous`` to the execution nodes ``node`` reads that have not run yet, then to ``node``."""
        for step in [*self._producers(node, set()), node]:
            self.links.append((previous.node.uid, previous.name, step.uid, "Exec"))
            self._chained.add(step.uid)
            previous = Pin(step, step.then)
        return previous

    def chain(self, start: Node | Pin, *nodes: Node) -> Node:
        """Link the execution of each node to the next one; ``start`` may be an exit (``branch["True"]``).

        An execution node whose value a node of the chain reads (``settings.get``, a Lua call) and that runs nowhere
        yet is run just before it."""
        previous: Pin = start if isinstance(start, Pin) else Pin(start, start.then)
        last = start if isinstance(start, Node) else start.node
        for node in nodes:
            previous = self._run(previous, node)
            last = node
        return last

    def join(self, exits: list[Node | Pin], node: Node) -> Node:
        """Paths meeting again: each exit (a pin, or the end of a chain given by its last node) runs ``node``."""
        steps = [*self._producers(node, set()), node]
        for start in exits:
            pin = start if isinstance(start, Pin) else Pin(start, start.then)
            self.links.append((pin.node.uid, pin.name, steps[0].uid, "Exec"))
        self._chained.add(steps[0].uid)
        previous = Pin(steps[0], steps[0].then)
        for step in steps[1:]:
            self.links.append((previous.node.uid, previous.name, step.uid, "Exec"))
            self._chained.add(step.uid)
            previous = Pin(step, step.then)
        return node

    # ------------------------------------------------------------------------------------------------------- text

    def _layout(self) -> None:
        """Pure nodes under the first node that reads them, stacked."""
        readers: dict[int, list[int]] = {}
        for source, _, target, _ in self.links:
            readers.setdefault(source, []).append(target)
        by_id = {n.uid: n for n in self.nodes}
        stacked: dict[int, int] = {}

        def place(node: Node, depth: int = 0) -> None:
            if depth > 12:
                return
            for target in readers.get(node.uid, []):
                reader = by_id[target]
                if reader.pure and reader.position == (0, 0) and reader is not node:
                    place(reader, depth + 1)
                if reader.position != (0, 0) or not reader.pure:
                    count = stacked.get(reader.uid, 0)
                    stacked[reader.uid] = count + 1
                    node.position = (reader.position[0] - 160 - 40 * depth, reader.position[1] + 150 + 80 * count)
                    return

        for node in self.nodes:
            if node.pure and node.position == (0, 0):
                place(node)

    def text(self) -> str:
        """The whole ``.owlvs`` file."""
        self._layout()
        out = ["FormatVersion: 1", "VisualScript:"]
        out.append("  variables:" + ("" if self.variables else " []"))
        out += self.variables
        if self.functions:
            out.append("  functions:")
            out += self.functions
        if self.custom:
            out.append("  customNodes:")
            out += self.custom
        out.append("  nodes:")
        for node in self.nodes:
            out += [
                f"    - id: {node.uid}",
                f"      type: {node.type}",
                f"      position: [{node.position[0]}, {node.position[1]}]",
            ]
            if node.values:
                out.append("      values:")
                for key, value in sorted(node.values.items()):
                    out.append(f"        {key}: {_yaml_scalar(value)}")
        out.append("  links:" + ("" if self.links else " []"))
        for source, source_pin, target, target_pin in self.links:
            out.append(
                f"    - {{from: {source}, fromPin: {_yaml_scalar(source_pin)}, to: {target}, "
                f"toPin: {_yaml_scalar(target_pin)}}}"
            )
        if self.groups:
            out.append("  groups:")
            for index, (title, position, _) in enumerate(self.groups):
                band = [n.position for n in self.nodes if position[1] - 20 <= n.position[1] < position[1] + 400]
                left = min(p[0] for p in band) - 40
                right = max(p[0] for p in band) + 300
                bottom = max(p[1] for p in band) + 140
                group_id = self.next_id + 1 + index
                out.append(
                    f"    - {{id: {group_id}, title: {_yaml_scalar(title)}, position: [{left}, {position[1] - 60}],"
                    f" size: [{right - left}, {bottom - position[1] + 60}], color: Red}}"
                )
        return "\n".join(out) + "\n"


def _yaml_scalar(text: str) -> str:
    """A YAML scalar: plain when safe, single-quoted otherwise."""
    if (
        text == ""
        or text != text.strip()
        or any(c in text for c in ":{}[],#&*!|>'\"%@`\n")
        or text.lower()
        in (
            "yes",
            "no",
            "null",
            "~",
            "on",
            "off",
        )
    ):
        if "\n" in text:
            return '"' + text.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'
        return "'" + text.replace("'", "''") + "'"
    return text


_ENGINE_EVENTS = {
    "on_create",
    "on_update",
    "on_tick",
    "on_destroy",
    "on_collision",
    "on_collision_exit",
    "on_joint_break",
    "on_trigger_enter",
    "on_trigger_exit",
    "on_triggered",
    "on_interact",
    "on_timer",
}
"""Hooks with an event node of their own; any other name is a custom event."""

_PURE_READERS = {"engine.version", "time.delta", "time.real_delta", "time.tick_alpha", "scene.find_entity", "ui.find"}
"""Bindings read where their value is used although their name says nothing (``BindingNodes.cpp``)."""
