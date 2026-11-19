"""Builds GameFiles/Skyrim/Interface/SkyrimBetterTogether/compass.swf, the HUD helper the client loads into the game's HUD.

The client adds other players and their waypoints to the compass as custom markers (CompassMarkers.cpp) and then calls
SBT_Tint(localColour, first, playerSetFrame, colour0, kind0, colour1, kind1, ...) on the HUD menu. For every compass
marker clip the helper then
- kind 0, a player: hides the marker's own icon and draws a disc in the player's colour, ringed in white,
- kind 1, a waypoint: shows the custom marker's icon tinted in its player's colour,
- the game's own custom marker, while in a party: tints it in the local player's colour,
- anything else: undoes the above, as the game reuses marker clips for other markers.

There is no ActionScript 2 compiler here, so this assembles the bytecode itself. Run: python Tools/Hud/compass.py
"""
import pathlib
import struct

OUTPUT = pathlib.Path(__file__).resolve().parents[2] / "GameFiles/Skyrim/Interface/SkyrimBetterTogether/compass.swf"

SIMPLE = {
    "GetVariable": 0x1C, "GetMember": 0x4E, "SetMember": 0x4F, "CallMethod": 0x52, "NewObject": 0x40, "Pop": 0x17,
    "Return": 0x3E, "Add2": 0x47, "Subtract": 0x0B, "Multiply": 0x0C, "Divide": 0x0D, "Less2": 0x48, "Equals2": 0x49,
    "Not": 0x12, "Increment": 0x50, "InitObject": 0x43,
}


class Reg(int):
    pass


UNDEFINED = object()


def push(*values):
    body = b""
    for value in values:
        if value is UNDEFINED:
            body += b"\x03"
        elif isinstance(value, bool):
            body += b"\x05" + bytes([value])
        elif isinstance(value, Reg):
            body += b"\x04" + bytes([int(value)])
        elif isinstance(value, int):
            body += b"\x07" + struct.pack("<i", value)
        elif isinstance(value, float):
            raw = struct.pack("<d", value)
            body += b"\x06" + raw[4:] + raw[:4]  # SWF doubles store their high word first
        elif isinstance(value, str):
            body += b"\x00" + value.encode("latin1") + b"\0"
        else:
            raise TypeError(value)
    return ("raw", 0x96, body)


def op(name):
    return ("raw", SIMPLE[name], None)


def store(register):
    return [("raw", 0x87, bytes([register])), op("Pop")]


def label(name):
    return ("label", name)


def jump(name):
    return ("jump", 0x99, name)


def branch(name):
    return ("jump", 0x9D, name)


def function(params, registers, flags, body):
    """DefineFunction2 without a name, which leaves the function on the stack. params: [(register, name)]"""
    return ("function", params, registers, flags, assemble(body))


def flatten(items):
    for item in items:
        if isinstance(item, list):
            yield from flatten(item)
        else:
            yield item


def encode(item, offset=0, labels=None):
    kind = item[0]
    if kind == "raw":
        _, code, body = item
        return bytes([code]) if body is None else bytes([code]) + struct.pack("<H", len(body)) + body
    if kind == "jump":
        _, code, target = item
        delta = 0 if labels is None else labels[target] - (offset + 5)
        return bytes([code]) + struct.pack("<Hh", 2, delta)
    if kind == "function":
        _, params, registers, flags, code = item
        body = b"\0" + struct.pack("<HBH", len(params), registers, flags)
        for register, name in params:
            body += bytes([register]) + name.encode("latin1") + b"\0"
        body += struct.pack("<H", len(code))
        return b"\x8E" + struct.pack("<H", len(body)) + body + code
    return b""


def assemble(items):
    items = list(flatten(items))
    labels, offset = {}, 0
    for item in items:
        if item[0] == "label":
            labels[item[1]] = offset
        else:
            offset += len(encode(item))
    code, offset = b"", 0
    for item in items:
        chunk = encode(item, offset, labels)
        code += chunk
        offset += len(chunk)
    return code


def call(target, method, *args):
    """target.method(args), result left on the stack. Arguments go on the stack last one first."""
    return [push(*reversed(args), len(args)), target, push(method), op("CallMethod")]


def member(target, name):
    return [target, push(name), op("GetMember")]


def circle(shape, cx, cy, radius):
    """A circle of eight curves around registers cx, cy, radius scale an expression on the stack."""
    anchors = [(1, 0), (0.70710678, 0.70710678), (0, 1), (-0.70710678, 0.70710678), (-1, 0), (-0.70710678, -0.70710678), (0, -1), (0.70710678, -0.70710678)]
    controls = [(1, 0.41421356), (0.41421356, 1), (-0.41421356, 1), (-1, 0.41421356), (-1, -0.41421356), (-0.41421356, -1), (0.41421356, -1), (1, -0.41421356)]

    def coordinate(centre, factor):
        return [push(Reg(centre), Reg(radius), float(factor)), op("Multiply"), op("Add2")]

    out = [coordinate(cy, 0), coordinate(cx, 1), push(2), push(Reg(shape), "moveTo"), op("CallMethod"), op("Pop")]
    for (ax, ay), (qx, qy) in zip(anchors[1:] + anchors[:1], controls):
        out += [coordinate(cy, ay), coordinate(cx, ax), coordinate(cy, qy), coordinate(cx, qx), push(4), push(Reg(shape), "curveTo"), op("CallMethod"), op("Pop")]
    return out


def colour_object(art):
    """new Color(art), on the stack"""
    return [push(Reg(art), 1, "Color"), op("NewObject")]


# SBT_Apply(m, colour, kind), this = the HUD menu. Registers: 2 m, 3 colour, 4 kind, 5 art, 6 shape, 7 bounds,
# 8 centre x, 9 centre y, 10 radius, 11 inner radius.
APPLY = [
    # The marker's icon is placed on its timeline at depth 1, which ActionScript sees 16384 lower
    call(push(Reg(2)), "getInstanceAtDepth", -16383), store(5),
    push(Reg(4), 0), op("Less2"), op("Not"), branch("mine"),
    # Not one of ours: undo what an earlier marker left on this clip
    member(push(Reg(2)), "sbtKind"), push(UNDEFINED), op("Equals2"), op("Not"), branch("reset"),
    push(UNDEFINED), op("Return"),
    label("reset"),
    push(Reg(2), "sbtKind", UNDEFINED), op("SetMember"),
    call(member(push(Reg(2)), "sbtIcon"), "removeMovieClip"), op("Pop"),
    push(Reg(5), UNDEFINED), op("Equals2"), op("Not"), branch("restore"),
    push(UNDEFINED), op("Return"),
    label("restore"),
    push(Reg(5), "_visible", True), op("SetMember"),
    push("ra", 100, "rb", 0, "ga", 100, "gb", 0, "ba", 100, "bb", 0, "aa", 100, "ab", 0, 8), op("InitObject"),
    push(1), colour_object(5), push("setTransform"), op("CallMethod"), op("Pop"),
    push(UNDEFINED), op("Return"),

    label("mine"),
    # Without the icon there is nothing to tint or to size the disc on, and NaN bounds would stop the whole HUD drawing
    push(Reg(5), UNDEFINED), op("Equals2"), op("Not"), branch("found"),
    push(UNDEFINED), op("Return"),
    label("found"),
    push(Reg(4), 0), op("Equals2"), branch("player"),
    # A waypoint: the custom marker's icon in its player's colour
    member(push(Reg(2)), "sbtKind"), push(0), op("Equals2"), op("Not"), branch("tint"),
    call(member(push(Reg(2)), "sbtIcon"), "removeMovieClip"), op("Pop"),
    label("tint"),
    push(Reg(5), "_visible", True), op("SetMember"),
    push(Reg(3), 1), colour_object(5), push("setRGB"), op("CallMethod"), op("Pop"),
    jump("done"),

    # A player: a disc of their own over where the marker's icon would be
    label("player"),
    push(Reg(5), "_visible", False), op("SetMember"),
    member(push(Reg(2)), "sbtKind"), push(0), op("Equals2"), op("Not"), branch("draw"),
    member(push(Reg(2)), "sbtColour"), push(Reg(3)), op("Equals2"), branch("done"),
    label("draw"),
    call(push(Reg(5)), "getBounds", Reg(2)), store(7),
    member(push(Reg(7)), "xMin"), member(push(Reg(7)), "xMax"), op("Add2"), push(2), op("Divide"), store(8),
    member(push(Reg(7)), "yMin"), member(push(Reg(7)), "yMax"), op("Add2"), push(2), op("Divide"), store(9),
    member(push(Reg(7)), "yMax"), member(push(Reg(7)), "yMin"), op("Subtract"), push(0.36), op("Multiply"), store(10),
    push(Reg(10), 0.38), op("Multiply"), store(11),
    call(push(Reg(2)), "createEmptyMovieClip", "sbtIcon", 100), store(6),
    call(push(Reg(6)), "lineStyle", 2, 0xFFFFFF, 100), op("Pop"),
    call(push(Reg(6)), "beginFill", Reg(3), 100), op("Pop"),
    circle(6, 8, 9, 10),
    call(push(Reg(6)), "endFill"), op("Pop"),
    call(push(Reg(6)), "lineStyle"), op("Pop"),
    call(push(Reg(6)), "beginFill", 0xFFFFFF, 100), op("Pop"),
    circle(6, 8, 9, 11),
    call(push(Reg(6)), "endFill"), op("Pop"),

    label("done"),
    push(Reg(2), "sbtKind", Reg(4)), op("SetMember"),
    push(Reg(2), "sbtColour", Reg(3)), op("SetMember"),
    push(UNDEFINED), op("Return"),
]

# SBT_Tint(localColour, first, playerSetFrame, colour, kind, ...), this = the HUD menu. Registers: 1 this, 2 arguments,
# 3 local colour, 4 first, 5 custom marker frame, 6 marker list, 7 index, 8 clip, 9 index among ours, 10 colour, 11 kind,
# 12 index of the kind in the arguments.
TINT = [
    member(push(Reg(1)), "CompassMarkerList"), store(6),
    push(0), store(7),
    label("loop"),
    push(Reg(7)), member(push(Reg(6)), "length"), op("Less2"), op("Not"), branch("end"),
    push(Reg(6), Reg(7)), op("GetMember"), push("movie"), op("GetMember"), store(8),
    push(Reg(7), Reg(4)), op("Subtract"), store(9),
    push(-1), store(10),
    push(-1), store(11),
    push(Reg(9), 2), op("Multiply"), push(4), op("Add2"), store(12),
    push(Reg(9), 0), op("Less2"), branch("own"),
    push(Reg(12)), member(push(Reg(2)), "length"), op("Less2"), op("Not"), branch("own"),
    push(Reg(2), Reg(12), 1), op("Subtract"), op("GetMember"), store(10),
    push(Reg(2), Reg(12)), op("GetMember"), store(11),
    jump("apply"),
    label("own"),
    push(Reg(3), 0), op("Less2"), branch("apply"),
    member(push(Reg(8)), "_currentframe"), push(Reg(5)), op("Equals2"), op("Not"), branch("apply"),
    push(Reg(3)), store(10),
    push(1), store(11),
    label("apply"),
    call(push(Reg(1)), "SBT_Apply", Reg(8), Reg(10), Reg(11)), op("Pop"),
    push(Reg(7)), op("Increment"), store(7),
    jump("loop"),
    label("end"),
    push(True), op("Return"),
]

PRELOAD_THIS = 0x0001
PRELOAD_ARGUMENTS = 0x0004
SUPPRESS_ARGUMENTS = 0x0008
SUPPRESS_SUPER = 0x0020

# Frame 1 of the helper, loaded as a clip of the HUD menu: install both functions on it
FRAME = assemble([
    push("this"), op("GetVariable"), push("_parent"), op("GetMember"), push("SBT_Apply"),
    function([(2, "m"), (3, "colour"), (4, "kind")], 12, PRELOAD_THIS | SUPPRESS_ARGUMENTS | SUPPRESS_SUPER, APPLY),
    op("SetMember"),
    push("this"), op("GetVariable"), push("_parent"), op("GetMember"), push("SBT_Tint"),
    function([(3, "localColour"), (4, "first"), (5, "playerSetFrame")], 13, PRELOAD_THIS | PRELOAD_ARGUMENTS | SUPPRESS_SUPER, TINT),
    op("SetMember"),
]) + b"\0"


def tag(code, body):
    if len(body) < 0x3F:
        return struct.pack("<H", code << 6 | len(body)) + body
    return struct.pack("<HI", code << 6 | 0x3F, len(body)) + body


def build():
    # 20x20 pixel stage, in twips with 10 bit fields
    bits = "{:05b}".format(10) + "".join("{:010b}".format(v) for v in (0, 400, 0, 400))
    bits += "0" * (-len(bits) % 8)
    rect = int(bits, 2).to_bytes(len(bits) // 8, "big")
    body = rect + struct.pack("<HH", 30 << 8, 1)
    body += tag(69, struct.pack("<I", 0))  # FileAttributes: ActionScript 2
    body += tag(12, FRAME) + tag(1, b"") + tag(0, b"")
    return b"FWS" + bytes([8]) + struct.pack("<I", 8 + len(body)) + body


if __name__ == "__main__":
    data = build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_bytes(data)
    print(OUTPUT, len(data))
