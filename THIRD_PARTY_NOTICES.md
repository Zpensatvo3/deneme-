# OneStep — Üçüncü taraf bildirimleri

Bu belge projede kullanılan üçüncü taraf görsel kaynakları ve kütüphaneleri
listeler. Tam lisans metinleri, ilgili kaynak dosyalarında veya aşağıdaki yerel
lisans dosyalarında bulunur.

## İkonlar

| Kaynak | Üretici | Lisans / bildirim |
| --- | --- | --- |
| Spartan Helmet | redempticon / Flaticon | Önceki proje bildirimine göre ücretsiz kullanımda kaynak atfı gerekir. |
| Fluent System Icons: People, Desktop, Toolbox, Eye, Folder, Settings | Microsoft | MIT |

Kaynak bağlantıları:

- Spartan Helmet: https://www.flaticon.com/free-icon/spartan_11280819
- Fluent System Icons: https://github.com/microsoft/fluentui-system-icons

## Gökyüzü panoramaları

Sky modülünde kullanılan Greg Zaal / Poly Haven görselleri CC0 lisanslıdır.
Kaynak tonemapped JPEG dosyaları 4096×2048 boyutuna küçültülüp JPEG olarak
paketlenmiştir; Samanyolu çiziminde yalnızca üstteki gökyüzü bölümü örneklenir.

| Yerel dosya | Kaynak |
| --- | --- |
| assets/skies/kloppenheim_02_puresky.jpg | [Kloppenheim 02 (Pure Sky)](https://polyhaven.com/a/kloppenheim_02_puresky) |
| assets/skies/rogland_clear_night.jpg | [Rogland Clear Night](https://polyhaven.com/a/rogland_clear_night) |
| assets/skies/kloppenheim_06_puresky.jpg | [Kloppenheim 06 (Pure Sky)](https://polyhaven.com/a/kloppenheim_06_puresky) |

[Poly Haven CC0 lisansı](https://polyhaven.com/license). Görseller DLL içine
gömülür; oyun çalışırken internet bağlantısı veya indirme yapılmaz.
Kaynak dosyalar `scripts/Fetch-SkyPanoramas.ps1` ile yeniden hazırlanabilir.

Stilize `candy_dusk.png`, `nebula.png` ve `aurora.png` dosyaları bu proje için
yerleşik görsel üretme aracıyla üretildi; Poly Haven fotoğraflarından türetilmedi.
Üretim istemleri ve paketleme bilgileri [assets/skies/README.md](assets/skies/README.md)
içindedir. Bu dosyalar da DLL'e gömülür.

## Fontlar

| Font | Kaynak | Yerel lisans metni |
| --- | --- | --- |
| Google Sans SemiBold / Bold | Google Fonts | [OFL-Google-Sans.txt](assets/fonts/OFL-Google-Sans.txt) |
| Montserrat SemiBold / Bold | The Montserrat Project Authors | [OFL-Montserrat.txt](assets/fonts/OFL-Montserrat.txt) |
| Font Awesome Free Solid | Fonticons, Inc. | [LICENSE-Font-Awesome.txt](assets/fonts/LICENSE-Font-Awesome.txt) |

Depodaki bu font dosyaları için SIL Open Font License 1.1 metinleri bulunur.
Font Awesome'ın font dosyaları OFL kapsamındadır; SVG/JS ikon lisansı ayrıdır
ve aynı dosya içinde açıklanır.

- Google Sans: https://fonts.google.com/specimen/Google+Sans
- Montserrat: https://github.com/googlefonts/montserrat
- Font Awesome: https://fontawesome.com/license/free

## Spotify logosu

Spotify HUD logosu, kullanıcının sağladığı
`spotify-2015-vector-logo-seeklogo.zip` paketindeki `spotify-2015-seeklogo.png`
dosyasından alınmıştır. `assets/icons/spotify-2015-seeklogo.png` orijinal dosyanın
birebir kopyasıdır; yeniden çizilmedi veya renklendirilmedi. Önceki kullanıcı
asset'i `assets/icons/spotify.svg` ve mekanik mesh derleyicisi arşiv amaçlı korunur.
Spotify markası ve logosu ilgili hak sahibine aittir;
kullanıcı dosyası için ayrıca bir açık kaynak lisansı varsayılmamıştır.

## Kaynak kütüphaneleri

- Dear ImGui: https://github.com/ocornut/imgui — MIT.
- MinHook: https://github.com/TsudaKageyu/minhook — BSD 2-Clause;
  telif ve koşullar `include/minhook/MinHook.h` içinde bulunur.
- GLM: https://github.com/g-truc/glm — MIT veya Happy Bunny;
  tam metin: [copying.txt](include/glm/copying.txt).

Bu özet tam lisans metinlerinin yerine geçmez. Dağıtım sırasında kullanılan
bağımlılıkların lisans ve telif bildirimleri de korunmalıdır.

## Özgün kozmetik kaynakları

Bu proje için üretilen Wings/Cape görsellerinin boyut, paketleme ve özgün üretim
istemleri [assets/cosmetics/README.md](assets/cosmetics/README.md) içindedir.
Bu kayıt üçüncü taraf marka/asset lisanslarının yerine geçmez.

## Hibrit mimari için incelenen yerel referans

Kullanıcının sağladığı `6vb8h8/VapeV4.21-main` recovery/decompile projesi,
native/Java sorumluluk ayrımı ve yaşam döngüsü fikri için incelendi. Kod,
algoritma veya asset kopyalanmadı; yeni GameBridge özgün OneStep uygulamasıdır.
Referansın CC0 bildirimi üçüncü taraf haklarını kapsamaz. İncelenen dosyalar,
yararlanılan noktalar ve sınırlar: [Hibrit geçiş](docs/HYBRID_MIGRATION.md).
