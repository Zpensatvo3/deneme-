"""Small read-only classfile/mapping reader for offline Minecraft port audits.

Does not execute Java, transform game classes, or decide runtime compatibility.
"""
from dataclasses import dataclass
import re
import struct
import zipfile


def remap_descriptor(descriptor, classes):
    return re.sub(r"L([^;]+);", lambda m: "L" + classes.get(m[1], m[1]) + ";", descriptor)


def java_descriptor(name):
    primitives = {"void": "V", "boolean": "Z", "byte": "B", "char": "C", "short": "S",
                  "int": "I", "long": "J", "float": "F", "double": "D"}
    dimensions = 0
    while name.endswith("[]"):
        dimensions += 1
        name = name[:-2]
    return "[" * dimensions + primitives.get(name, "L" + name.replace(".", "/") + ";")


def read_proguard(text):
    """Return obfuscated -> Mojang classes and owner-qualified member records."""
    classes, records = {}, []
    owner = None
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        if not line.startswith(" "):
            match = re.fullmatch(r"(.+) -> (.+):", line)
            if not match:
                raise ValueError("Invalid ProGuard class mapping")
            named, obfuscated = (s.replace(".", "/") for s in match.groups())
            classes[obfuscated] = named
            owner = (named, obfuscated)
            continue
        if owner is None:
            raise ValueError("Member without mapping owner")
        declaration, obfuscated = line.strip().rsplit(" -> ", 1)
        declaration = re.sub(r"^\d+:\d+:", "", declaration)
        if "(" in declaration:
            match = re.fullmatch(r"(\S+) (\S+)\(([^)]*)\)(?::\d+(?::\d+)?)?", declaration)
            if not match:
                raise ValueError("Invalid ProGuard method mapping")
            result, name, args = match.groups()
            descriptor = "(" + "".join(java_descriptor(a) for a in args.split(",") if a) + ")" + java_descriptor(result)
            kind = "method"
        else:
            field_type, name = declaration.split()
            descriptor, kind = java_descriptor(field_type), "field"
        records.append((owner, kind, name, obfuscated, descriptor))
    inverse = {named: obfuscated for obfuscated, named in classes.items()}
    members = {}
    for (named_owner, obfuscated_owner), kind, named, obfuscated, descriptor in records:
        key = (obfuscated_owner, kind, obfuscated, remap_descriptor(descriptor, inverse))
        value = (named_owner, named, descriptor)
        if key in members and members[key] != value:
            raise ValueError("Ambiguous ProGuard member mapping")
        members[key] = value
    return classes, members


def read_intermediary(path):
    with zipfile.ZipFile(path) as jar:
        lines = jar.read("mappings/mappings.tiny").decode("utf-8").splitlines()
    if lines[0] != "tiny\t2\t0\tofficial\tintermediary":
        raise ValueError("Expected Fabric official/intermediary Tiny v2 mappings")
    classes, records = {}, []
    owner = None
    for line in lines[1:]:
        parts = line.split("\t")
        if parts[0] == "c":
            owner = parts[1]
            classes[owner] = parts[2]
        elif len(parts) == 5 and parts[:2] in (["", "m"], ["", "f"]):
            if owner is None:
                raise ValueError("Member without Tiny class")
            records.append((owner, "method" if parts[1] == "m" else "field", parts[3], parts[4], parts[2]))
    members = {}
    for owner, kind, obfuscated, intermediary, descriptor in records:
        key = (kind, intermediary, remap_descriptor(descriptor, classes))
        members.setdefault(key, []).append((owner, obfuscated, descriptor))
    return classes, members


@dataclass(frozen=True)
class ClassInfo:
    name: str
    parents: tuple
    members: dict
    major: int


def read_class(data):
    offset = 0

    def take(count):
        nonlocal offset
        result = data[offset:offset + count]
        if len(result) != count:
            raise ValueError("Truncated classfile")
        offset += count
        return result

    def u1(): return take(1)[0]
    def u2(): return struct.unpack(">H", take(2))[0]
    def u4(): return struct.unpack(">I", take(4))[0]
    if u4() != 0xCAFEBABE:
        raise ValueError("Not a Java classfile")
    u2()
    major, count = u2(), u2()
    pool = [None] * count
    index = 1
    while index < count:
        tag = u1()
        if tag == 1:
            # Modified UTF-8 string constants may contain unpaired surrogates;
            # class/member names and descriptors used here are ASCII.
            pool[index] = take(u2()).decode("utf-8", errors="replace")
        elif tag in (7, 8, 16, 19, 20): pool[index] = u2()
        elif tag in (3, 4, 9, 10, 11, 12, 17, 18): take(4)
        elif tag in (5, 6): take(8); index += 1
        elif tag == 15: take(3)
        else: raise ValueError(f"Unknown constant-pool tag: {tag}")
        index += 1

    def class_name(index): return pool[pool[index]]
    u2()  # class access flags
    name, parent = class_name(u2()), u2()
    parents = ([class_name(parent)] if parent else []) + [class_name(u2()) for _ in range(u2())]

    def skip_attributes():
        for _ in range(u2()):
            u2()
            take(u4())

    members = {}
    for kind in ("field", "method"):
        for _ in range(u2()):
            flags, member, descriptor = u2(), pool[u2()], pool[u2()]
            members[kind, member, descriptor] = flags
            skip_attributes()
    skip_attributes()
    if offset != len(data):
        raise ValueError("Trailing classfile bytes")
    return ClassInfo(name, tuple(parents), members, major)


class ClassArchive:
    def __init__(self, path):
        self.jar = zipfile.ZipFile(path)
        self.cache = {}

    def close(self): self.jar.close()
    def __enter__(self): return self
    def __exit__(self, *_): self.close()

    def get(self, name):
        if name not in self.cache:
            try: self.cache[name] = read_class(self.jar.read(name + ".class"))
            except KeyError: self.cache[name] = None
        return self.cache[name]

    def lineage(self, owner):
        pending, seen = [owner], set()
        while pending:
            current = pending.pop(0)
            if current in seen: continue
            seen.add(current)
            yield current
            info = self.get(current)
            if info: pending.extend(info.parents)

    def find(self, owner, kind, name, descriptor):
        for current in self.lineage(owner):
            info = self.get(current)
            if info and (kind, name, descriptor) in info.members:
                return current, info.members[kind, name, descriptor]
            if name == "<init>": break  # constructors are never inherited
        return None
