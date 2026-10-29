"""Check Fast Place's 26.x bindings against local official client classfiles."""
import argparse
from pathlib import Path
from minecraft_classfile import ClassArchive

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--client", type=Path, required=True)
args = parser.parse_args()

checks = [
    ("net/minecraft/client/Options", "field", "keyUse", "Lnet/minecraft/client/KeyMapping;"),
    ("net/minecraft/client/Minecraft", "field", "rightClickDelay", "I"),
    ("net/minecraft/client/player/LocalPlayer", "method", "getMainHandItem", "()Lnet/minecraft/world/item/ItemStack;"),
    ("net/minecraft/client/player/LocalPlayer", "method", "getOffhandItem", "()Lnet/minecraft/world/item/ItemStack;"),
    ("net/minecraft/world/item/ItemStack", "method", "isEmpty", "()Z"),
    ("net/minecraft/world/item/ItemStack", "method", "getItem", "()Lnet/minecraft/world/item/Item;"),
]
with ClassArchive(args.client) as client:
    for owner, kind, name, descriptor in checks:
        result = client.find(owner, kind, name, descriptor)
        if result is None or result[1] & 0x0008:
            raise SystemExit(f"Missing or incompatible binding: {owner}.{name}{descriptor}")
    def inherits(name, parent):
        pending, visited = [name], set()
        while pending:
            current = pending.pop()
            if current == parent:
                return True
            if current in visited:
                continue
            visited.add(current)
            info = client.get(current)
            if info is not None:
                pending.extend(info.parents)
        return False
    prefix = "net/minecraft/world/item/"
    for child, parent in (("BlockItem", "Item"), ("PotionItem", "Item"),
                          ("SplashPotionItem", "PotionItem"), ("LingeringPotionItem", "PotionItem")):
        if not inherits(prefix + child, prefix + parent):
            raise SystemExit(f"Item inheritance mismatch: {child} -> {parent}")
print(f"PASS {args.client.name}: Fast Place bindings and item classes")
