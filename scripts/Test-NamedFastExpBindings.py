"""Check Fast EXP's 26.x bindings against a local official client classfile."""
import argparse
from pathlib import Path
from minecraft_classfile import ClassArchive

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--client", type=Path, required=True)
args = parser.parse_args()
checks = [
    ("net/minecraft/client/Minecraft", "field", "instance", "Lnet/minecraft/client/Minecraft;", True),
    ("net/minecraft/client/Minecraft", "field", "player", "Lnet/minecraft/client/player/LocalPlayer;", False),
    ("net/minecraft/client/Minecraft", "field", "level", "Lnet/minecraft/client/multiplayer/ClientLevel;", False),
    ("net/minecraft/client/Minecraft", "field", "screen", "Lnet/minecraft/client/gui/screens/Screen;", False),
    ("net/minecraft/client/Minecraft", "field", "rightClickDelay", "I", False),
    ("net/minecraft/client/Minecraft", "method", "isSameThread", "()Z", False),
    ("net/minecraft/client/Minecraft", "method", "startUseItem", "()V", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "isAlive", "()Z", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "getMainHandItem", "()Lnet/minecraft/world/item/ItemStack;", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "getOffhandItem", "()Lnet/minecraft/world/item/ItemStack;", False),
    ("net/minecraft/world/item/ItemStack", "method", "isEmpty", "()Z", False),
    ("net/minecraft/world/item/ItemStack", "method", "getItem", "()Lnet/minecraft/world/item/Item;", False),
    ("net/minecraft/world/item/Items", "field", "EXPERIENCE_BOTTLE", "Lnet/minecraft/world/item/Item;", True),
]
with ClassArchive(args.client) as client:
    for owner, kind, name, descriptor, is_static in checks:
        result = client.find(owner, kind, name, descriptor)
        if result is None or bool(result[1] & 0x0008) != is_static:
            raise SystemExit(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
print(f"PASS {args.client.name}: {len(checks)} Fast EXP bindings")
