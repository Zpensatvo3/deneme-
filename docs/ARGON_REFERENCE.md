# Argon kaynak referansı

OneStep'in Triggerbot, Jump Reset ve Auto Totem modüllerinin bağımsız Java uygulamalarında incelenen referans:

- Depo: [LvStrnggg/Argon](https://github.com/LvStrnggg/Argon)
- Kaynak kodu: Java, Fabric
- Minecraft hedefi: `1.21` (1.21.1 değil)
- Durum: Depo sahibi tarafından arşivlenmiş; GitHub üzerinde salt okunur.

## Lisans ve kullanım

Argon deposundaki [LICENSE](https://github.com/LvStrnggg/Argon/blob/main/LICENSE)
dosyası tüm hakları saklı tutuyor. README değişiklik ve yeniden yayımlamaya izin
veren ifadeler içeriyor; ancak bu izin lisans dosyasında açıkça tanımlanmıyor.
Bu nedenle Argon'un kaynak kodu veya derlenmiş istemcisi OneStep'e kopyalanıp
dağıtılmamalı. Kodun projeye alınması için hak sahibinden açık izin veya net bir
lisans alınmalı.

Argon yalnızca davranış referansı olarak kullanıldı; kaynak kodu kopyalanmadı.
OneStep'in denetleyicisi `src/bridge/WorldBridge.java` içindedir. Argon'un `1.21`
kaynağı OneStep'in hedef sürümleri için doğrudan uyumlu kabul edilmez.

## Triggerbot için incelenen akış

`738e75f0623581ffbbc116656835352f8df9eb22` commit'indeki [TriggerBot](https://github.com/LvStrnggg/Argon/blob/738e75f0623581ffbbc116656835352f8df9eb22/src/main/java/dev/lvstrng/argon/module/modules/combat/TriggerBot.java)
ve [WorldUtils.hitEntity](https://github.com/LvStrnggg/Argon/blob/738e75f0623581ffbbc116656835352f8df9eb22/src/main/java/dev/lvstrng/argon/utils/WorldUtils.java)
incelendi. Referans davranışları: nişangâhtaki entity, arkadaş/eşya kullanımı
filtreleri, isteğe bağlı fiziksel sol tuş, silah/kritik/kalkan kontrolleri,
döngü başına örneklenen gecikme ve vanilla attackEntity + el sallama.
Kod kopyalanmadı; ilk geçişte Argon'un ek seçenekleri OneStep'e eklenmedi.

OneStep mevcut `cooldown`, `reaction`, `targets`, `weapon`, `criticals` ve
`on_click` sözleşmesini korur; Argon'un silaha göre sabit ms gecikmeleriyle
ayarları değiştirmez. Kararlar, rastgelelik, kritik bekleme, güncel vanilla
ray sorgusu ve saldırı WorldBridge'in bağımsız 4 numaralı Java özelliğindedir.
Paylaşılan arkadaş/Anti Bot servisi, saldırı sahipliği ve Shield Breaker'ın
kendi hazırlama/geri dönüş işlemleri native servis callback'i üzerinden kullanılır;
bu diğer modüllerin Java'ya taşındığı anlamına gelmez. Windows tıklama üretimi yoktur.

### GLFW girişi ve Swing Hand

Triggerbot'un `On Click Only` seçeneği sol tuşu Java'da, oyunun güncel window
handle'ı üzerinden `GLFW.glfwGetMouseButton(handle, 0)` ile okur. Argon'daki gibi
`GLFW_PRESS` gerekir; kontrol döngü başında ve saldırıdan hemen önce yapılır.
Native odak/izin ve manuel saldırı nesli denetimleri devam eder. GLFW tuş durumu
okunur; sentetik mouse olayı üretilmez.

Yeni `swing_hand` / **Swing Hand** ayarı varsayılan açıktır. Kapalıyken ek
`player.swingHand(MAIN_HAND)` çağrısı yapılmaz; cooldown/reaction ve hedef
kontrolleri aynı kalır. Önceki config'lerde alan bulunmaması geçerlidir.

### Click Simulation

`click_simulation` / **Click Simulation** varsayılan false'tur. Argon'un aynı
commit'teki [MouseSimulation](https://github.com/LvStrnggg/Argon/blob/738e75f0623581ffbbc116656835352f8df9eb22/src/main/java/dev/lvstrng/argon/utils/MouseSimulation.java)
callback yolu davranış referansıdır; kod kopyalanmadı. Başarılı doğrudan saldırı
çağrısından sonra Java client thread'inde oyunun Mouse callback'ine bir sol tuş
PRESS/RELEASE çifti gönderilir. CPS dinleyicilerinin bu yolu izlemesi amaçlanır;
her üçüncü taraf HUD'un bunu sayacağı oyun içinde doğrulanmadı.

OneStep pulse'u aynı çağrıda bitirir; Argon'un thread havuzu ve 35 ms uyutması
alınmadı. Bekleyen release işi, Windows SendInput veya ikinci doğrudan saldırı
üretilmez. Sol mouse'a bağlı bütün KeyBinding'lerin önceki pressed/timesPressed
halleri saklanır: callback'in eklediği vanilla press kuyruğu kaldırılır, mevcut
manuel kuyruk ve fiziksel basılı hal korunur. Pencere/oturum/odak değişmişse
basılı durum yeniden yaratılmaz. Callback istisnasında release/temizlik denenir;
modül başka backend üzerinden tekrar saldırmaz.

KeyBinding `KEYS_BY_ID`, `pressed` ve `timesPressed` adları
[Yarn 1.21.11 API](https://maven.fabricmc.net/docs/yarn-1.21.11+build.6/net/minecraft/client/option/KeyBinding.html)
ve Mojang 1.21.11 client mappings ile kontrol edildi. 26.1 ailesi mevcut SDK'nın
MouseButtonInfo sözleşmesini ve exact Mojang adlarını kullanır; bu kaynak
kontrolü yedi hedef için oyun içi kabul değildir.

## Jump Reset için incelenen akış

Aynı sabit commit'teki [AutoJumpReset](https://github.com/LvStrnggg/Argon/blob/738e75f0623581ffbbc116656835352f8df9eb22/src/main/java/dev/lvstrng/argon/module/modules/combat/AutoJumpReset.java)
incelendi. Menü/eşya kullanımı sırasında bekleme, hasar animasyonu, yerde olma
ve aktivasyon şansı davranış referansıdır. Argon `hurtTime == 9` üzerinde doğrudan
`jump()` çağırır ve iki şans kontrolü yapar. OneStep'in bağımsız Java uygulaması
mevcut tek hasar kenarı/tek şans örneklemesi, milisaniye gecikmesi, living-source
onayı ve yapılandırılmış jump binding sözleşmesini korur; Argon kodu kopyalanmadı.

WorldBridge özelliği 5 oyun okumalarını, kararları, zamanlamayı ve tuş
sahipliğini yönetir. `mode`, `chance`, `jump_delay_ms`, `only_combat` alanları
ve preset'ler korunur. Tuş/mouse durumu güncel oyun penceresi üzerinden Java'da
`GLFW.glfwGetKey` / `glfwGetMouseButton` ile okunur; SCANCODE atamaları
`glfwGetKeyScancode` ile eşlenir. Basma ve bırakma oyunun `KeyBinding.setPressed`
yolundadır. GLFW fiziksel durumu değiştirilmez.

Eski `mode: 1` / System Space kayıtları **Game Space** olarak açılır: jump tuşu
Space'e atanmışsa Java aynı game-binding yolunu kullanır. Windows SendInput,
native Space ticket'ı veya sonradan teslim edilecek input olayı yoktur. Native
callback yalnızca runtime izin/odak ve iptal neslini denetler. Argon 1.21
kaynakları diğer hedeflere birebir taşınmış kabul edilmez.


## Auto Totem için incelenen akış

Aynı sabit commit'teki `AutoInventoryTotem`, `InventoryUtils` ve `FakeInvScreen`
incelendi. Kaynak yalnızca 9–35 aralığındaki ana envanterden seçilir: Blatant
ilk uygun slotu, Random uygun slotlardan rastgele birini seçer. Önce offhand,
sonra Hotbar açıksa boş/Force Totem ile değiştirilebilen seçili slot doldurulur.
Auto Switch kapalıysa hedef mevcut seçili slottur; Totem Slot ise otomatik ekran
kapatma koşulunda kullanılmaya devam eder. Bu ayrıntı kaynak davranışıdır.

Delay ve Stay Open For sayaçları gerçek client tick HEAD olayında ilerler.
Auto Open yerel bir InventoryScreen alt sınıfı oluşturur; Argon FakeInvScreen
ile aynı şekilde hem slot tıklama hem mouseClicked çağrıları etkisizdir.
Alt sınıf, hedef oyunun gerçek metot imzalarıyla üretilir. Kapatma sahipliği
oluşturulan ekran nesnesiyle izlenir. WorldBridge ve hook'lar bağımsız uygulanır.
Native yalnızca ayar/yetki köprüsüdür; Windows tuş veya fare üretimi yoktur.

`Test-AutoTotem.py`, gerçek Java denetleyicisini ve üretilen ekran bytecode'unu
izole JVM'de çalıştırır. Bu testler Windows DLL derlemesi veya oyun içi kabulün
yerine geçmez.
