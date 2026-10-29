# Minecraft 26.x SDK/JNI audit

Status: **26.1/26.1.1/26.1.2 source integration and static SDK/JNI audits complete;
exact DLL development targets enabled. Windows MSVC builds and in-game acceptance remain.**
The original audit covered 57 registrations; its config IDs/gameplay decisions
were preserved. The current catalog has 36 modules. These 26.1 targets retain
native gameplay except the Java WorldBridge controllers; the retained
1.21.11 hybrid I/O paths are documented separately. These three exact releases
share a verified layout; do not treat `26.x` or later patches as one compatible ABI.

This document preserves the **17–18 October 2026** fixture/audit evidence.
Current build/output instructions belong to [MINECRAFT_TARGETS](MINECRAFT_TARGETS.md).
Historical test counts below are not the current test inventory or a new live run.
The earlier discovery tool's `supported: false` describes that discovery report,
not a reversal of the exact development targets enabled later.

## Patch release verification (26.1.1 and 26.1.2)

Each official patch client jar was verified against its release metadata, size
and SHA-1, then audited using its own compiled target ID: `26101` and `26102`.
Both passed **740/740** SDK/raw-JNI records. Byte comparison against 26.1 found
**no changes in 274 referenced client classes**, including their declaring
superclasses/interfaces. This is not a claim that the entire game is unchanged.
Java 25, authlib 7.0.63, fastutil 8.5.18 and JOML 1.10.8 remain unchanged.

The targets share the 26.1 binding profile through an explicit three-ID compile
guard. Java bridge target allowlists, build properties, build script, manifest
and CI matrix include both patches. Every DLL retains exact-version rejection;
`26103`, prereleases and later releases are unsupported. No copied SDK or module
tree, gameplay change, launcher publication or remote change was needed.

Repeat for each patch, replacing `<patch>` with `26.1.1` or `26.1.2`:

```sh
python scripts/Test-MinecraftSdk.py --target <patch> --client /path/to/client-cache/client-<patch>.jar --compare-client /path/to/client-cache/client-26.1.jar --library-dir /path/to/test-libraries --java-home /path/to/jdk25 --output /path/to/sdk-<patch>.json
python scripts/Test-Minecraft261Offline.py --target <patch>
python scripts/Test-Offline.py --minecraft-version <patch> --jobs 2
```

Keep matching `version-<patch>.json` beside each client jar. `--compare-client`
also validates the 26.1 fixture; its optional report lists changed class bytes
for review, while unresolved contracts always fail the SDK audit.

## Implemented native batch

- `src/platform/adapter/runtime_names.hpp` selects explicit compile-time names
  at binding sites. It is an identity macro for **the 1.21.11 target** and
  never changes generic JNI lookup/caching or external Java/library names.
- `src/sdk/minecraft/fabric_26_1/official_names.hpp` contains 667 generated,
  explicit spelling entries, not copied SDK/module implementations or runtime
  fuzzy lookups. Historical declarations retained for older SDK branches use
  their verified historical spellings; **667 is not a count of working 26.1 APIs**.
  Missing table entries fail compilation instead of using intermediary names.
- Native SDK classes, member names, raw JNI descriptors and native transformer
  class lookup sites now use the selected spelling. Java reflection and ASM
  targets are selected explicitly as described in the bridge batch below.
- Inventory actions use `handleContainerInput` and the game's actual named
  `ContainerInput.PICKUP`, `QUICK_MOVE` and `SWAP` enum fields. No custom packet
  IDs, gameplay decision rules or config settings were changed.
- Camera FOV is read through `Camera.getFov()`, without attempting the removed
  GameRenderer call on 26.1.
- Item extraction reads relocated quad/transform classes, the quad's nested
  `MaterialInfo` sprite/tint metadata and the layer's bounded `IntList` tints.
  It applies the layer's local matrix **before** the existing GUI centering and
  item rotation. This order was checked against official 26.1 class bytecode
  and the decompiled transform bodies, not guessed from the field name.
- The exact ray-query method is private static in 1.21.11 and 26.1. The staged
  1.21.11 binding's previous instance declaration was corrected as well.
- The earlier 93-binding critical check is now supplemented by the exhaustive
  declaration/raw-JNI audit below. These checks establish symbol contracts, not
  the gameplay behavior of every module.

## Complete SDK and raw-JNI audit (2026-10-18)

`scripts/Test-MinecraftSdk.py` preprocesses the actual selected C++ branches,
extracts every binding from all ten SDK domain headers and the Chams/Sky/Vape
private binding headers, and compiles a metadata probe using the real JNI
templates. It checks **696 SDK records: 199 class aliases, 304 methods/constructors
and 193 fields**. Records are per C++ declaration, not unique Java API counts.
It additionally checks **44 raw JNI/class lookup records**, including Fabric
version detection, native-to-embedded-Java interfaces and both registered native
Sky methods (including their native flags). All **740 passed**.

Checks include inherited owners/interfaces, exact descriptors, constructor
ownership, static flags and every descriptor's referenced class. An SDK header
outside the inventory, unknown raw owner/expression, unsupported binding form,
missing library/type or unresolved member fails the audit. The exact 26.1 client
is verified against Mojang's release metadata, size and SHA-1. The Java base
classes come from the selected JDK, including JDK distributions without jmods.

Corrections made without changing module decisions or config IDs:

| Area | 26.1 adaptation |
| --- | --- |
| ItemStack | Read `getItem()` instead of interpreting the new `Holder<Item>` field as an Item. Preserve native `stack.item.get()` callers. |
| Item/block comparisons | Resolve inherited `TypedInstance.is(Object)` with its erased descriptor; typed SDK views preserve existing `isOf.call(Item/Block)` calls. |
| PotRefill | Read `Holder.Reference<Potion>` field descriptors, then expose the existing Holder view to potion matching. |
| Effects | Correct Holder-based effect fields, constructor and has/remove signatures. |
| Other wrappers | Correct LivingEntity's attacker return type, BlockPos offset, item-use packet arguments and User Optional return types. |

Proven-unused historical declarations are excluded **only from the shared 26.1 branch**,
and remain available for the existing four targets: the removed SwordItem alias
(active sword checks use tags), legacy baked-model/color/renderer aliases (the
26.1 icon reader uses render states), the uncalled old pick/click packet wrappers,
the unused account-type helper and unused ClientLevelData time/weather helpers.
No active module was disabled or skipped. The valid ClientLevelData game-time
getter and its class/field remain. No production data or resources were accessed.

External audit fixtures: the client metadata's authlib `7.0.63` and fastutil
`8.5.18`, JOML `1.10.8`, Fabric Loader `0.19.5`, the rendering jars below, and JDK
25. These are test inputs, not additional shipping dependencies. Store
`version-26.1.json` next to `client-26.1.jar`; store dependency jars in an external
library directory:

```sh
python scripts/Test-MinecraftSdk.py --client /path/to/client-cache/client-26.1.jar --library-dir /path/to/test-libraries --java-home /path/to/jdk25 --output /path/to/sdk-26.1.json
python -m unittest discover -s tests -p 'test_minecraft*.py' -v
python scripts/Test-Offline.py --minecraft-version 26.1 --jobs 2
python scripts/Test-JvmLifecycle.py --java-home /path/to/jdk25
```

The lifecycle test starts a disposable Java 25 VM with `-Xcheck:jni`, uses the
production JVMTI class-lookup wrapper, attaches/detaches a native thread, and
retransforms/restores/reinstalls a classfile-69 fixture twice before shutdown.
This passed, as did JNI owner-isolation tests on Java 25. It does not load the
Windows DLL or reproduce Minecraft/mod classloader and render-thread timing.

## Sky bridge batch (2026-10-18)

- Sky uses Fabric's **`rendering.v1.level.LevelRenderEvents`**, `levelState()`
  and Mojang `state.level.LevelRenderState`/`SkyRenderState` on 26.1. The panorama
  callback uses `START_MAIN`, after vanilla sky and before opaque terrain, where
  Fabric permits OpenGL calls. `END_EXTRACTION` updates the per-frame sky color.
  Previous targets retain their existing events and class names.
- The buffer handle is read from `com.mojang.blaze3d.opengl.GlBuffer.handle`.
  The actual 26.1 fog/projection GLSL blocks preserve the offsets used by the
  existing bounded native uniform reader. This does not test a live GL context.
- `FogRenderer.computeFogColor(..., Vector4f)` now fills argument 6 and returns
  void. The 26.1 transformer filters that caller-owned output at normal returns.
  1.21.11 retains the copy-on-filter return-vector path. Both preserve alpha;
  stopping or disabling fog matching preserves the original values.
Use public development fixtures in external directories. Required file names:
`client-<version>.jar`, `intermediary-<1.21.version>.jar`, plus the following test
libraries (not shipped): ASM 9.9 `asm.jar`, `asm-tree.jar`, `asm-commons.jar`,
`asm-analysis.jar`, JOML 1.10.8 `joml.jar`, and `fabric-rendering-<version>.jar`.
The Fabric rendering fixtures inspected in this batch are:

| Minecraft | Fabric API reference | fabric-rendering-v1 |
| --- | --- | --- |
| 1.21.1 | `0.116.17+1.21.1` | `5.2.1+f44c2eea19` |
| 1.21.4 | `0.119.4+1.21.4` | `10.2.1+0d31b09f04` |
| 1.21.8 | `0.136.1+1.21.8` | `12.6.0+486b7ff72c` |
| 1.21.11 | `0.141.6+1.21.11` | `16.2.10+0290ad933e` |
| 26.1 | `0.145.1+26.1` | `23.0.4+c7e428fd47` |
| 26.1.1 | `0.145.4+26.1.1` | `23.0.4+c7e428fdb3` |
| 26.1.2 | `0.155.3+26.1.2` | `23.3.1+e9207d814c` |

```sh
python scripts/Test-MinecraftBridges.py --fixture-dir /path/to/client-cache --library-dir /path/to/test-libraries --java-home /path/to/jdk25
# Repeat --target to test selected releases only:
python scripts/Test-MinecraftBridges.py --fixture-dir /path/to/client-cache --library-dir /path/to/test-libraries --java-home /path/to/jdk25 --target 1.21.11 --target 26.1
```

### Exact development targets

IDs `26100`, `26101` and `26102` are explicit build targets. Unknown IDs,
prereleases and DLL/game mismatches are rejected; no wildcard/fallback is enabled.
Use the canonical [build table](MINECRAFT_TARGETS.md#build) and Java 25 for the
matching Fabric version. A development artifact is not a production release
approval; no launcher manifest is automatically published.

### Tests for this batch

```sh
# No game fixtures, downloads, JVM or Windows required (C++20 compiler needed):
python scripts/Test-Minecraft261Offline.py
python -m unittest discover -s tests -p test_minecraft26_readiness.py -v
# Optional earlier critical-binding subset, in addition to the full audit:
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --native-261 --output /path/to/native-261.json
```

Unlike discovery mode, `--native-261` returns a failure if **any** selected native
binding is absent or has the wrong static flag. It does not build or run a DLL.
`tests/minecraft261_sdk_test.cpp` checks exact version rejection,
Mojang descriptors and finite/bounded affine item transforms. Native adapter,
item-reader and version-manager translation units plus private Chams/Sky/Vape
binding headers are syntax-checked. The ordinary offline suite is separate; the 35-case counts in the dated table
below describe that audit revision, not the present suite size.

To reproduce the generated spelling table, provide local `client-<version>.txt`
Mojang ProGuard mappings and `intermediary-<version>.jar` Fabric v2 mappings for
1.21.11 in one external cache, then run:

```sh
python scripts/Generate-Minecraft261Names.py --cache-dir /path/to/mapping-cache
# Only when intentionally updating the generated header:
python scripts/Generate-Minecraft261Names.py --cache-dir /path/to/mapping-cache --write
```

No game jars, complete vendor mappings, compiler dependencies or secrets are
committed. The first command is a reproducibility check, not an implicit rewrite.

### Remaining owner validation

1. Build/link each exact 26.1/26.1.1/26.1.2 Release x64 target with Windows MSVC.
2. Run the [current full module checklist](MINECRAFT_TARGETS.md#required-game-checks-before-claiming-full-support), including native
   attach/reload, world transitions, inventory operations, Fabric render timing,
   panorama projection/depth, Iris interaction and shutdown on Java 25.

All source-side audits and available offline checks are complete. Windows
compilation and in-game visuals/gameplay have not been verified here.

### Executed validation — historical 18 October audit

| Check | Result |
| --- | --- |
| Complete 26.1 SDK/raw JNI audit | 740/740 records present; no unresolved signatures/types/static/native flags |
| Complete 26.1.1 and 26.1.2 SDK/raw JNI audits | 740/740 records present per exact target |
| Patch class comparison against 26.1 | 274 referenced client classes per patch; zero byte differences |
| Critical native subset | 93/93 signatures present |
| Offline C++ suite, `--minecraft-version 26.1 --jobs 2` | 35 passed |
| Offline C++ suites, `--minecraft-version 26.1.1` and `26.1.2`, `--jobs 2` | 35 passed per patch |
| Native SDK/adapter/item-reader syntax and unknown-target rejection | Passed on all three exact 26.1 targets |
| Default 1.21.1 offline suite, `--jobs 2` | 35 passed |
| Additional 1.21.4/1.21.8/1.21.11 `minecraft_version` and `sdk_mappings` | 6 passed, including standalone header checks |
| Python discovery, `-p 'test_minecraft*.py' -v` | 21 passed |
| Java bridge source/embedded runs | 14 passed across all seven releases |
| Java 25 JNI owner isolation and JVMTI lifecycle | Both passed with `-Xcheck:jni` |
| Infrastructure | 57 module IDs, 123 compile inputs, matching filters and manifest |
| MSBuild target ID mapping and `git diff --check` | Passed |
| Windows MSVC Release x64 / live game | Not executed; remaining owner checks |

## Earlier discovery fixtures — 17 October 2026

Inspected on 2026-10-17 using Mojang's version manifest and official client jars,
with size and SHA-1 checks against the manifest's download metadata. The fixtures
stay in an external development cache, not the repository or shipping DLL.

| Release | Java / classfile | Baseline signatures present | Unmatched baseline bindings |
| --- | --- | --- | --- |
| 26.1 | 25 / 69 | 69 / 80 | 11 |
| 26.1.1 | 25 / 69 | 69 / 80 | 11 |
| 26.1.2 | 25 / 69 | 69 / 80 | 11 |
| 26.2 | 25 / 69 | 69 / 80 | 11 |
| 26.3 | 25 / 69 | 66 / 80 | 14 |

These counts cover **only the 80 critical JNI declarations** currently emitted by
`tests/minecraft_bindings_dump.cpp` for the 1.21.11 target. They are not a full SDK,
Java bridge, loader, renderer, behavioral or gameplay audit. An exact signature
match does not imply unchanged semantics. A missing baseline path may mean a
moved class rather than removed functionality. No 26.x game or Windows DLL was run.

26.x jars use Mojang names (for example `net/minecraft/client/Minecraft`), not the
old intermediary `net/minecraft/class_310` namespace. Fabric's Yarn API returns no
26.1 Yarn version, and the intermediary API returns the identity version `0.0.0`.
Do not reuse 1.21.11 names or generate a fake 26.x Yarn profile.

## Earlier discovery findings

- **Namespace:** migrate all active class/member descriptors, including raw JNI
  lookups and Java reflection/ASM targets outside the SDK. Do not change global
  JNI resolution for the existing 1.21.x targets as a shortcut.
- **Inventory:** `MultiPlayerGameMode.handleInventoryMouseClick(..., ClickType,
  Player)` no longer matches. The 26.1 jar declares `handleContainerInput(...,
  ContainerInput, Player)`. Verify enum meanings, packet behavior and every
  inventory module before replacing this call; a name substitution is insufficient.
- **FOV:** the old `GameRenderer.getFov(Camera, float, boolean)` is absent. The
  26.1 camera exposes `getFov()` and additional FOV update/calculation methods.
  Verify timing and projection semantics before adapting native overlays.
- **Item models:** `BakedQuad` moved under `client/resources/model/geometry` and
  `ItemTransform` under `client/resources/model/cuboid`. The layer's old
  `transform` field no longer matches; `itemTransform` and `localTransform` exist.
  Tint storage is `IntList`, not `int[]`. Preserve existing icon projection,
  shading and transforms while adapting extraction.
- **26.3 rendering:** texture types move from `com/mojang/blaze3d` to
  `com/mojang/renderpearl`; the old GL class path no longer matches. Layer quads
  use `ItemQuads` rather than the earlier list preparation method. Audit the
  render backend, OpenGL hook and GPU reads separately; library presence alone
  does not establish that a backend is selected or compatible.
- **Bridges:** audit Sky callbacks, fog data and ASM
  classfile-69 handling against actual 26.x Fabric dependencies. Existing
  1.21.x transformer counts are not proof of 26.x compatibility.
- **Runtime:** Java 25 is required by the official releases. This does not mean
  gameplay logic must move from C++ into Java. Validate native attach/JVMTI,
  classloader lifecycle and unload/reload on that runtime.

## Reproduce the audit

Requires Python 3.11+ and a C++20 compiler, but no running JVM or Minecraft.
Use an external cache/report path, not production resources:

```sh
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --download --output /path/to/26-readiness.json
# Later: same verified fixtures, no network access
python scripts/Test-Minecraft26Readiness.py --cache-dir /path/to/cache --output /path/to/26-readiness.json
# Synthetic regression tests, with no fixtures or network
python -m unittest discover -s tests -p test_minecraft26_readiness.py -v
```

`--target 26.1` restricts the audit; repeat the option for additional exact releases.
The tool compiles the **existing 1.21.11** binding dump with owner-qualified names,
joins Fabric intermediary and Mojang ProGuard mappings, verifies each source
binding against the actual baseline class hierarchy, and compares named members
against the target jar's classes, superclasses and interfaces. Constructors are
not inherited; method/field kinds, descriptors and static flags are checked.
Unresolved or ambiguous baseline mappings fail the audit, not silently disappear.
Possible class relocations are hints only, never automatic runtime mappings.

Exit zero means the report was successfully generated, **not that a 26.x target
is ready**. JSON explicitly includes `supported: false`. No build targets, DLLs,
launcher manifests or remote resources are generated or changed by this tool.

## Current acceptance boundary

The isolated 26.1 name table, native SDK/raw-JNI adaptation, inventory/FOV/item
contracts and version-scoped bridges above were implemented and audited.
Exact build targets were enabled only after those source checks. This is not
proof of Windows link or live game behavior.

The remaining checklist is the [shared per-target acceptance checklist](MINECRAFT_TARGETS.md#required-game-checks-before-claiming-full-support).
26.2/26.3 remain discovery-only; audit their full SDK/bridge/render contracts
before enabling any target, especially the 26.3 renderpearl changes.
Do not skip modules to declare compatibility.

No deployment, Git push, auth migration or gameplay-to-Java rewrite was part
of this dated native-port audit. Current security/protocol requirements are
documented separately in launcher v2.
