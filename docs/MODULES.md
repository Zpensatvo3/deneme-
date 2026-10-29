# Modül ve config notları

Bu belge README'deki tekrarlanan özellik kayıtlarını tek yerde toplar.
Tam kayıt listesi ve stable ID'ler için kaynak otoritesi
`src/modules/module_catalog.cpp` ile `src/modules/settings_manifest.json`'dır.
Mevcut katalog 36 modül içerir; bu sayı oyun içi kabul testi sayısı değildir.
Arayüz gruplaması [UI rehberinde](GRIM_UI_LAYOUT.md), Java I/O kapsamı
[hibrit geçiş belgesinde](HYBRID_MIGRATION.md) açıklanır.

## Ayarlar ve config sözleşmesi

- `core/setting_schema.hpp` bool, tamsayı, sayısal değer, aralık, renk, metin
  listesi ve seyrek enum seçeneklerini tanımlar; `ui/settings_renderer.hpp`
  aynı alanları mevcut kontrollerle çizer.
- Combat/utility/visual ayarları kilitli snapshot depolarını kullanır. Vape/Chams
  mevcut kilitlerini, Jump Reset revision/iptal sahipliğini, Aim/Trigger
  kendi runtime sınırlarını korur. HUD/ArrayList tercihleri render katmanındadır.
- Friends Manager, Click GUI, Self Destruct, Full Bright, No Jump Delay'in
  modüle özel config alanı yoktur. Arkadaş listesi ve UI tercihleri ayrı sahiptedir.
- Yeni public config kökü `modules`; her kayıt `id`, `enabled`, `keybind`,
  `settings` taşır. Eski sürüm etiketi okunur, yeni çıktıya yazılmaz.
- Sınırlar: 96 KB, en fazla 128 modül, JSON derinliği 32. Tam belge önce
  doğrulanır; yanlış tür, yinelenen alan/ID, bozuk JSON, geçersiz enum/tuş
  reddedilir. Kaldırılmış modüller atlanır, gizli alanlar korunur.
- Beklenmeyen uygulama hatasında eski ayarlar geri yüklenmeye çalışılır;
  enabled/keybind en son yayınlanır. Bu, worker'lara karşı modüller arası
  tek bir atomik transaction değildir.
- UI/import sınırları her zaman aynı olmak zorunda değildir. Radar'ın geniş ekran konumları ve özel Block ESP/Spotify/Jump Reset
  panelleri korunur. Numpad Enter'ın genişletilmiş kodu için
  [keybind uyumluluğunu](GRIM_UI_LAYOUT.md#keybind-uyumluluğu-ve-kayıt) incele.

## Birleştirilmiş modüller

### Auto Totem

Auto Totem’in Java akışı Argon’daki Auto Inventory Totem davranışını izler:
Blatant ilk uygun ana envanter totemini, Random rastgele uygun totemi seçer.
Önce offhand doldurulur; Hotbar açıksa eldeki slotun boş olması veya Force Totem
kullanılması durumunda seçili slot da doldurulur. Auto Switch seçilen totem slotuna
geçer. Auto Open modülün açtığı envanteri, Stay Open For süresi bitince kapatır.
Tüm kararlar ve SWAP işlemleri oyun thread’inde Java’dan yapılır.

## Hedefleme ve oyun I/O

- **Anti Bot:** yeni kullanımda açık, kaydedilmiş false korunur. İlk 40 tick
  bekler; ardından 40 tick kayıtsız kalan oyuncuyu ortak hedef/görsel filtresinden
  çıkarır. Görünmezlik, isim veya hareketsizlik tek başına kanıt değildir.
  Tab listesi güvenilmezse elemez; dünya/sunucu değişiminde geçmiş temizlenir.
- **Triggerbot:** hedef/silah ve kritik kontrolleri, rastgele cooldown/reaction,
  güncel ray sorgusu ve vanilla saldırı Java WorldBridge'dedir. Arkadaş/bot
  filtreleri ile Shield Breaker hazırlıkları ortak native servislere
  bağlanır. On Click Only, Java tarafında GLFW ile sol tuşu okur. Varsayılan
  açık Swing Hand seçeneği el sallamayı yönetir. Click Simulation (varsayılan
  kapalı), saldırı sonrası oyunun mouse callback'ine bir basma/bırakma çifti
  yollar; eklenen vanilla press kuyruğunu temizler. Eski config anahtarları korunur. Menü, odak, oturum,
  manuel slot veya ayar değişimi döngüyü iptal eder; Windows click üretilmez.
- **Vape:** yalnızca Aim Assist (Simple/Adaptive).
- **Jump Reset:** hasar/zemin kontrolleri, şans/gecikme, Living Attacks Only
  onayı ve jump binding sahipliği WorldBridge Java denetleyicisindedir.
  Tuşlar Java'da GLFW ile okunur; basma/bırakma vanilla KeyBinding yolundadır.
  Game Binding ve Game Space modları mevcut config/preset'leri korur;
  eski System Space kayıtları Windows input üretmeden Game Space olarak çalışır.
  [Java geçiş kapsamı](HYBRID_MIGRATION.md#jump-reset-java-denetleyicisi).
- **No Jump Delay:** tekrar zıplama cooldown'unu kaldırır; jump basışı üretmez.
- **Pot Refill:** boş hotbar slotlarına Healing I/II taşır; slot sırası ve gecikme
  ayarlanır, eski `delay_ms` korunur. Creative'de çalışmaz. Kaynak/hedef/adet/
  bileşenler her işlemde okunur; 1,5 saniye sonuç yoksa tekrar tıklamak yerine
  envanter oturumu yenilenene kadar durur. Manuel işlem ve bağlam değişimi iptal eder.

1.21.11'de bazı I/O yolları Java'ya taşınmıştır; hesaplama, giriş sahipliği ve
diğer sürümlerin native yolları için [grup sözleşmeleri](HYBRID_MIGRATION.md)
esas alınır. Kaynak notları sunucu kabulü veya tüm modüllerin oyun testi değildir.

## Görseller ve yerel sınırlar

### ESP, Chams ve hedef bilgisi

ESP/Chams/Storage ESP/Block ESP için ayrı mesafe seçimi yoktur.
Yalnızca oyunun yüklediği/verdiği nesneler görülebilir. Block ESP'nin 10.000
işaret sınırı korunur; tarama bütçelidir. İkon seçicisi görünür blok item
modellerini kare başına iki okuma bütçesiyle hazırlar; item'i olmayan blok küple gösterilir.
Block ESP seçilen blok hücrelerini dünya koordinatlarına oturan 3D kutularla
gösterir: 12 kenar ve altı yarı saydam yüzey kullanılır; dolgu sıfırsa yalnız
kenarlar çizilir. Kutu ve yüzeyler uzaktan yakına sıralanır, kamera yakın düzlemi
ve ekran sınırlarında kesilir. Çizgi kalınlığı ve dolgu ayarları geçerlidir.
Görüş dışındaki kutular çizilmez; ek bir mesafe veya minimum piksel boyutu sınırı yoktur.
Item ESP yüklenmiş düşen eşyaların kutu/isim/adet/mesafesini gösterir.
Dünya/oyuncu kimliğine bağlı overlay snapshot'ları geçersiz oturumda temizlenir;
önceki worker sonuçları yeni sunucunun üstüne yazılamaz.

### Kozmetikler ve efektler

China Hat, Wings ve Cape yalnızca yerel üçüncü şahıs kozmetikleridir; diğer
oyunculara gönderilmez. Gövde/blok maskesi tam uzuv veya kısmi/şeffaf blok
simülasyonu değildir; desteklenmeyen pozlarda çizim durur.
Wings'in Dragon/Angel/Butterfly görselleri ve Cape'in Phoenix/Astral stilleri
gömülüdür. Eski Cape indeksleri 0–7 korunur. Tint Artwork açık değilse özgün
renkler kullanılır. Kumaş 9×13 parçacık, kanatlar sönümlü eklemlerle çalışır;
120 Hz adım ve kare başına altı adım bütçesi vardır. Teleport/oturum/disable
sıfırlar; Reduce Motion/Animation kapalıyken sabit poz kullanılır.
[Asset kökeni ve üretim istemleri](../assets/cosmetics/README.md).

Hit Particles en fazla 96, Jump Circles en fazla 8 efekt tutar. Hasar yerel
saldırıya yakın gözlenmelidir; bunlar sunucu onayı değildir ve saldırı göndermez.

Trails en fazla 96 parçacık / 48 ayak izi tutar. Slab/merdiven yüzeyleri vanilla
collider raycast'inden gelir. Görünürlük kare başına 12 ray ve yaklaşık 200 ms
önbellekle bütçelidir; düşük FPS'de yenileme uzayabilir. Uzun karelerde toplu
emission yoktur. Oturum/teleport/disable izleri siler.

### Sky

Sky renk ve gömülü panorama seçenekleri sunar; saat/hava/ışık vanilla kalır.
Sis, güncel terrain fog rengi/mesafesiyle eşleşir; alt yarı küre gerçek sis
rengini kullanır, doku satırı uzatılmaz. Sarma/zenith yumuşatma ve periyodik
türevler seam/mipmap çizgilerini azaltır. Aktif Iris shader paketi, ışığı olmayan
boyutlar veya su/lav/kar içindeki kamera panorama yolunu durdurur.
Başka modun sonradan değiştirdiği renk eski kayıtla ezilmez.
Fabric olayı/GLSL/JNI farkları [sürüm denetiminde](MINECRAFT_26_PORT.md) belgelenir.
Kapanışta listener pasifleştirilir; Fabric kayıt silme API'si olmayan yolda pasif
listener kalabilir. Sky bytecode'u kaynak değişince `scripts/Build-SkyBridge.ps1` ile yenilenir.
[Panorama kaynak ve indeksleri](../assets/skies/README.md).

Kozmetik/efekt/overlay ayrıntıları için oyun içi görünüm, shader/resource-pack
uyumluluğu ve FPS ayrıca test edilmelidir. Eski bir DLL veya yalnızca offline
kontrol için verilmiş başarı kaydı yeni kaynak ağacının tamamını doğrulamaz.
