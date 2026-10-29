# Kaynak bakımı ve temizlik denetimi

> Bu belge tarihsel bir denetim kaydıdır. Auth, launcher ve eski sunucu artık
> kaldırılmıştır; geçmişte korunduklarını belirten ifadeler bugünkü durumu anlatmaz.

Bu kayıt, `4bcb9bd` başlangıcındaki kaynak düzeni için yapılan yerel temizliği
açıklar. Çalışma öncesindeki derleme düzeltmeleri ve Markdown düzenlemeleri ayrı
başlangıç değişiklikleridir; bu denetimin kaldırdığı kod olarak sayılmazlar.

## Korunan sözleşmeler

- 61 modülün kimliği, kayıt sırası, görünen adı, kategorisi ve varsayılan ayarları.
- Config alanları, eski config okuma yolları, keybind profili ve UI tercihlerinin JSON biçimi.
- Yedi tam Minecraft hedefi ve mevcut C++/Java görev paylaşımı.
- GrimUI görünümü, kartlar, sağ ayar paneli, Themes/background, HUD, Friends ve Keybinds.
- DLL/launcher çıktı adları, API yolları, session audience'ları, HWID/device biçimleri ve resource ID'leri.
- Auth izinleri, imza doğrulaması, rate limit, Access/MFA, lisans epoch'u ve runtime lease davranışı.

Üretim kaynaklarına erişim, D1 migration, anahtar/secret değişimi, push veya deploy
yapılmadı. Cloudflare denetimi yerel testler ve Worker dry-run ile sınırlıdır.

## Kaldırılan kod ve kanıt

| İçerik | Neden güvenli? |
| --- | --- |
| Watermark içindeki `draw_glow_border`, `draw_separator`, `draw_thedorius_mark` | Anonim namespace içindeki fonksiyonların hiçbir çağrısı veya adres kullanımı yoktu. Aktif overlay yalnızca `gui::watermark_draw::draw` kullanıyor. |
| Aynı dosyadaki `color_with_alpha` | Yalnızca yukarıdaki ölü çizici zincirinden çağrılıyordu. |
| `gui::begin_surface`, `gui::end_surface` | Tüm kaynak, test ve script taramasında yalnızca bildirimleri ve tanımları vardı. Aktif sayfalar kendi ImGui child alanlarını yönetiyor. |
| Native auth içindeki `parse_json_bool` | Anonim namespace içinde tek tanım; çağrı, callback, export veya makro kullanımı yok. Aktif v2 parser/crypto yolu korunuyor. |
| `CUSTOMBASE_EXPORTS` | Dört MSBuild tanımı dışında kaynak/vendor kullanımı yoktu; dışa aktarılan sembolü veya auth yapılandırmasını yönetmiyordu. |

Bu, **7 fonksiyonun** kaldırılmasıdır. Üç aim adapter kaynağındaki gereksiz
ImGui/cache/UI redirect include'ları ve eski HUD çizicisine ait `cstdio` include'u
da temizlendi. Oyun algoritmaları değiştirilmedi.

## İsim eşleştirmeleri

| Eski ad | Yeni ad |
| --- | --- |
| `custom-base.slnx`, `.vcxproj`, `.vcxproj.filters` | `OneStep.slnx`, `.vcxproj`, `.vcxproj.filters` |
| `combat/aimassist` dosyaları ve `modules::aimassist` | `combat/legit_click_aim_assist`, `modules::legit_click_aim_assist` |
| `combat/clickaimassist` dosyaları ve `modules::clickaimassist` | `combat/aim_assist`, `modules::aim_assist` |
| `combat/blatant_clickaimassist` dosyaları ve sınıfı | `combat/blatant_click_aim_assist`, `modules::blatant_click_aim_assist` |
| `render/hud` dosyaları ve `modules::hud` | `render/watermark`, `modules::watermark` |
| `render/settings/hud_settings.hpp`, `module_settings::hud` | `render/settings/watermark_settings.hpp`, `module_settings::watermark` |
| `gui::aegis` | `gui::ui_state` |
| `ImGui::Thedorius*` iç widget girişleri | `ImGui::OneStep*` |
| `THEDORIUS_UI_NO_REDIRECT` | `ONESTEP_UI_NO_REDIRECT` |

Toplam 12 dosya yeniden adlandırıldı; bunlar işlevi silinen dosyalar değildir.
Build/test scriptleri, solution girdisi, MSBuild girdileri/filtreleri ve settings
manifesti birlikte güncellendi. Proje GUID'i ve `TargetName=Onestep` korunur.
Filtre UUID üretimindeki eski namespace de gereksiz GUID değişimi yaratmamak için tutulur.

## Belirsiz veya hâlâ aktif olduğu için korunanlar

- `src/gui/` ve `src/ui/` birlikte aktiftir: platform/render ile uygulama sayfaları ayrı sorumluluklardır.
- `ui_redirect.hpp` eski görünümlü ama çalışan widget yönlendirmesidir; yalnızca iç isimleri yenilendi.
- `mappings.hpp` ve `modules.hpp` aktif compatibility umbrella'larıdır.
- Eski UI JSON alanları ve `Thedorius Lite` kullanıcı klasörü eski tercihleri kaybetmemek için korunur.
- `##ThedoriusGrimUI` kalıcı pencere kimliği ve mevcut imzalama sertifikasının adı değiştirilmez.
- Spotify HTTP User-Agent kimliği bu kaynak temizliğinde değiştirilmez.
- Dört boş GLM dosyası upstream include sözleşmesinin parçasıdır; vendor ağacı değiştirilmez.
- Gömülü Java sınıfları ve üretilmiş SDK/Spotify verileri resource, build veya test girdisidir.
- Watermark status cache/API'si ve diğer kullanılmıyor görünen genel yardımcılar bu turda dar kapsamlı kanıtın ötesinde sökülmez.
- Windows/native ve Java test dosyaları ana offline runner'da görünmeseler bile otomatik silinmez.

## Tekrar birikmesini önleyen kontroller

`Test-Infrastructure.py`, mevcut build/SDK/settings kontrollerine ek olarak
`repository_hygiene.py` üzerinden şunları doğrular:

- İlk taraf metin dosyaları boş veya yalnızca whitespace olamaz.
- Quoted C++ include'ları mevcut include köklerinden çözülebilmelidir.
- Statik ve literal dinamik yerel JS import'larının hedefleri bulunmalıdır.
- RCDATA dosyaları ve resource ID'leri mevcut ve benzersiz olmalıdır; `.class` dahil.
- Markdown dosya bağlantıları ve başlık hedefleri geçerli olmalıdır.
- Solution ve client projelerinin literal dosya yolları bulunmalıdır.
- Tekrarlanan build girdileri set'e dönüştürülerek gizlenmez; proje ve filtreler ayrıca karşılaştırılır.

Vendor ağacı, tarihî arşiv, npm/Wrangler önbellekleri ve yerel credential
dizinleri taranmaz. Bu araç kullanılmayan kodu otomatik silmez veya bütün koşullu
derleme/çalışma yollarının erişilebilirliğini ispatladığını iddia etmez.

`test_repository_hygiene.py` pozitif/negatif geçici fixture'ları ve 61 modülün
bağımsız kimlik/sıra kaydını denetler. Bilerek modül eklerken bu kayıt da gözden
geçirilerek güncellenmelidir. Mevcut Themes/background testi gerçek Themes
uygulamasını da derleyip çizer; yalnız fixture üzerinden geçen bir kontrol değildir.

## Doğrulama

```sh
python scripts/Test-Infrastructure.py
python -m unittest discover -s tests -p 'test_repository_hygiene.py' -v
python scripts/Test-HybridBridge.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python -m unittest discover -s tests -p 'test_minecraft*.py' -v
git diff --check
```

`Test-HybridBridge.py`, Triggerbot'un gerçek Java kontrolcüsünü de kaynak ve
gömülü sınıflarla sınar. Yalnız bu kontrol için `python scripts/Test-TriggerbotBridge.py`
çalıştırılabilir. JDK 17+ ve C++ derleyicisi gerekir (Windows: MSVC Developer
PowerShell; Linux: clang++ veya g++). Geçici JNI kütüphanesi yalnız koordinatörü
taklit eder; oyuna bağlanmaz. Dört hedef kimliği için binding protokolü sınanır;
bu fixture kontrolü gerçek oyun mapping doğrulamasının yerine geçmez.

Küçük UI model kontrolleri `tests/ui_models_test.cpp` içinde toplanmıştır;
`python scripts/Test-Offline.py --test ui_models` ile çalışır. Java gameplay
binding denetimleri `scripts/Test-GameplayBindings.py --client <client.jar>`
üzerinden çalışır; 1.21.11 için ayrıca `--intermediary <mapping.jar>` verilir.

Her yedi hedef için ayrıca `Test-Offline.py --test minecraft_version --test
sdk_mappings --minecraft-version <hedef> --jobs 2`, üç 26.x hedefi için
`Test-Minecraft261Offline.py --target <hedef>` çalıştırılır.

Bu Linux ortamında MSVC yoktur; Windows DLL/launcher derlemesi ve native auth
testleri burada doğrulanamaz. Java runtime var ama `javac` bulunmadığından hibrit
köprü ve yeni JVM/JVMTI fixture derlemeleri de çalıştırılamaz. Gerçek Minecraft,
OpenGL sürücüsü ve oyun içi davranış ayrıca test edilmelidir. Windows/Java/oyun
kabulü tamamlanmadan bu kayıt tam platform doğrulaması sayılmaz.

## Bu çalışmada alınan sonuçlar

| Kontrol | Sonuç |
| --- | --- |
| `Test-Infrastructure.py` | 61 modül, 136 compile girdisi, filtre/SDK/settings/resource/link denetimleri geçti. |
| `test_repository_hygiene.py` | 8/8 geçti. |
| `Test-Offline.py --jobs 2` | Son tam turda 47/47 geçti; gerçek Themes çizimi de dahil. |
| `Test-GrimUI.py` | Gerçek ImGui/UI ve preference testleri geçti. |
| `Test-AuthConfiguration.py` | 17 derleyici senaryosu geçti. |
| AimAssist/Triggerbot/JumpReset | C++ offline ve Java denetleyici davranış testleri; native DLL derlemesi değildir. |
| Yedi hedefin `minecraft_version` ve `sdk_mappings` testleri | 14/14 geçti. |
| `test_minecraft*.py` | 21/21 geçti. |
| Üç 26.x hedefinin `Test-Minecraft261Offline.py` denetimi | Üçü de geçti. |
| Sunucu `npm test` | 19/19 geçti. |
| Sunucu `test:v2`, `test:v2:runtime`, `test:runtime` | Sırasıyla 2/2, 1/1, 1/1 geçti; yerel D1/DO/workerd kullanıldı. |
| Dashboard `test:bundle`, `test:browser` | 2/2 ve 1/1 geçti; Chromium desktop/mobile, onay ve XSS senaryoları çalıştı. |
| `npm run check --prefix server` | Wrangler 4.151.1 dry-run geçti; yükleme yapılmadı. |
| Dört yeniden adlandırılan modül ve iki appearance kaynağı | GCC C++20 sözdizimi denetimi geçti. |
| Modül metot gövdeleri ve UI string kimlikleri | Başlangıçla karşılaştırma geçti; modül gövdelerinde yalnız tip isimleri değişti. |
| `git diff --check` | Geçti. |
| `Test-HybridBridge.py` | Denendi, `javac` bulunmadığı için çalıştırılamadı. |
| Windows DLL/launcher/native auth ve oyun içi test | Bu ortamda çalıştırılmadı. |

İlk npm denemesinde bağımlılıklar eksikti; kilit dosyasına bağlı `npm ci
--prefix server --ignore-scripts --no-audit --no-fund` sonrası başlangıç testleri
geçti. Yeniden adlandırma sırasında testlerin yakaladığı kısa namespace
referansları, eski test tip adı ve Linux test include yolu düzeltildi. Hiçbir test
kaldırılmadı veya başarısızlığı görmezden gelinmedi; son tam C++ turu geçti.

## Değişiklik istatistikleri

Bu tur: **12 yeniden adlandırma, 3 yeni dosya, 38 diğer değiştirilmiş dosya;
tamamen kaldırılan dosya yok.** Üretim kaynaklarında net **126 satır azalma** var.
Yeni kontroller, regresyon fixture'ları ve bu denetim belgesi ayrıca satır ekler.
Git diff toplamı: **609 ekleme / 293 silme; net +316 satır**. Önceki yerel değişiklikler hariçtir.

Ölçüm, çalışma başında kaydedilen yerel tree
`1f80357703ffea70df0ec9f6c928819fe3c8df63` ile son çalışma ağacının geçici
index'teki tree'si arasında `git diff --find-renames=20% --stat/--numstat` ile
alınır. Gerçek Git index'i değiştirilmez. Benzer aim header'larında Git'in
eşleştirmesi farklı olabilse de amaçlanan eski→yeni eşleştirme yukarıda açıkça verilir.
