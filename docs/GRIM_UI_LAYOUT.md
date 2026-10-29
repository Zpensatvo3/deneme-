# GrimUI, HUD ve Keybinds

Güncel giriş `src/gui/gui.cpp` → `gui::grim::render` →
`src/ui/grim_ui.cpp` şeklindedir. `src/gui/` platform/render altyapısını ve ortak
kontrolleri; `src/ui/` uygulama sayfalarını yönetir. İki dizin de aktiftir.

## Menü ve modül kartları

- Features ve Other aynı ana pencereyi kullanır; normal boyut 960×620'dir.
  Kategori/arama/HUD geçişi konumu değiştirmez. Sürükleme ekrana sınırlıdır;
  odak kaybında durur, küçültülmüş pencere konumu ezmez.
- Modüller registry sırasıyla soldan sağa iki sütuna dizilir; dar alanda tek
  sütuna geçer. Kart yüksekliği artık sarılan ad/açıklamadan hesaplanır:
  17 px başlık ve 14 px açıklama kullanılır. Eski sabit 62 px ölçüsü güncel değildir.
- Ayarı olan kart sağdaki ayrı ayar panelini seçer; ana pencere yer değiştirmez.
  Yer daralırsa panel ana pencerenin sağ tarafına bindirilir.
  Oyuncu önizlemesi yalnızca yeterli alan varsa gösterilir.
- Kart üzerinde dişli veya toggle yoktur. Enabled, ayar panelinde Keybind'in
  üstündedir ve tam satır hover zemini çizmez. Etkin ad tema rengini kullanır;
  seçili kart çerçevesi Reduce Motion kapalıysa animasyonludur.
- Full Bright gibi keybind dışında kontrolü olmayan kartlar doğrudan açılır/
  kapanır. No Jump Delay'in açıklama-only panelleri de bu kapsamdadır.
  Click GUI/Friends Manager'ın özel kontrolleri korunur. Self Destruct anlık unload'dur.
  Arama/Enter ile seçim aynı davranışı uygular.
- Ayar panelinin hit-test alanı ayrıdır; modüle özgü child ID'si kaydırma
  konumunu ayırır. Kart/metin/filtre ölçümleri uygun anahtarla önbelleklenir;
  görünmeyen kartlar çizilmez. Modül ID/kategori/config alanları değiştirilmez.
- Arayüz İngilizcedir. Eski `language` tercihi yok sayılır; Unicode kullanıcı
  metni korunur. Listeler çubuksuz kayar; taşan native popup'larda geçerli
  scrollbar ölçüsü tutulması gerekir.

## Arama, profil ve giriş sahipliği

Arama sol altta profilin üstündedir. Yarı saydam kutuya doğrudan yazılır;
ayrı sonuç paneli açılmaz. Oklar/Enter, otomatik tamamlama ve X ile temizleme
korunur. Tamamlama odak kaybında iptal edilir. Profilde yerel skin kafası ve adı,
doğrulanmış lisans süresi veya `Lifetime` gösterilir; bilinmeyen süre lifetime
sayılmaz. Bu etiket oturum yetkisi sağlamaz.

ESC önce açık arama/popup/onayı kapatır; sonra ana menüyü kapatır.
Eski keybind yakalayıcısında ESC atamayı siler, menüyü kapatmaz.
HUD yerleştirmede ESC menüye döner. Kapatma ESC'sinin tekrarı oyuna iletilmez.

Menü açılışında vanilla tuş/kuyruk durumu nötrlenir, fare kilidi bırakılır.
Açık menü oyun girdisini engeller; fiziksel tuşlara sentetik bırakma gönderilmez.
Kapanışta kilit yalnızca aynı dünya/oyuncu etkinse, oyun odaktaysa ve başka
Minecraft ekranı yoksa geri alınır. Alt+Tab/Windows pencereye iletilir;
arka plan girdileri ve dönüşte önceden basılı tuşlar yeni basış sayılmaz.

## Other sayfaları

| Sayfa | Kapsam |
| --- | --- |
| Friends | Mevcut orta tık ayarı, Name/Server players seçimi, ekleme/çıkarma; arkadaş listesi oturumluk kalır. Yarıçap/ESP override eklenmedi. |
| Configs | İsim yazılınca aşağı açılan oluşturma alanı; My Configs/Public Configs/Favourites, yükle/güncelle/sil ve onaylar. İndirme/modül sayacı yoktur. |
| Themes | Sekiz vurgu paleti, klasik/hazır/özel menü arka planları; yoğunluk ve karartma. |
| Hud | Watermark/ArrayList Scale, sabit Spotify ölçeği ve tek Edit girişi; modüllerin diğer ayarları mevcut kategoridedir. |
| Keybinds | Etkileşimli ANSI klavye ve gerçek modül atamaları. |

HUD yerleştirme Grid/Snap, Alt ile serbest sürükleme, oklar/Shift, Ctrl+Z,
Undo/Reset ve yerel kaydı korur. Kontroller dar alanda küçülmek yerine alt satıra geçer.
Spotify logosu kullanıcı PNG'sinin özgün saydamlığını/renklerini korur;
eski SVG/mesh araçları kullanılmayan kaynak referanslarıdır, bu temizlikte silinmez.

Menü arka planları: Classic, Citadel, Candy Dusk, Nebula, Aurora ve Custom image.
PNG/JPG içe aktarma 16 MB, kenar başına 8192 ve toplam 16 megapiksel ile sınırlıdır.
Görüntü oranı korunarak ortadan kırpılır. Yerel kopya
`%LOCALAPPDATA%/onestep/menu-background.image` içindedir; kaynak dosya değişmez.
Seçim UI tercihlerine aittir, public config'e eklenmez. Kayıp görselde klasik
zemin kullanılır. Disk yazımı son değişiklikten sonra bekler; hata her karede denenmez.

## Keybinds

104 gerçek ImGui hit target'ı vardır: F1–F12, ANSI ana blok, navigation/arrows ve
ayrı numpad. Geometri `platform/input/key_codes.hpp`, okunabilir legend/ölçü
`ui/keyboard_layout.hpp` içindedir. Dar pencerede en az 29 birimlik tuşlar
korunur; yalnızca klavye yatay kayar. Harf/rakam 15 px, diğer etiketler 12.5 px;
tam ad tooltip'te görünür. Statik klavye resmi kullanılmaz.

Tuş seçilince altında aranabilir/kaydırılabilir modül listesi açılır.
Modüller registry'den dinamik gelir; sunum kategorileri menüyle ortaktır:
Movement/Player/World, Misc'te birleşir. Friends yönetim grubu ve sabit Right
Shift kullanan Click GUI atama listesinde yoktur. Ek ayarı olmayan modüller
desteklenir; ayar sayısı keybind yeteneğini belirlemez.

Satıra tıklamak atar/taşır; aynı tuştaki satıra tekrar tıklamak kaldırır.
X mevcut mouse/legacy bağını da temizler. Seçili/atanmış/hover durumları tema
rengiyle ayrılır; çoklu atamalar başlıkta ve satırlarda görünür.
Aynı tuşun yükselen kenarı mevcut sistemde tüm atanmış modülleri tetikler;
atama işlemi modülün enabled durumunu değiştirmez. Aramanın sağındaki büyüteç
odağı aynı alana verir; kategori etiketi/oku tek zemindir. Başarı yazısı gizlidir,
kayıt hatası görünürdür. Reduce Motion tüm sayfa geçişlerine uygulanır.

Ayrılmış tuşlar seçilip incelenebilir ama yeni atama alamaz:

- ESC: yakalayıcı temizleme veya menü kapatma.
- Right Shift: sabit menü kısayolu.
- End: acil unload; Windows tuşları: işletim sistemi.

Eski/import edilmiş ayrılmış atamalar zorla göç ettirilmez; temizlenebilir.

## Keybind uyumluluğu ve kayıt

Tüm yazmalar mevcut `modules::module::set_keybind` alanına gider;
`module_loop` / `key_edge` tek tetikleme motorudur. Atomic değer ve revision
yalnızca eşzamanlı örnekleme/kaydetmeye hizmet eder. Menüde yazmak, kapatırken
basılı tutmak veya basılı tuşu yeniden atamak fazladan tetikleme üretmemelidir.

Eski Windows VK 0–255 değerleri değişmez; **269 (0x10d)** numpad Enter'ı
E0 1C ile ana Enter 1C'den ayıran tek ek değerdir. NumLock kapalı keypad ile
navigation scan'leri de ayrılır; numpad 1, End unload'u üretmez.
Etiketler ANSI US yerleşimidir; otomatik OS dil algılama değildir.
Aynı adlandırma eski yakalayıcı ve HUD keybind etiketlerinde kullanılır.

Public config'in `modules/id/enabled/keybind/settings` biçimi korunur.
**Eski DLL'ler 269 içeren config'i reddedebilir**; eski build'e paylaşmadan
önce bu bağı kaldır veya güncel DLL kullan. Yerel binding-only export/apply aynı
JSON parser ve stable ID'leri kullanır; gameplay ayarı veya enabled yüklemez.

Yerel profil `%LOCALAPPDATA%/onestep/keybinds.json` içindedir ve DLL hedefleri
arasında ortaktır. Registry sonrası, hook/tick öncesi yüklenir. Bilinmeyen ID
atlanır; geçersiz/tekrarlı kayıt tüm yüklemeyi değişiklik yapmadan reddeder.
Bozuk dosya yeni bir kullanıcı değişikliği olana kadar ezilmez.
Yeni sayfa hemen, eski yakalayıcı/public import 400 ms debounce ve shutdown flush
ile yazar. Temp dosyanın atomic değişimi başarısızsa eski profil korunur.
Hata toast/metinle bildirilir; cloud yüklemesi veya ikinci dispatch sistemi yoktur.

## Temizlik ve uyumluluk

Çağrı/başvuru denetiminde kullanılmayan eski stepper, coordinate row, status badge,
module-card çizici zinciri ve okunmayan geometry-reset flag'i kaldırılmıştır.
Config/HUD forwarding wrapper'ları tek çağrı noktasına indirildi.
Historical preference alanları parser/serialization kullandığı için korunur;
Widget uyumluluk girişleri `ImGui::OneStep*`, tercih servisi `gui::ui_state`
olarak adlandırılır. Eski kullanıcı klasörü ve `##ThedoriusGrimUI` pencere kimliği
uyumluluk için korunur. Font/texture sahipliği, authentication kapıları ve
mevcut platform hooks değişmez.
Kapanış GrimUI ve ortak widget animasyon önbelleklerini ImGui'den önce temizler.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-GrimUI.py --preview-dir out/ui-preview
git diff --check
```

Gerçek ImGui testi kartlar/ayarlar, HUD kontrolleri, keybind atama/taşıma/silme,
104 tuşun hit target'ları, shared key, arama/popup/ESC, çubuksuz kaydırma,
yerel dosya yeniden yükleme/bozulma/I/O hatası ve dar viewport'ları denetler.
Oyun, auth ve platform bağımlılıkları test karşılıklarıdır; gerçek oyuna bağlanmaz.
Tercih parser'ı gerçek kullanıcı dosyasına erişmeden ayrıca test edilir.

Önizlemeler gerçek ImGui üçgenleri/font atlasından 2× yoğunlukla rasterize edilir;
eski görsel büyütülüp keskinleştirilmez. Pillow yalnızca opsiyonel PNG export içindir.
Çıktılar ignored `out/` altında kalır; uygulamaya bağımlılık eklenmez.
**Bunlar oyun içi ekran görüntüsü değildir.** Windows linking, Win32 giriş
teslimi, OpenGL, dosya seçicisi ve Minecraft davranışı ayrıca doğrulanmalıdır.
