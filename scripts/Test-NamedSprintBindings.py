"""Check Sprint's exact 26.x bindings against local official client classfiles."""
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
    ("net/minecraft/client/Minecraft", "field", "options", "Lnet/minecraft/client/Options;", False),
    ("net/minecraft/client/Minecraft", "method", "isSameThread", "()Z", False),
    ("net/minecraft/client/Options", "field", "keySprint", "Lnet/minecraft/client/KeyMapping;", False),
    ("net/minecraft/client/KeyMapping", "method", "isDown", "()Z", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "isAlive", "()Z", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "canStartSprinting", "()Z", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "isSprinting", "()Z", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "setSprinting", "(Z)V", False),
    ("net/minecraft/client/player/LocalPlayer", "method", "isUsingItem", "()Z", False),
]
with ClassArchive(args.client) as client:
    for owner, kind, name, descriptor, is_static in checks:
        result = client.find(owner, kind, name, descriptor)
        if result is None or bool(result[1] & 0x0008) != is_static:
            raise SystemExit(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
print(f"PASS {args.client.name}: {len(checks)} Sprint bindings")
