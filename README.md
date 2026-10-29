# OneStep

Minecraft Fabric için Windows x64 üzerinde çalışan C++20 istemci. OpenGL/ImGui
arayüzü, modüller, HUD düzenleyicisi ve sürüme özel Java köprüleri içerir.

Auth ve launcher kaynakları kaldırılmıştır. Public config, ayrı Cloudflare
Worker ve D1 servisine bağlanır. Yeni Windows istemci bağlantısı henüz DLL
derlemesi ve oyun içi test ile doğrulanmamıştır.

## Derleme

Windows x64, Visual Studio C++ Build Tools, CMake + Ninja, Windows SDK ve PowerShell
gereklidir. Köprüler yeniden üretilirken JDK da gerekir. Sürümler ve diğer
gereksinimler [hedefler rehberinde](docs/MINECRAFT_TARGETS.md) açıklanır.

Komutları x64 Developer PowerShell içinde çalıştırın. Varsayılan artımlı
derleme ve önbellek davranışı [derleme rehberinde](docs/INCREMENTAL_BUILD.md) açıklanır.

```powershell
./scripts/Build-Dev.ps1
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./Build-Test.ps1 -MinecraftVersion 1.21.11
```

Her DLL yalnızca derlendiği tam Minecraft Fabric sürümünü kabul eder. Kaynak
testlerinin geçmesi, Windows derlemesinin veya oyun içi davranışın doğrulandığı
anlamına gelmez.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-HybridBridge.py
git diff --check
```

## Belgeler

- [Minecraft sürümleri ve derleme](docs/MINECRAFT_TARGETS.md)
- [Modüller](docs/MODULES.md)
- [Menü ve HUD](docs/GRIM_UI_LAYOUT.md)
- [Java köprüleri](docs/HYBRID_MIGRATION.md)
- [Bakım](docs/MAINTENANCE.md)
- [Üçüncü taraf bildirimleri](THIRD_PARTY_NOTICES.md)
