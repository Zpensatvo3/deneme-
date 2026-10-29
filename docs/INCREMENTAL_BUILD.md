# Artımlı derleme

Varsayılan derleme CMake + Ninja, derleyici MSVC'dir. Visual Studio Installer'da
C++ Build Tools, Windows SDK ve C++ CMake tools bileşenlerini kurun. JDK 17+
ve Python da gereklidir. Komutları x64 Developer PowerShell içinde çalıştırın.

```powershell
./scripts/Build-Dev.ps1
./scripts/Build-Release.ps1 -MinecraftVersion 1.21.11 -SkipSign
./Build-Test.ps1 -MinecraftVersion 1.21.11
```

İlk derleme Ninja için yeni bir önbellek oluşturur. Sonrakiler değişen `.cpp`
dosyalarını ve değişen başlıkları kullanan kaynakları derler. Değişiklik yoksa
DLL yeniden linklenmez. Bir ortak başlık çok dosyada kullanılıyorsa bu dosyaların
yeniden derlenmesi gereklidir. Bir nesne veya gömülü kaynak değiştiğinde DLL'in
yeniden linklenmesi normaldir.

Ara dosyalar `build/ninja/<sürüm>/<yapılandırma>/` altında korunur; test derlemesi
ayrı `Debug-test` klasörünü kullanır. Bunları silmek önbelleği sıfırlar. `-Clean`
yalnız özellikle tam yeniden derleme isteniyorsa kullanılmalıdır. Sürüm veya
yapılandırma değiştirildiğinde ilgili ayrı önbellek kullanılır.

Ninja kaynak listesini `OneStep.vcxproj` üzerinden okur ve wildcard klasörlerine
yeni `.cpp` eklenmesini algılar. Üretilen Java sınıfları ve Spotify başlığı yapı
klasöründe tutulur. RC girdileri, başlıklar, Java kaynakları ve kontrol scriptleri
grafikte bağımlılıktır; JNI sözleşme ve DLL kaynak kontrolleri korunur.

DLL çıktı yolları mevcut yollarla aynıdır. Geliştirme/test scriptleri önceki
DLL'i taşımak yerine kopyalayarak yedekler; çıktı dosyasının zaman damgası korunur.

Paralelliği sınırlamak için doğrudan yardımcıyı kullanabilirsiniz:

```powershell
./scripts/Build-Ninja.ps1 -MinecraftVersion 1.21.11 -Jobs 4
```

MSBuild karşılaştırması veya eski araç zinciri için:

```powershell
./scripts/Build-Release.ps1 -Backend MSBuild -MinecraftVersion 1.21.11 -SkipSign
```

CI, Windows Ninja derlemesini ve arka arkaya değişikliksiz ikinci derlemede
DLL zaman damgasının aynı kalmasını denetleyecek şekilde ayarlandı. Yerel Linux
kontrolünde CMake grafik üretimi ve Spotify üretiminin ikinci çalıştırmada
`no work to do` davranışı doğrulandı. Bu, gerçek MSVC derlemesi değildir;
Windows DLL derlemesi ve hız karşılaştırması henüz çalıştırılmadı.
