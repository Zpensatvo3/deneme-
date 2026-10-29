"""Audit Java gameplay bindings against local official Minecraft client archives.

Use --intermediary for the obfuscated 1.21.11 client; otherwise checks the
shared named Sprint, Fast Place, No Jump Delay and Fast EXP bindings for 26.x.
"""
import argparse
from pathlib import Path
from minecraft_classfile import ClassArchive, read_intermediary

NAMED_CHECKS = [
    ('net/minecraft/client/KeyMapping', 'method', 'isDown', '()Z', False),
    ('net/minecraft/client/Minecraft', 'field', 'instance', 'Lnet/minecraft/client/Minecraft;', True),
    ('net/minecraft/client/Minecraft', 'field', 'level', 'Lnet/minecraft/client/multiplayer/ClientLevel;', False),
    ('net/minecraft/client/Minecraft', 'field', 'options', 'Lnet/minecraft/client/Options;', False),
    ('net/minecraft/client/Minecraft', 'field', 'player', 'Lnet/minecraft/client/player/LocalPlayer;', False),
    ('net/minecraft/client/Minecraft', 'field', 'rightClickDelay', 'I', False),
    ('net/minecraft/client/Minecraft', 'field', 'screen', 'Lnet/minecraft/client/gui/screens/Screen;', False),
    ('net/minecraft/client/Minecraft', 'method', 'isSameThread', '()Z', False),
    ('net/minecraft/client/Minecraft', 'method', 'startUseItem', '()V', False),
    ('net/minecraft/client/Options', 'field', 'keySprint', 'Lnet/minecraft/client/KeyMapping;', False),
    ('net/minecraft/client/Options', 'field', 'keyUse', 'Lnet/minecraft/client/KeyMapping;', False),
    ('net/minecraft/client/player/LocalPlayer', 'field', 'noJumpDelay', 'I', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'canStartSprinting', '()Z', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'getMainHandItem', '()Lnet/minecraft/world/item/ItemStack;', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'getOffhandItem', '()Lnet/minecraft/world/item/ItemStack;', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'isAlive', '()Z', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'isSprinting', '()Z', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'isUsingItem', '()Z', False),
    ('net/minecraft/client/player/LocalPlayer', 'method', 'setSprinting', '(Z)V', False),
    ('net/minecraft/world/item/ItemStack', 'method', 'getItem', '()Lnet/minecraft/world/item/Item;', False),
    ('net/minecraft/world/item/ItemStack', 'method', 'isEmpty', '()Z', False),
    ('net/minecraft/world/item/Items', 'field', 'EXPERIENCE_BOTTLE', 'Lnet/minecraft/world/item/Item;', True)
]
INTERMEDIARY_CHECKS = [
    ('class_310', 'field', 'field_1700', 'Lnet/minecraft/class_310;', True),
    ('class_310', 'field', 'field_1724', 'Lnet/minecraft/class_746;', False),
    ('class_310', 'field', 'field_1687', 'Lnet/minecraft/class_638;', False),
    ('class_310', 'field', 'field_1755', 'Lnet/minecraft/class_437;', False),
    ('class_310', 'field', 'field_1752', 'I', False),
    ('class_310', 'method', 'method_18854', '()Z', False),
    ('class_310', 'method', 'method_1583', '()V', False),
    ('class_746', 'method', 'method_5805', '()Z', False),
    ('class_746', 'method', 'method_6047', '()Lnet/minecraft/class_1799;', False),
    ('class_746', 'method', 'method_6079', '()Lnet/minecraft/class_1799;', False),
    ('class_1799', 'method', 'method_7960', '()Z', False),
    ('class_1799', 'method', 'method_7909', '()Lnet/minecraft/class_1792;', False),
    ('class_1802', 'field', 'field_8287', 'Lnet/minecraft/class_1792;', True)
]


def check_named(client):
    for owner, kind, name, descriptor, is_static in NAMED_CHECKS:
        result = client.find(owner, kind, name, descriptor)
        if result is None or bool(result[1] & 0x0008) != is_static:
            raise ValueError(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
    prefix = "net/minecraft/world/item/"
    for child, parent in (("BlockItem", "Item"), ("PotionItem", "Item"),
                          ("SplashPotionItem", "PotionItem"), ("LingeringPotionItem", "PotionItem")):
        pending, visited = [prefix + child], set()
        while pending:
            current = pending.pop()
            if current == prefix + parent:
                break
            if current in visited:
                continue
            visited.add(current)
            info = client.get(current)
            if info is not None:
                pending.extend(info.parents)
        else:
            raise ValueError(f"Item inheritance mismatch: {child} -> {parent}")
    return len(NAMED_CHECKS)


def check_intermediary(client, mapping):
    classes, members = read_intermediary(mapping)
    owners = {intermediary: official for official, intermediary in classes.items()}
    for owner, kind, name, descriptor, is_static in INTERMEDIARY_CHECKS:
        runtime_owner = owners.get("net/minecraft/" + owner)
        candidates = members.get((kind, name, descriptor), [])
        for _, runtime_name, runtime_descriptor in candidates:
            result = client.find(runtime_owner, kind, runtime_name, runtime_descriptor) if runtime_owner else None
            if result is not None and bool(result[1] & 0x0008) == is_static:
                break
        else:
            raise ValueError(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
    return len(INTERMEDIARY_CHECKS)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--client", type=Path, required=True, action="append")
    parser.add_argument("--intermediary", type=Path)
    args = parser.parse_args()
    for archive in args.client:
        with ClassArchive(archive) as client:
            count = check_intermediary(client, args.intermediary) if args.intermediary else check_named(client)
        print(f"PASS {archive.name}: {count} gameplay bindings" + (" and item inheritance" if not args.intermediary else ""))


if __name__ == "__main__":
    main()
