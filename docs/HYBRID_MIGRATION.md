# C++ / Java köprüleri

## Güncel kapsam

Katalog 36 modüldür. Beş oyun I/O köprüsü yalnızca halen kayıtlı özellikleri içerir:

| Köprü | Özellikler |
| --- | --- |
| GameBridge | No Jump Delay |
| UtilityBridge | Fast Place, Sprint, Full Bright |
| ItemBridge | Fast EXP |
| InventoryBridge | Pot Refill, Mouse Tweaks |
| WorldBridge | Pearl Macro, Triggerbot, Jump Reset, Auto Totem |

İlk dört köprünün hibrit I/O yolu 1.21.11'e özeldir; diğer hedeflerin mevcut
native yolları korunur. WorldBridge denetleyicileri desteklenen tam sürümler için
açık eşlemeler kullanır: 1.21.11, 26.1, 26.1.1 ve 26.1.2. Bu tablo oyun içi
doğrulama iddiası değildir.

## Yaşam döngüsü

C++ ayarları, odak/menü kapısını ve iptal nesillerini yönetir. Java oyun
nesnelerini client thread'inde okur ve değiştirir. Snapshot işlemleri
player/world kimliğini, manuel slot değişimlerini ve tek kullanımlık işlem
sahipliğini yeniden doğrular. Başarısız bir yazma aynı karede tekrar oynatılmaz.
Kapanışta yalnızca modülün sahip olduğu durum temizlenir.

## Jump Reset: Java denetleyicisi

Hasar, zemin, şans ve gecikme kontrolü WorldBridge'dedir. Gerçek GLFW girdisi
okunur; zıplama vanilla KeyBinding üzerinden uygulanır. İptal nesli ve binding
sahipliği, manuel girdinin eski bir cleanup tarafından ezilmesini önler.

## Auto Totem ve ortak tick kancası

ClientTickRelay yalnızca tick bildirimi taşır. ClientTickTransformer gerçek
client tick HEAD'ine bu bildirimi ekler; item-use girişine müdahale etmez.
Aynı transformer Auto Totem'in yerel, tıklanamayan envanter ekranını üretir.
Eski bir sahip aktif callback'i kaldıramaz; shutdown kendi JVMTI dönüşümünü geri alır.

## Derleme ve doğrulama

Normal sınıflar Build-GameBridge.ps1 ve CMake/Ninja hedefinde kaynak değişince
yenilenir. ASM kullanan tick transformer değiştiğinde gömülü sınıfı ayrıca üret:

```powershell
.\scripts\Build-ClientTickHooks.ps1 -AsmClasspath 'C:\libs\asm.jar;C:\libs\asm-tree.jar'
```

```sh
python scripts/Test-HybridBridge.py
python scripts/Test-ClientTickHooks.py --asm-classpath '<ASM ve ASM Tree classpath>'
python scripts/Test-AutoTotem.py --asm-classpath '<ASM ve ASM Tree classpath>'
python scripts/Check-GameBridgeContracts.py
```

Sözleşme kontrolü native JNI imzalarını derlenmiş Java metotlarıyla karşılaştırır.
Kaynak ve gömülü sınıf senaryoları ayrı çalışır; tick bytecode'u JVM verifier ile
doğrulanır. Bunlar Windows MSVC DLL derlemesi veya gerçek oyun testi değildir.
