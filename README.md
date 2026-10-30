# OneStep — proje haritası

Windows x64 üzerinde çalışan C++20 Minecraft Fabric istemcisi. Tam hedefler: **1.21.11, 26.1, 26.1.1 ve 26.1.2**. Her hedef için ayrı DLL üretilir.

## Ana akış

`src/main.cpp` → `src/app/runtime.cpp` → JVM/sürüm kontrolü → ortak Java manager oturumu → native modül kaydı → render hook'u → modül döngüsü → kaynakların kapatılması.

C++ menü, görsel çizim, platform erişimi ve JNI bağlantılarını yönetir. Java oyun kontrolcüleri `src/bridge/` altındadır; gömülü çıktıları `assets/hybrid/` ve `assets/skybridge/` içindedir. Başlangıç tamamlanmadan çalışmayı `gui::runtime_ready` engeller. Launcher, lisans/auth servisi ve loader ekranı bulunmaz.

36 modülün kimlik, kategori, varsayılan ve ayar şemalarının tek otoritesi [ModuleCatalog.java](src/bridge/ModuleCatalog.java) dosyasıdır. [ModuleManager.java](src/bridge/ModuleManager.java) aç/kapa, kısayol, revision ve lifecycle nesillerini; [ConfigManager.java](src/bridge/ConfigManager.java) doğrulama, eski config dönüşümleri ve atomik import/export işlemlerini yönetir. Ayarların native görünüm envanteri [settings_manifest.json](src/modules/settings_manifest.json), sürüm ve mapping envanteri [manifest.json](src/sdk/minecraft/common/manifest.json) içindedir.

[ManagerBridge.java](src/bridge/ManagerBridge.java) bütün gameplay köprülerinin paylaştığı oyun classloader'ına yüklenir. ABI, tam hedef sürüm veya oturum sahibi uyuşmazsa başlangıç durur; eski native yönetim yoluna dönüş yapılmaz. C++ tarafındaki [manager.cpp](src/modules/core/manager.cpp) yalnızca değişen revision için bütün snapshot'ı yeniler. Callback boyunca sabitlenen görünüm sayesinde farklı revision'lardan ayar ve enabled alanları karışmaz. UI ve kısayollar Java'ya istek gönderir; [settings_store.hpp](src/modules/core/settings_store.hpp) bağımsız ayar deposu değil, salt okunur DTO önbelleğidir.

Public config biçimi ve yerel keybind dosyası korunur. Hatalı config hiçbir modüle kısmen uygulanmaz; aynı config'in tekrar yüklenmesi bekleyen oyun işlemlerini sıfırlar. Watermark/ArrayList ayarları da ortak yönetime dahildir; menü teması ve pencere tercihleri native kalır. Native gameplay motorları bu değişiklikle Java'ya taşınmamıştır. Java gameplay kontrolcüleri ise ayarları JNI parametreleri yerine manager snapshot'ından okur.

## Derleme ve kontroller

Windows derlemesi için x64 Developer PowerShell, MSVC C++20/Windows SDK, CMake, Ninja, Python 3 ve JDK 21 gerekir. Ninja yalnızca değişen girdilere bağlı işleri tekrar çalıştırır. 26.x oyun çalışma zamanı Java 25 gerektirir.

```powershell
.\scripts\Build-Release.ps1 -MinecraftVersion '1.21.11' -SkipSign
.\scripts\Build-Release.ps1 -AllMinecraftVersions -SkipSign
.\Build-Test.ps1 -MinecraftVersion '1.21.11'
```

Normal DLL: `build/minecraft/<sürüm>/Onestep.dll`. İzole deneme DLL'i: `build/local-test/<sürüm>/Onestep-TEST-NO-LAUNCHER.dll`. Ara dosyalar: `build/ninja/<sürüm>/<yapılandırma>/`.

```sh
python scripts/Test-Infrastructure.py
python scripts/Test-Offline.py --jobs 2
python scripts/Test-GrimUI.py
python scripts/Test-ModuleManagers.py
python scripts/Test-HybridBridge.py
python scripts/Test-GameBridgeContracts.py
node --test config-service/worker.test.mjs
```

Java testleri JDK 17+ ve C++20 derleyicisi ister. `Test-ModuleManagers.py`, kaynak ve gömülü çekirdeği, gerçek JVM/JNI üzerinden paket yüklemeyi, ortak parent/two-child classloader görünürlüğünü, dört hedefi, config işlemlerini ve değişmeyen üretim çıktılarının zaman damgalarını denetler. Resmi client dosyalarını kullanan ek denetimler `Test-GameplayBindings.py`, `Test-MinecraftSdk.py`, `Test-MinecraftMappings.py` ve `Test-MinecraftBridges.py` içindedir; gerekli argümanlar `--help` ile gösterilir. Offline sonuçlar Windows DLL derlemesi veya oyun içi doğrulama yerine geçmez.

`Build-GameBridge.ps1` veya CMake/MSBuild, Java sınıflarını derleyip [Pack-ManagerCore.py](scripts/Pack-ManagerCore.py) ile `ManagerCore.bin`, native factory tablosu ve DTO/UI varsayılan/sınır sabitlerini üretir. Bu üretilmiş dosyalar elle düzenlenmez; içerik aynıysa tekrar yazılmaz. MSBuild bu adımı C++ derlemesinden önce çalıştırır ve ayrı bir freshness stamp kullanır. Java yönetim kodu değiştiğinde aynı kalan native sabitler gereksiz yeniden derlemeye neden olmaz.

## Public config servisi

[config-service/](config-service/) istemciden bağımsız Cloudflare Worker/D1 servisidir. Elle anahtar veya kullanıcı girişi istemez; yayın sahipliği istemcinin otomatik oluşturup DPAPI ile sakladığı kurulum sırrıyla belirlenir. Sunucu yalnızca hash saklar. Native HTTPS taşıması [transport.cpp](src/configs/transport.cpp), Worker davranışı [worker.js](config-service/worker.js), veri şeması [schema.sql](config-service/schema.sql) içindedir.

## Tam klasör ve dosya ağacı

951 mevcut proje dosyası; üçüncü taraf `include/` içerikleri dahildir. `.git/`, önbellekler ve üretilmiş yerel derleme çıktıları bu kaynak haritasına dahil değildir. Satır sonundaki notlar klasörün veya ana dosyanın görevini belirtir. Yeni dosya eklendiğinde bu envanter de güncellenmelidir.

```text
./
├── .github/
│   └── workflows/  # GitHub Actions derleme ve yayın hazırlığı
│       ├── build.yml
│       └── publish-dll.yml
├── assets/  # DLL içine gömülen görseller, fontlar ve Java sınıfları
│   ├── backgrounds/
│   │   └── main-background.png
│   ├── cosmetics/  # Wings ve Cape dokuları
│   │   ├── cape-astral.png
│   │   ├── cape-phoenix.png
│   │   ├── wing-angel.png
│   │   ├── wing-butterfly.png
│   │   └── wing-dragon.png
│   ├── fonts/  # Font dosyaları ve tam lisans metinleri
│   │   ├── FontAwesome-Free-Solid-900.otf
│   │   ├── GoogleSans-Bold.ttf
│   │   ├── GoogleSans-SemiBold.ttf
│   │   ├── LICENSE-Font-Awesome.txt
│   │   ├── Montserrat-Bold.ttf
│   │   ├── Montserrat-SemiBold.ttf
│   │   ├── OFL-Google-Sans.txt
│   │   └── OFL-Montserrat.txt
│   ├── hybrid/  # Üretilmiş gameplay/manager sınıfları ve native DTO sabitleri; kaynakları src/bridge
│   │   ├── net/
│   │   │   └── onestep/
│   │   │       └── hybrid/
│   │   │           ├── ClientTickRelay.class
│   │   │           ├── ClientTickTransformer.class
│   │   │           ├── ConfigManager.class
│   │   │           ├── GameBridge.class
│   │   │           ├── InventoryBridge.class
│   │   │           ├── ItemBridge.class
│   │   │           ├── Json$Parser.class
│   │   │           ├── Json.class
│   │   │           ├── ManagerBridge.class
│   │   │           ├── ModuleCatalog$Definition.class
│   │   │           ├── ModuleCatalog$Field.class
│   │   │           ├── ModuleCatalog.class
│   │   │           ├── ModuleManager$Result.class
│   │   │           ├── ModuleManager$Snapshot.class
│   │   │           ├── ModuleManager$State.class
│   │   │           ├── ModuleManager.class
│   │   │           ├── UtilityBridge.class
│   │   │           └── WorldBridge.class
│   │   ├── catalog_values.hpp  # Java kataloğundan üretilen native varsayılan ve sınır değerleri
│   │   ├── ManagerCore.bin  # Ortak parent classloader'a yüklenen manager sınıf paketi
│   │   └── module_manager_catalog.inc  # Java modül sırasından üretilen native factory tablosu
│   ├── icons/  # Menü ve Spotify ikonları
│   │   ├── helmet.png
│   │   ├── spotify-2015-seeklogo.png
│   │   └── spotify.png
│   ├── skies/  # Sky modülü panoramaları
│   │   ├── aurora.png
│   │   ├── candy_dusk.png
│   │   ├── kloppenheim_02_puresky.jpg
│   │   ├── kloppenheim_06_puresky.jpg
│   │   ├── nebula.png
│   │   └── rogland_clear_night.jpg
│   └── skybridge/  # Derlenmiş Sky/Fog köprüleri
│       └── net/
│           └── onestep/
│               └── visual/
│                   ├── SkyBridge.class
│                   ├── SkyFogBridge.class
│                   └── SkyTransformer.class
├── build-config/  # MSBuild sürüm hedefi tanımları
│   └── MinecraftTarget.props
├── config-service/  # Bağımsız public config Worker ve D1 şeması
│   ├── schema.sql  # D1 veri şeması
│   ├── worker.js  # Public config HTTPS API'si
│   ├── worker.test.mjs  # API doğrulama ve sahiplik testleri
│   └── wrangler.jsonc  # Worker ve DB binding yapılandırması
├── include/  # Depoya alınmış bağımlılıklar ve ortak derleme başlığı
│   ├── glm/  # GLM matematik kütüphanesi
│   │   ├── detail/
│   │   │   ├── _features.hpp
│   │   │   ├── _fixes.hpp
│   │   │   ├── _noise.hpp
│   │   │   ├── _swizzle.hpp
│   │   │   ├── _swizzle_func.hpp
│   │   │   ├── _vectorize.hpp
│   │   │   ├── compute_common.hpp
│   │   │   ├── compute_vector_decl.hpp
│   │   │   ├── compute_vector_relational.hpp
│   │   │   ├── func_common.inl
│   │   │   ├── func_common_simd.inl
│   │   │   ├── func_exponential.inl
│   │   │   ├── func_exponential_simd.inl
│   │   │   ├── func_geometric.inl
│   │   │   ├── func_geometric_simd.inl
│   │   │   ├── func_integer.inl
│   │   │   ├── func_integer_simd.inl
│   │   │   ├── func_matrix.inl
│   │   │   ├── func_matrix_simd.inl
│   │   │   ├── func_packing.inl
│   │   │   ├── func_packing_simd.inl
│   │   │   ├── func_trigonometric.inl
│   │   │   ├── func_trigonometric_simd.inl
│   │   │   ├── func_vector_relational.inl
│   │   │   ├── func_vector_relational_simd.inl
│   │   │   ├── glm.cpp
│   │   │   ├── qualifier.hpp
│   │   │   ├── setup.hpp
│   │   │   ├── type_float.hpp
│   │   │   ├── type_half.hpp
│   │   │   ├── type_half.inl
│   │   │   ├── type_mat2x2.hpp
│   │   │   ├── type_mat2x2.inl
│   │   │   ├── type_mat2x3.hpp
│   │   │   ├── type_mat2x3.inl
│   │   │   ├── type_mat2x4.hpp
│   │   │   ├── type_mat2x4.inl
│   │   │   ├── type_mat3x2.hpp
│   │   │   ├── type_mat3x2.inl
│   │   │   ├── type_mat3x3.hpp
│   │   │   ├── type_mat3x3.inl
│   │   │   ├── type_mat3x4.hpp
│   │   │   ├── type_mat3x4.inl
│   │   │   ├── type_mat4x2.hpp
│   │   │   ├── type_mat4x2.inl
│   │   │   ├── type_mat4x3.hpp
│   │   │   ├── type_mat4x3.inl
│   │   │   ├── type_mat4x4.hpp
│   │   │   ├── type_mat4x4.inl
│   │   │   ├── type_mat4x4_simd.inl
│   │   │   ├── type_quat.hpp
│   │   │   ├── type_quat.inl
│   │   │   ├── type_quat_simd.inl
│   │   │   ├── type_vec1.hpp
│   │   │   ├── type_vec1.inl
│   │   │   ├── type_vec2.hpp
│   │   │   ├── type_vec2.inl
│   │   │   ├── type_vec3.hpp
│   │   │   ├── type_vec3.inl
│   │   │   ├── type_vec4.hpp
│   │   │   ├── type_vec4.inl
│   │   │   ├── type_vec4_simd.inl
│   │   │   └── type_vec_simd.inl
│   │   ├── ext/
│   │   │   ├── _matrix_vectorize.hpp
│   │   │   ├── matrix_clip_space.hpp
│   │   │   ├── matrix_clip_space.inl
│   │   │   ├── matrix_common.hpp
│   │   │   ├── matrix_common.inl
│   │   │   ├── matrix_double2x2.hpp
│   │   │   ├── matrix_double2x2_precision.hpp
│   │   │   ├── matrix_double2x3.hpp
│   │   │   ├── matrix_double2x3_precision.hpp
│   │   │   ├── matrix_double2x4.hpp
│   │   │   ├── matrix_double2x4_precision.hpp
│   │   │   ├── matrix_double3x2.hpp
│   │   │   ├── matrix_double3x2_precision.hpp
│   │   │   ├── matrix_double3x3.hpp
│   │   │   ├── matrix_double3x3_precision.hpp
│   │   │   ├── matrix_double3x4.hpp
│   │   │   ├── matrix_double3x4_precision.hpp
│   │   │   ├── matrix_double4x2.hpp
│   │   │   ├── matrix_double4x2_precision.hpp
│   │   │   ├── matrix_double4x3.hpp
│   │   │   ├── matrix_double4x3_precision.hpp
│   │   │   ├── matrix_double4x4.hpp
│   │   │   ├── matrix_double4x4_precision.hpp
│   │   │   ├── matrix_float2x2.hpp
│   │   │   ├── matrix_float2x2_precision.hpp
│   │   │   ├── matrix_float2x3.hpp
│   │   │   ├── matrix_float2x3_precision.hpp
│   │   │   ├── matrix_float2x4.hpp
│   │   │   ├── matrix_float2x4_precision.hpp
│   │   │   ├── matrix_float3x2.hpp
│   │   │   ├── matrix_float3x2_precision.hpp
│   │   │   ├── matrix_float3x3.hpp
│   │   │   ├── matrix_float3x3_precision.hpp
│   │   │   ├── matrix_float3x4.hpp
│   │   │   ├── matrix_float3x4_precision.hpp
│   │   │   ├── matrix_float4x2.hpp
│   │   │   ├── matrix_float4x2_precision.hpp
│   │   │   ├── matrix_float4x3.hpp
│   │   │   ├── matrix_float4x3_precision.hpp
│   │   │   ├── matrix_float4x4.hpp
│   │   │   ├── matrix_float4x4_precision.hpp
│   │   │   ├── matrix_int2x2.hpp
│   │   │   ├── matrix_int2x2_sized.hpp
│   │   │   ├── matrix_int2x3.hpp
│   │   │   ├── matrix_int2x3_sized.hpp
│   │   │   ├── matrix_int2x4.hpp
│   │   │   ├── matrix_int2x4_sized.hpp
│   │   │   ├── matrix_int3x2.hpp
│   │   │   ├── matrix_int3x2_sized.hpp
│   │   │   ├── matrix_int3x3.hpp
│   │   │   ├── matrix_int3x3_sized.hpp
│   │   │   ├── matrix_int3x4.hpp
│   │   │   ├── matrix_int3x4_sized.hpp
│   │   │   ├── matrix_int4x2.hpp
│   │   │   ├── matrix_int4x2_sized.hpp
│   │   │   ├── matrix_int4x3.hpp
│   │   │   ├── matrix_int4x3_sized.hpp
│   │   │   ├── matrix_int4x4.hpp
│   │   │   ├── matrix_int4x4_sized.hpp
│   │   │   ├── matrix_integer.hpp
│   │   │   ├── matrix_integer.inl
│   │   │   ├── matrix_projection.hpp
│   │   │   ├── matrix_projection.inl
│   │   │   ├── matrix_relational.hpp
│   │   │   ├── matrix_relational.inl
│   │   │   ├── matrix_transform.hpp
│   │   │   ├── matrix_transform.inl
│   │   │   ├── matrix_uint2x2.hpp
│   │   │   ├── matrix_uint2x2_sized.hpp
│   │   │   ├── matrix_uint2x3.hpp
│   │   │   ├── matrix_uint2x3_sized.hpp
│   │   │   ├── matrix_uint2x4.hpp
│   │   │   ├── matrix_uint2x4_sized.hpp
│   │   │   ├── matrix_uint3x2.hpp
│   │   │   ├── matrix_uint3x2_sized.hpp
│   │   │   ├── matrix_uint3x3.hpp
│   │   │   ├── matrix_uint3x3_sized.hpp
│   │   │   ├── matrix_uint3x4.hpp
│   │   │   ├── matrix_uint3x4_sized.hpp
│   │   │   ├── matrix_uint4x2.hpp
│   │   │   ├── matrix_uint4x2_sized.hpp
│   │   │   ├── matrix_uint4x3.hpp
│   │   │   ├── matrix_uint4x3_sized.hpp
│   │   │   ├── matrix_uint4x4.hpp
│   │   │   ├── matrix_uint4x4_sized.hpp
│   │   │   ├── quaternion_common.hpp
│   │   │   ├── quaternion_common.inl
│   │   │   ├── quaternion_common_simd.inl
│   │   │   ├── quaternion_double.hpp
│   │   │   ├── quaternion_double_precision.hpp
│   │   │   ├── quaternion_exponential.hpp
│   │   │   ├── quaternion_exponential.inl
│   │   │   ├── quaternion_float.hpp
│   │   │   ├── quaternion_float_precision.hpp
│   │   │   ├── quaternion_geometric.hpp
│   │   │   ├── quaternion_geometric.inl
│   │   │   ├── quaternion_relational.hpp
│   │   │   ├── quaternion_relational.inl
│   │   │   ├── quaternion_transform.hpp
│   │   │   ├── quaternion_transform.inl
│   │   │   ├── quaternion_trigonometric.hpp
│   │   │   ├── quaternion_trigonometric.inl
│   │   │   ├── scalar_common.hpp
│   │   │   ├── scalar_common.inl
│   │   │   ├── scalar_constants.hpp
│   │   │   ├── scalar_constants.inl
│   │   │   ├── scalar_int_sized.hpp
│   │   │   ├── scalar_integer.hpp
│   │   │   ├── scalar_integer.inl
│   │   │   ├── scalar_packing.hpp
│   │   │   ├── scalar_packing.inl
│   │   │   ├── scalar_reciprocal.hpp
│   │   │   ├── scalar_reciprocal.inl
│   │   │   ├── scalar_relational.hpp
│   │   │   ├── scalar_relational.inl
│   │   │   ├── scalar_uint_sized.hpp
│   │   │   ├── scalar_ulp.hpp
│   │   │   ├── scalar_ulp.inl
│   │   │   ├── vector_bool1.hpp
│   │   │   ├── vector_bool1_precision.hpp
│   │   │   ├── vector_bool2.hpp
│   │   │   ├── vector_bool2_precision.hpp
│   │   │   ├── vector_bool3.hpp
│   │   │   ├── vector_bool3_precision.hpp
│   │   │   ├── vector_bool4.hpp
│   │   │   ├── vector_bool4_precision.hpp
│   │   │   ├── vector_common.hpp
│   │   │   ├── vector_common.inl
│   │   │   ├── vector_double1.hpp
│   │   │   ├── vector_double1_precision.hpp
│   │   │   ├── vector_double2.hpp
│   │   │   ├── vector_double2_precision.hpp
│   │   │   ├── vector_double3.hpp
│   │   │   ├── vector_double3_precision.hpp
│   │   │   ├── vector_double4.hpp
│   │   │   ├── vector_double4_precision.hpp
│   │   │   ├── vector_float1.hpp
│   │   │   ├── vector_float1_precision.hpp
│   │   │   ├── vector_float2.hpp
│   │   │   ├── vector_float2_precision.hpp
│   │   │   ├── vector_float3.hpp
│   │   │   ├── vector_float3_precision.hpp
│   │   │   ├── vector_float4.hpp
│   │   │   ├── vector_float4_precision.hpp
│   │   │   ├── vector_int1.hpp
│   │   │   ├── vector_int1_sized.hpp
│   │   │   ├── vector_int2.hpp
│   │   │   ├── vector_int2_sized.hpp
│   │   │   ├── vector_int3.hpp
│   │   │   ├── vector_int3_sized.hpp
│   │   │   ├── vector_int4.hpp
│   │   │   ├── vector_int4_sized.hpp
│   │   │   ├── vector_integer.hpp
│   │   │   ├── vector_integer.inl
│   │   │   ├── vector_packing.hpp
│   │   │   ├── vector_packing.inl
│   │   │   ├── vector_reciprocal.hpp
│   │   │   ├── vector_reciprocal.inl
│   │   │   ├── vector_relational.hpp
│   │   │   ├── vector_relational.inl
│   │   │   ├── vector_uint1.hpp
│   │   │   ├── vector_uint1_sized.hpp
│   │   │   ├── vector_uint2.hpp
│   │   │   ├── vector_uint2_sized.hpp
│   │   │   ├── vector_uint3.hpp
│   │   │   ├── vector_uint3_sized.hpp
│   │   │   ├── vector_uint4.hpp
│   │   │   ├── vector_uint4_sized.hpp
│   │   │   ├── vector_ulp.hpp
│   │   │   └── vector_ulp.inl
│   │   ├── gtc/
│   │   │   ├── bitfield.hpp
│   │   │   ├── bitfield.inl
│   │   │   ├── color_space.hpp
│   │   │   ├── color_space.inl
│   │   │   ├── constants.hpp
│   │   │   ├── constants.inl
│   │   │   ├── epsilon.hpp
│   │   │   ├── epsilon.inl
│   │   │   ├── integer.hpp
│   │   │   ├── integer.inl
│   │   │   ├── matrix_access.hpp
│   │   │   ├── matrix_access.inl
│   │   │   ├── matrix_integer.hpp
│   │   │   ├── matrix_inverse.hpp
│   │   │   ├── matrix_inverse.inl
│   │   │   ├── matrix_transform.hpp
│   │   │   ├── matrix_transform.inl
│   │   │   ├── noise.hpp
│   │   │   ├── noise.inl
│   │   │   ├── packing.hpp
│   │   │   ├── packing.inl
│   │   │   ├── quaternion.hpp
│   │   │   ├── quaternion.inl
│   │   │   ├── quaternion_simd.inl
│   │   │   ├── random.hpp
│   │   │   ├── random.inl
│   │   │   ├── reciprocal.hpp
│   │   │   ├── round.hpp
│   │   │   ├── round.inl
│   │   │   ├── type_aligned.hpp
│   │   │   ├── type_precision.hpp
│   │   │   ├── type_precision.inl
│   │   │   ├── type_ptr.hpp
│   │   │   ├── type_ptr.inl
│   │   │   ├── ulp.hpp
│   │   │   ├── ulp.inl
│   │   │   └── vec1.hpp
│   │   ├── gtx/
│   │   │   ├── associated_min_max.hpp
│   │   │   ├── associated_min_max.inl
│   │   │   ├── bit.hpp
│   │   │   ├── bit.inl
│   │   │   ├── closest_point.hpp
│   │   │   ├── closest_point.inl
│   │   │   ├── color_encoding.hpp
│   │   │   ├── color_encoding.inl
│   │   │   ├── color_space.hpp
│   │   │   ├── color_space.inl
│   │   │   ├── color_space_YCoCg.hpp
│   │   │   ├── color_space_YCoCg.inl
│   │   │   ├── common.hpp
│   │   │   ├── common.inl
│   │   │   ├── compatibility.hpp
│   │   │   ├── compatibility.inl
│   │   │   ├── component_wise.hpp
│   │   │   ├── component_wise.inl
│   │   │   ├── dual_quaternion.hpp
│   │   │   ├── dual_quaternion.inl
│   │   │   ├── easing.hpp
│   │   │   ├── easing.inl
│   │   │   ├── euler_angles.hpp
│   │   │   ├── euler_angles.inl
│   │   │   ├── extend.hpp
│   │   │   ├── extend.inl
│   │   │   ├── extended_min_max.hpp
│   │   │   ├── extended_min_max.inl
│   │   │   ├── exterior_product.hpp
│   │   │   ├── exterior_product.inl
│   │   │   ├── fast_exponential.hpp
│   │   │   ├── fast_exponential.inl
│   │   │   ├── fast_square_root.hpp
│   │   │   ├── fast_square_root.inl
│   │   │   ├── fast_trigonometry.hpp
│   │   │   ├── fast_trigonometry.inl
│   │   │   ├── float_normalize.inl
│   │   │   ├── float_notmalize.inl
│   │   │   ├── functions.hpp
│   │   │   ├── functions.inl
│   │   │   ├── gradient_paint.hpp
│   │   │   ├── gradient_paint.inl
│   │   │   ├── handed_coordinate_space.hpp
│   │   │   ├── handed_coordinate_space.inl
│   │   │   ├── hash.hpp
│   │   │   ├── hash.inl
│   │   │   ├── integer.hpp
│   │   │   ├── integer.inl
│   │   │   ├── intersect.hpp
│   │   │   ├── intersect.inl
│   │   │   ├── io.hpp
│   │   │   ├── io.inl
│   │   │   ├── iteration.hpp
│   │   │   ├── iteration.inl
│   │   │   ├── log_base.hpp
│   │   │   ├── log_base.inl
│   │   │   ├── matrix_cross_product.hpp
│   │   │   ├── matrix_cross_product.inl
│   │   │   ├── matrix_decompose.hpp
│   │   │   ├── matrix_decompose.inl
│   │   │   ├── matrix_factorisation.hpp
│   │   │   ├── matrix_factorisation.inl
│   │   │   ├── matrix_interpolation.hpp
│   │   │   ├── matrix_interpolation.inl
│   │   │   ├── matrix_major_storage.hpp
│   │   │   ├── matrix_major_storage.inl
│   │   │   ├── matrix_operation.hpp
│   │   │   ├── matrix_operation.inl
│   │   │   ├── matrix_query.hpp
│   │   │   ├── matrix_query.inl
│   │   │   ├── matrix_transform_2d.hpp
│   │   │   ├── matrix_transform_2d.inl
│   │   │   ├── mixed_product.hpp
│   │   │   ├── mixed_product.inl
│   │   │   ├── norm.hpp
│   │   │   ├── norm.inl
│   │   │   ├── normal.hpp
│   │   │   ├── normal.inl
│   │   │   ├── normalize_dot.hpp
│   │   │   ├── normalize_dot.inl
│   │   │   ├── number_precision.hpp
│   │   │   ├── number_precision.inl
│   │   │   ├── optimum_pow.hpp
│   │   │   ├── optimum_pow.inl
│   │   │   ├── orthonormalize.hpp
│   │   │   ├── orthonormalize.inl
│   │   │   ├── pca.hpp
│   │   │   ├── pca.inl
│   │   │   ├── perpendicular.hpp
│   │   │   ├── perpendicular.inl
│   │   │   ├── polar_coordinates.hpp
│   │   │   ├── polar_coordinates.inl
│   │   │   ├── projection.hpp
│   │   │   ├── projection.inl
│   │   │   ├── quaternion.hpp
│   │   │   ├── quaternion.inl
│   │   │   ├── range.hpp
│   │   │   ├── raw_data.hpp
│   │   │   ├── raw_data.inl
│   │   │   ├── rotate_normalized_axis.hpp
│   │   │   ├── rotate_normalized_axis.inl
│   │   │   ├── rotate_vector.hpp
│   │   │   ├── rotate_vector.inl
│   │   │   ├── scalar_multiplication.hpp
│   │   │   ├── scalar_relational.hpp
│   │   │   ├── scalar_relational.inl
│   │   │   ├── spline.hpp
│   │   │   ├── spline.inl
│   │   │   ├── std_based_type.hpp
│   │   │   ├── std_based_type.inl
│   │   │   ├── string_cast.hpp
│   │   │   ├── string_cast.inl
│   │   │   ├── structured_bindings.hpp
│   │   │   ├── structured_bindings.inl
│   │   │   ├── texture.hpp
│   │   │   ├── texture.inl
│   │   │   ├── transform.hpp
│   │   │   ├── transform.inl
│   │   │   ├── transform2.hpp
│   │   │   ├── transform2.inl
│   │   │   ├── type_aligned.hpp
│   │   │   ├── type_aligned.inl
│   │   │   ├── type_trait.hpp
│   │   │   ├── type_trait.inl
│   │   │   ├── vec_swizzle.hpp
│   │   │   ├── vector_angle.hpp
│   │   │   ├── vector_angle.inl
│   │   │   ├── vector_query.hpp
│   │   │   ├── vector_query.inl
│   │   │   ├── wrap.hpp
│   │   │   └── wrap.inl
│   │   ├── simd/
│   │   │   ├── common.h
│   │   │   ├── exponential.h
│   │   │   ├── geometric.h
│   │   │   ├── integer.h
│   │   │   ├── matrix.h
│   │   │   ├── neon.h
│   │   │   ├── packing.h
│   │   │   ├── platform.h
│   │   │   ├── trigonometric.h
│   │   │   └── vector_relational.h
│   │   ├── CMakeLists.txt
│   │   ├── common.hpp
│   │   ├── copying.txt
│   │   ├── exponential.hpp
│   │   ├── ext.hpp
│   │   ├── fwd.hpp
│   │   ├── geometric.hpp
│   │   ├── glm.cppm
│   │   ├── glm.hpp
│   │   ├── integer.hpp
│   │   ├── mat2x2.hpp
│   │   ├── mat2x3.hpp
│   │   ├── mat2x4.hpp
│   │   ├── mat3x2.hpp
│   │   ├── mat3x3.hpp
│   │   ├── mat3x4.hpp
│   │   ├── mat4x2.hpp
│   │   ├── mat4x3.hpp
│   │   ├── mat4x4.hpp
│   │   ├── matrix.hpp
│   │   ├── packing.hpp
│   │   ├── trigonometric.hpp
│   │   ├── vec2.hpp
│   │   ├── vec3.hpp
│   │   ├── vec4.hpp
│   │   └── vector_relational.hpp
│   ├── imgui/  # Dear ImGui ve platform/render backend'leri
│   │   ├── imconfig.h
│   │   ├── imgui.cpp
│   │   ├── imgui.h
│   │   ├── imgui_demo.cpp
│   │   ├── imgui_draw.cpp
│   │   ├── imgui_impl_opengl3.cpp
│   │   ├── imgui_impl_opengl3.h
│   │   ├── imgui_impl_opengl3_loader.h
│   │   ├── imgui_impl_win32.cpp
│   │   ├── imgui_impl_win32.h
│   │   ├── imgui_internal.h
│   │   ├── imgui_tables.cpp
│   │   ├── imgui_widgets.cpp
│   │   ├── imstb_rectpack.h
│   │   ├── imstb_textedit.h
│   │   └── imstb_truetype.h
│   ├── jni/  # JNI/JVMTI başlıkları ve Windows JVM import kütüphanesi
│   │   ├── linux/
│   │   │   ├── jawt_md.h
│   │   │   └── jni_md.h
│   │   ├── win32/
│   │   │   ├── bridge/
│   │   │   │   ├── AccessBridgeCallbacks.h
│   │   │   │   ├── AccessBridgeCalls.h
│   │   │   │   └── AccessBridgePackages.h
│   │   │   ├── jawt_md.h
│   │   │   └── jni_md.h
│   │   ├── classfile_constants.h
│   │   ├── jawt.h
│   │   ├── jdwpTransport.h
│   │   ├── jni.h
│   │   ├── jvm.lib
│   │   ├── jvmti.h
│   │   └── jvmticmlr.h
│   ├── minhook/  # Windows fonksiyon kancaları
│   │   ├── hde/
│   │   │   ├── hde32.c
│   │   │   ├── hde32.h
│   │   │   ├── hde64.c
│   │   │   ├── hde64.h
│   │   │   ├── pstdint.h
│   │   │   ├── table32.h
│   │   │   └── table64.h
│   │   ├── mem/
│   │   │   ├── mem.cpp
│   │   │   └── mem.hpp
│   │   ├── buffer.c
│   │   ├── buffer.h
│   │   ├── hook.c
│   │   ├── hook.hpp
│   │   ├── MinHook.h
│   │   ├── trampoline.c
│   │   └── trampoline.h
│   ├── pch.hpp
│   └── tinyformat.h
├── scripts/  # Derleme, kaynak üretimi ve doğrulama araçları
│   ├── Build-ClientTickHooks.ps1
│   ├── Build-CosmeticAssets.ps1
│   ├── Build-Dev.ps1
│   ├── Build-GameBridge.ps1
│   ├── Build-Ninja.ps1
│   ├── Build-Release.ps1
│   ├── Build-SkyBridge.ps1
│   ├── Check-GameBridgeContracts.py
│   ├── Fetch-SkyPanoramas.ps1
│   ├── Generate-Minecraft261Names.py
│   ├── minecraft_classfile.py
│   ├── Pack-ManagerCore.py  # Tekrarlanabilir manager paketi/factory/DTO sabiti üretimi
│   ├── Protect-Release.ps1
│   ├── Refresh-ProjectFilters.py
│   ├── repository_hygiene.py
│   ├── Resolve-AsmClasspath.ps1
│   ├── Test-AutoTotem.py
│   ├── Test-ClientTickHooks.py
│   ├── Test-GameBridgeContracts.py
│   ├── Test-GameplayBindings.py  # Resmi oyun dosyaları üzerinden gameplay binding kontrolü
│   ├── Test-GrimUI.py
│   ├── Test-HybridBridge.py
│   ├── Test-Infrastructure.py
│   ├── Test-JvmLifecycle.py
│   ├── Test-Minecraft261Offline.py
│   ├── Test-Minecraft26Readiness.py
│   ├── Test-MinecraftBridges.py
│   ├── Test-MinecraftMappings.py
│   ├── Test-MinecraftSdk.py
│   ├── Test-ModuleManagers.py  # Kaynak + gömülü Java core ve gerçek JNI manager testleri
│   ├── Test-Offline.py
│   ├── Test-TriggerbotBridge.py
│   └── world_binding_fixture.py
├── src/  # Uygulamanın C++ ve Java kaynakları
│   ├── app/  # Başlatma, hazır/kapanış durumu ve modül döngüsü
│   │   ├── module_loop.cpp  # Modül tuşları ve worker tick döngüsü
│   │   ├── module_loop.hpp
│   │   ├── runtime.cpp  # JVM, modüller, config ve render yaşam döngüsü
│   │   └── runtime.hpp
│   ├── bridge/  # Java oyun kontrolcüleri ve bytecode dönüşümleri
│   │   ├── ClientTickRelay.java  # Ortak client tick bildirimi
│   │   ├── ClientTickTransformer.java  # Tick ve yerel envanter ekranı bytecode üretimi
│   │   ├── ConfigManager.java  # Ortak doğrulama, legacy dönüşümler, atomik config/keybind işlemleri
│   │   ├── GameBridge.java  # No Jump Delay
│   │   ├── InventoryBridge.java  # Pot Refill ve Mouse Tweaks
│   │   ├── ItemBridge.java  # Fast EXP
│   │   ├── Json.java  # Sınırlı, strict UTF-8 JSON codec
│   │   ├── ManagerBridge.java  # Ortak classloader oturumu ve UTF-8 manager protokolü
│   │   ├── ModuleCatalog.java  # 36 modülün tek kimlik, kategori, varsayılan ve ayar şeması
│   │   ├── ModuleManager.java  # Enabled/keybind/lifecycle otoritesi ve immutable snapshot yayını
│   │   ├── SkyBridge.java  # Sky render entegrasyonu
│   │   ├── SkyFogBridge.java  # Sis rengi aktarımı
│   │   ├── SkyTransformer.java  # Sky/Fog bytecode dönüşümleri
│   │   ├── UtilityBridge.java  # Sprint, Fast Place ve Full Bright
│   │   └── WorldBridge.java  # Triggerbot, Jump Reset, Auto Totem ve Pearl işlemleri
│   ├── cache/  # Minecraft bağlamının güncellenen görünümü
│   │   ├── cache.cpp
│   │   └── cache.hpp
│   ├── configs/  # Config taşıma, yerel anahtar atamaları ve arka plan istekleri
│   │   ├── configs.cpp
│   │   ├── configs.hpp
│   │   ├── keybind_profile.cpp
│   │   ├── keybind_profile.hpp
│   │   ├── transport.cpp
│   │   └── transport.hpp
│   ├── friends/  # Arkadaş listesi ve paylaşılan oyuncu kontrolleri
│   │   ├── friends.cpp
│   │   └── friends.hpp
│   ├── gui/  # OpenGL/Win32 giriş noktaları, widget'lar ve UI tercihleri
│   │   ├── control_draw.hpp
│   │   ├── gui.cpp  # Win32/OpenGL hook'ları, fiziksel giriş ve render döngüsü
│   │   ├── gui.hpp
│   │   ├── module_descriptions.hpp
│   │   ├── popup_controls.hpp
│   │   ├── surface_style.hpp
│   │   ├── theme_palette.hpp
│   │   ├── ui_state.cpp
│   │   ├── ui_state.hpp
│   │   ├── ui_typography.hpp
│   │   ├── watermark_draw.hpp
│   │   ├── widgets.cpp
│   │   └── widgets.hpp
│   ├── jvmti/  # Yüklü Java sınıflarını bulma ve JVMTI oturumu
│   │   ├── jvmti.cpp
│   │   └── jvmti.hpp
│   ├── maths/  # Ortak vektör ve açı işlemleri
│   │   ├── maths.cpp
│   │   └── maths.hpp
│   ├── modules/  # Modül kataloğu, davranışlar ve ayar şemaları
│   │   ├── client/  # Menü ve Panic
│   │   │   ├── click_gui.hpp
│   │   │   ├── panic.cpp
│   │   │   └── panic.hpp
│   │   ├── combat/  # Combat modülleri ve karar motorları
│   │   │   ├── settings/
│   │   │   │   ├── anti_bot_settings.hpp
│   │   │   │   ├── auto_totem_settings.hpp
│   │   │   │   └── shield_breaker_settings.hpp
│   │   │   ├── aim_assist.cpp
│   │   │   ├── aim_assist.hpp
│   │   │   ├── anti_bot.cpp
│   │   │   ├── anti_bot.hpp
│   │   │   ├── anti_bot_engine.hpp
│   │   │   ├── auto_totem.cpp
│   │   │   ├── auto_totem.hpp
│   │   │   ├── auto_totem_legit.cpp
│   │   │   ├── blatant_aim_config.hpp
│   │   │   ├── blatant_aim_engine.cpp
│   │   │   ├── blatant_aim_engine.hpp
│   │   │   ├── blatant_click_aim_assist.cpp
│   │   │   ├── blatant_click_aim_assist.hpp
│   │   │   ├── jump_reset.cpp
│   │   │   ├── jump_reset.hpp
│   │   │   ├── jump_reset_config.hpp
│   │   │   ├── legit_aim_config.hpp
│   │   │   ├── legit_aim_engine.cpp
│   │   │   ├── legit_aim_engine.hpp
│   │   │   ├── legit_aim_math.hpp
│   │   │   ├── legit_click_aim_assist.cpp
│   │   │   ├── legit_click_aim_assist.hpp
│   │   │   ├── shield_breaker.cpp
│   │   │   ├── shield_breaker.hpp
│   │   │   ├── trigger_adapter.cpp
│   │   │   ├── trigger_adapter.hpp
│   │   │   ├── trigger_config.hpp
│   │   │   ├── trigger_engine.cpp
│   │   │   ├── trigger_engine.hpp
│   │   │   ├── trigger_options.cpp
│   │   │   ├── triggerbot.cpp
│   │   │   └── triggerbot.hpp
│   │   ├── core/  # Ortak modül, config, kayıt ve yaşam döngüsü sözleşmeleri
│   │   │   ├── config_fields.cpp
│   │   │   ├── config_fields.hpp
│   │   │   ├── config_json.hpp
│   │   │   ├── manager.cpp  # Revision tabanlı senkronizasyon ve callback boyunca sabit görünüm
│   │   │   ├── manager.hpp  # Salt okunur native snapshot ve manager istek sözleşmesi
│   │   │   ├── module.cpp
│   │   │   ├── module.hpp
│   │   │   ├── module_registry.cpp
│   │   │   ├── module_registry.hpp
│   │   │   ├── module_runtime_state.hpp
│   │   │   ├── public_config.cpp
│   │   │   ├── public_config.hpp
│   │   │   ├── setting_schema.hpp
│   │   │   ├── settings_store.hpp  # Java-owned ayarların typed DTO görünümü; bağımsız mutable depo değil
│   │   │   └── snapshot_session.hpp
│   │   ├── friends/  # Friends Manager modül arayüzü
│   │   │   └── friend_manager.hpp
│   │   ├── misc/  # Misc modülleri
│   │   │   ├── settings/
│   │   │   │   ├── fast_exp_settings.hpp
│   │   │   │   ├── fast_place_settings.hpp
│   │   │   │   ├── mouse_tweaks_settings.hpp
│   │   │   │   ├── pearl_macro_settings.hpp
│   │   │   │   ├── pot_refill_settings.hpp
│   │   │   │   └── sprint_settings.hpp
│   │   │   ├── fast_exp.cpp
│   │   │   ├── fast_exp.hpp
│   │   │   ├── fast_place.cpp
│   │   │   ├── fast_place.hpp
│   │   │   ├── mouse_tweaks.cpp
│   │   │   ├── mouse_tweaks.hpp
│   │   │   ├── no_jump_delay.cpp
│   │   │   ├── no_jump_delay.hpp
│   │   │   ├── pearl_macro.cpp
│   │   │   ├── pearl_macro.hpp
│   │   │   ├── pot_refill.cpp
│   │   │   ├── pot_refill.hpp
│   │   │   ├── pot_refill_engine.hpp
│   │   │   ├── sprint.cpp
│   │   │   └── sprint.hpp
│   │   ├── render/  # Watermark ve ArrayList HUD çizimi
│   │   │   ├── settings/
│   │   │   │   ├── arraylist_settings.hpp
│   │   │   │   └── watermark_settings.hpp
│   │   │   ├── arraylist.cpp
│   │   │   ├── arraylist.hpp
│   │   │   ├── arraylist_layout.hpp
│   │   │   ├── watermark.cpp
│   │   │   └── watermark.hpp
│   │   ├── vape/  # Vape aim modülü ve bağımsız motorları
│   │   │   ├── engines/
│   │   │   │   ├── aimassist/
│   │   │   │   │   ├── AimAssist.hpp
│   │   │   │   │   ├── AimAssistRotation.hpp
│   │   │   │   │   └── AimAssistTargeting.hpp
│   │   │   │   └── math/
│   │   │   │       ├── RotationUtil.hpp
│   │   │   │       └── Vector3.hpp
│   │   │   ├── game_bindings.hpp
│   │   │   ├── vape_config.hpp
│   │   │   ├── vape_module.cpp
│   │   │   ├── vape_module.hpp
│   │   │   └── vape_options.cpp
│   │   ├── visual/  # ESP, kozmetik, Sky, Spotify ve diğer görseller
│   │   │   ├── settings/
│   │   │   │   ├── block_esp_settings.hpp
│   │   │   │   ├── cape_settings.hpp
│   │   │   │   ├── chams_settings.hpp
│   │   │   │   ├── china_hat_settings.hpp
│   │   │   │   ├── hit_particles_settings.hpp
│   │   │   │   ├── item_esp_settings.hpp
│   │   │   │   ├── jump_circles_settings.hpp
│   │   │   │   ├── player_esp_settings.hpp
│   │   │   │   ├── radar_settings.hpp
│   │   │   │   ├── sky_settings.hpp
│   │   │   │   ├── spotify_hud_settings.hpp
│   │   │   │   ├── storage_esp_settings.hpp
│   │   │   │   ├── trails_settings.hpp
│   │   │   │   └── wings_settings.hpp
│   │   │   ├── block_esp.cpp
│   │   │   ├── block_esp.hpp
│   │   │   ├── block_icon_bindings.hpp
│   │   │   ├── cape.cpp
│   │   │   ├── cape.hpp
│   │   │   ├── cape_design.hpp
│   │   │   ├── chams.cpp
│   │   │   ├── chams.hpp
│   │   │   ├── chams_model.hpp
│   │   │   ├── china_hat.cpp
│   │   │   ├── china_hat.hpp
│   │   │   ├── cosmetic_physics.hpp
│   │   │   ├── cosmetic_renderer.cpp
│   │   │   ├── cosmetic_renderer.hpp
│   │   │   ├── full_bright.cpp
│   │   │   ├── full_bright.hpp
│   │   │   ├── hit_particles.cpp
│   │   │   ├── hit_particles.hpp
│   │   │   ├── item_esp.cpp
│   │   │   ├── item_esp.hpp
│   │   │   ├── item_esp_geometry.hpp
│   │   │   ├── jump_circles.cpp
│   │   │   ├── jump_circles.hpp
│   │   │   ├── player_esp.cpp
│   │   │   ├── player_esp.hpp
│   │   │   ├── radar.cpp
│   │   │   ├── radar.hpp
│   │   │   ├── sky.cpp
│   │   │   ├── sky.hpp
│   │   │   ├── sky_bindings.hpp
│   │   │   ├── sky_panorama.cpp
│   │   │   ├── sky_panorama.hpp
│   │   │   ├── spotify_hud.cpp
│   │   │   ├── spotify_hud.hpp
│   │   │   ├── spotify_source.cpp
│   │   │   ├── spotify_source.hpp
│   │   │   ├── storage_esp.cpp
│   │   │   ├── storage_esp.hpp
│   │   │   ├── trails.cpp
│   │   │   ├── trails.hpp
│   │   │   ├── visual_effects_logic.hpp
│   │   │   ├── wings.cpp
│   │   │   └── wings.hpp
│   │   ├── module_catalog.cpp  # Üretilmiş tabloyla native motor nesnelerini oluşturur
│   │   ├── module_catalog.hpp
│   │   ├── module_lifecycle.hpp
│   │   ├── module_support.hpp
│   │   ├── modules.cpp
│   │   ├── modules.hpp
│   │   ├── settings_manifest.json  # Modül ayarlarının kayıt envanteri
│   │   └── ui_redirect.hpp
│   ├── platform/  # Sürüm, girdi ve JVM erişim katmanı
│   │   ├── adapter/  # Tam sürüm profilleri, kamera ve eşya modeli erişimi
│   │   │   ├── bindings_profile.hpp
│   │   │   ├── game_adapter.cpp
│   │   │   ├── game_adapter.hpp
│   │   │   ├── item_model.cpp
│   │   │   ├── item_model.hpp
│   │   │   ├── item_transform.hpp
│   │   │   ├── runtime_names.hpp
│   │   │   └── target_version.hpp
│   │   ├── input/  # Tuş kimlikleri ve scan-code eşlemesi
│   │   │   └── key_codes.hpp
│   │   ├── jni/  # JNI bağlama, oturum ve member-ID önbelleği
│   │   │   ├── jni_context.cpp
│   │   │   ├── jni_context.hpp
│   │   │   ├── jvm_session.cpp
│   │   │   ├── jvm_session.hpp
│   │   │   └── member_id_cache.hpp
│   │   └── test_crash_report.hpp
│   ├── sdk/
│   │   └── minecraft/  # Minecraft JNI sınıf ve üye tanımları
│   │       ├── common/  # Ortak SDK alanları ve manifest
│   │       │   ├── client.hpp
│   │       │   ├── client_entities.hpp
│   │       │   ├── entities.hpp
│   │       │   ├── entity_types.hpp
│   │       │   ├── geometry.hpp
│   │       │   ├── input.hpp
│   │       │   ├── inventory_network.hpp
│   │       │   ├── item_render.hpp
│   │       │   ├── java_types.hpp
│   │       │   ├── manifest.json
│   │       │   └── world_blocks.hpp
│   │       └── fabric_26_1/  # 26.x resmi ad eşlemeleri
│   │           └── official_names.hpp
│   ├── services/  # Modüllerin paylaştığı oyun ve fiziksel girdi servisleri
│   │   ├── game/  # Oyun bağlamı, hedef, envanter ve native/Java köprüleri
│   │   │   ├── anti_bot_service.cpp
│   │   │   ├── anti_bot_service.hpp
│   │   │   ├── attack_coordinator.cpp
│   │   │   ├── attack_coordinator.hpp
│   │   │   ├── attack_service.cpp
│   │   │   ├── attack_service.hpp
│   │   │   ├── client_context.cpp
│   │   │   ├── client_context.hpp
│   │   │   ├── hotbar_service.cpp
│   │   │   ├── hotbar_service.hpp
│   │   │   ├── hybrid_bridge.cpp
│   │   │   ├── hybrid_bridge.hpp
│   │   │   ├── inventory_bridge.cpp
│   │   │   ├── inventory_bridge.hpp
│   │   │   ├── inventory_service.cpp
│   │   │   ├── inventory_service.hpp
│   │   │   ├── item_bridge.cpp
│   │   │   ├── item_bridge.hpp
│   │   │   ├── loaded_chunks.hpp
│   │   │   ├── manager_bridge.cpp  # Parent classloader'a paket yükleme ve fail-closed JNI bağlantısı
│   │   │   ├── manager_bridge.hpp
│   │   │   ├── menu_input.cpp
│   │   │   ├── menu_input.hpp
│   │   │   ├── menu_input_policy.hpp
│   │   │   ├── overlay_context.hpp
│   │   │   ├── target_service.cpp
│   │   │   ├── target_service.hpp
│   │   │   ├── utility_bridge.cpp
│   │   │   ├── utility_bridge.hpp
│   │   │   ├── world_bridge.cpp
│   │   │   └── world_bridge.hpp
│   │   └── input/  # Fiziksel girdi takibi ve kalan native giriş yolu
│   │       ├── input_service.cpp
│   │       └── input_service.hpp
│   ├── ui/  # Menü sayfaları, HUD düzenleyici ve yerleşim modelleri
│   │   ├── block_picker.hpp
│   │   ├── config_page.cpp
│   │   ├── config_page.hpp
│   │   ├── config_page_model.hpp
│   │   ├── dock_layout.hpp
│   │   ├── friends_page.cpp
│   │   ├── friends_page.hpp
│   │   ├── grim_ui.cpp  # Ana menü, modül kartları, arama ve ayar panelleri
│   │   ├── grim_ui.hpp
│   │   ├── hud_editor.cpp
│   │   ├── hud_editor.hpp
│   │   ├── hud_editor_layout.hpp
│   │   ├── item_icon.hpp
│   │   ├── keybinds_model.hpp
│   │   ├── keybinds_page.cpp
│   │   ├── keybinds_page.hpp
│   │   ├── keyboard_layout.hpp
│   │   ├── menu_background.cpp
│   │   ├── menu_background.hpp
│   │   ├── module_categories.hpp
│   │   ├── settings_renderer.hpp
│   │   ├── themes_page.cpp
│   │   └── themes_page.hpp
│   ├── utils/  # Sürüm tespiti ve küçük ortak yardımcılar
│   │   ├── utils.cpp
│   │   ├── utils.hpp
│   │   ├── version_manager.cpp
│   │   └── version_manager.hpp
│   ├── main.cpp  # DLL giriş noktası ve worker başlatma
│   ├── mappings.hpp  # SDK uyumluluk giriş başlığı
│   ├── meta_jni.hpp  # JNI sınıf, alan, metot ve referans yardımcıları
│   └── pch.cpp
├── tests/  # Offline testler ve gerçek oyun yerine kullanılan taklitler
│   ├── auto_totem/  # Auto Totem Java kontrolcüsü
│   │   ├── AutoTotemBridgeTest.java
│   │   ├── class_1657.java
│   │   └── class_490.java
│   ├── client_tick/  # Client tick bytecode kancaları
│   │   └── ClientTickHooksTest.java
│   ├── hybrid/  # Oyun köprülerinin yaşam döngüsü ve işlem sahipliği
│   │   ├── ContainerBridgeTest.java
│   │   ├── GLFW.java
│   │   ├── HybridBridgeTest.java
│   │   ├── ItemBridgeTest.java
│   │   ├── UtilityBridgeTest.java
│   │   └── WorldBridgeTest.java
│   ├── jump_reset/  # Jump Reset Java kontrolcüsü
│   │   └── JumpResetBridgeTest.java
│   ├── support/  # Ekransız UI/Windows test yardımcıları
│   │   ├── dto_fixture.hpp  # Eski DTO format kontrolleri için izole codec fixture
│   │   ├── imgui_capture.hpp
│   │   ├── manager_fixture.cpp  # Headless native UI için inert manager test double
│   │   ├── ManagerTestDriver.java  # Gameplay testlerini gerçek manager üzerinden yapılandırır
│   │   └── Windows.h
│   ├── triggerbot/  # Triggerbot Java kontrolcüsü ve taklit JNI koordinatörü
│   │   ├── trigger_service.cpp
│   │   └── TriggerbotBridgeTest.java
│   ├── utility_named/  # 26.x adlandırmalarıyla gameplay kontrolcüleri ve oyun taklitleri
│   │   ├── net/
│   │   │   └── minecraft/
│   │   │       ├── client/
│   │   │       │   ├── gui/
│   │   │       │   │   └── screens/
│   │   │       │   │       └── Screen.java
│   │   │       │   ├── multiplayer/
│   │   │       │   │   └── ClientLevel.java
│   │   │       │   ├── player/
│   │   │       │   │   └── LocalPlayer.java
│   │   │       │   ├── KeyMapping.java
│   │   │       │   ├── Minecraft.java
│   │   │       │   └── Options.java
│   │   │       └── world/
│   │   │           └── item/
│   │   │               ├── BlockItem.java
│   │   │               ├── Item.java
│   │   │               ├── Items.java
│   │   │               ├── ItemStack.java
│   │   │               ├── PotionItem.java
│   │   │               └── SplashPotionItem.java
│   │   ├── NamedFastExpTest.java
│   │   ├── NamedFastPlaceTest.java
│   │   ├── NamedNoJumpDelayTest.java
│   │   └── NamedSprintTest.java
│   ├── aimassist_config_test.cpp
│   ├── aimassist_math_test.cpp
│   ├── anti_bot_engine_test.cpp
│   ├── BridgeCompatibilityTest.java
│   ├── config_fields_test.cpp
│   ├── config_service_test.cpp
│   ├── cosmetic_physics_test.cpp
│   ├── grim_ui_test.cpp  # Gerçek ImGui çizim ve etkileşim testi
│   ├── input_service_test.cpp
│   ├── item_esp_geometry_test.cpp
│   ├── jni_member_cache_test.cpp
│   ├── jni_owner_jvm_test.cpp
│   ├── jump_reset_config_test.cpp
│   ├── jvm_lifecycle_test.cpp
│   ├── key_codes_test.cpp
│   ├── ManagerCoreTest.java  # 36 modül, dört hedef, config/lifecycle ve yarış testleri
│   ├── menu_background_renderer_test.cpp
│   ├── minecraft261_sdk_test.cpp
│   ├── minecraft_bindings_dump.cpp
│   ├── minecraft_version_test.cpp
│   ├── module_runtime_test.cpp
│   ├── popup_controls_test.cpp
│   ├── pot_refill_engine_test.cpp
│   ├── public_config_test.cpp  # Gerçek JVM'de manager paketi, JNI, snapshot ve config entegrasyonu
│   ├── remaining_settings_test.cpp
│   ├── RetiredBridgeTargetsTest.java
│   ├── ReTransformFixture.java
│   ├── sdk_mappings_test.cpp
│   ├── settings_renderer_test.cpp
│   ├── settings_store_test.cpp
│   ├── SkyBridgeInstallationTest.java
│   ├── snapshot_session_test.cpp
│   ├── test_minecraft26_readiness.py
│   ├── test_minecraft_sdk_audit.py
│   ├── test_repository_hygiene.py
│   ├── trigger_config_test.cpp
│   ├── trigger_input_test.cpp
│   ├── ui_models_test.cpp  # Birleştirilmiş UI model kontrolleri
│   ├── ui_preferences_test.cpp
│   ├── utility_settings_test.cpp
│   ├── vape_adapter_test.cpp
│   └── vape_config_test.cpp
├── .gitignore
├── assets.rc  # Gömülü DLL kaynakları
├── Build-Test.ps1  # İzole yerel deneme DLL'i derleme
├── CMakeLists.txt  # Ninja bağımlılık grafiği ve DLL hedefi
├── OneStep.slnx  # Visual Studio çözümü
├── OneStep.vcxproj  # MSBuild projesi; Ninja kaynak listesinin de otoritesi
├── OneStep.vcxproj.filters  # Visual Studio dosya gruplaması
├── README.md  # Tek proje belgesi ve tam dosya haritası
└── resource.h  # Kaynak kimlikleri
```

## Kaynaklar ve lisanslar

Tam lisans/telif metinleri ilgili kaynak başlıklarında ve aşağıdaki yerel lisans dosyalarında korunur; bu özet onların yerine geçmez.

| Bileşen | Kaynak ve bildirim |
| --- | --- |
| Spartan Helmet ikonu | [redempticon / Flaticon](https://www.flaticon.com/free-icon/spartan_11280819); önceki proje bildirimine göre ücretsiz kullanımda kaynak atfı gerekir. |
| Fluent System Icons | [Microsoft](https://github.com/microsoft/fluentui-system-icons), MIT. |
| Dear ImGui | [ocornut/imgui](https://github.com/ocornut/imgui), MIT; bildirimler `include/imgui/` kaynaklarında. |
| MinHook | [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook), BSD 2-Clause; bildirim [MinHook.h](include/minhook/MinHook.h) içinde. |
| GLM | [g-truc/glm](https://github.com/g-truc/glm), MIT veya Happy Bunny; [copying.txt](include/glm/copying.txt). |
| Google Sans SemiBold/Bold | [Google Fonts](https://fonts.google.com/specimen/Google+Sans); [OFL-Google-Sans.txt](assets/fonts/OFL-Google-Sans.txt). |
| Montserrat SemiBold/Bold | [The Montserrat Project Authors](https://github.com/googlefonts/montserrat); [OFL-Montserrat.txt](assets/fonts/OFL-Montserrat.txt). |
| Font Awesome Free Solid | [Fonticons, Inc.](https://fontawesome.com/license/free); [LICENSE-Font-Awesome.txt](assets/fonts/LICENSE-Font-Awesome.txt). Font dosyaları OFL kapsamındadır; SVG/JS ikon lisansı ayrıdır. |

Greg Zaal / Poly Haven panoramaları [CC0](https://polyhaven.com/license) lisanslıdır. Tonemapped JPEG kaynakları 4096×2048 boyutuna küçültülerek paketlenmiştir:

| Dosya | Kaynak |
| --- | --- |
| `assets/skies/kloppenheim_02_puresky.jpg` | [Kloppenheim 02 Pure Sky](https://polyhaven.com/a/kloppenheim_02_puresky) |
| `assets/skies/rogland_clear_night.jpg` | [Rogland Clear Night](https://polyhaven.com/a/rogland_clear_night) |
| `assets/skies/kloppenheim_06_puresky.jpg` | [Kloppenheim 06 Pure Sky](https://polyhaven.com/a/kloppenheim_06_puresky) |

`assets/skies/` içindeki Candy Dusk, Nebula ve Aurora PNG'leri ile `assets/cosmetics/` altındaki beş Wings/Cape dokusu bu proje için görsel üretme aracıyla üretilmiştir. Kozmetik üretim tarihi 2026-10-20'dir. Kanatların üretim boyutu 640×640 RGBA, pelerinlerin boyutu 512×768'dir; yeniden boyutlandırma aracı `Build-CosmeticAssets.ps1` içindedir. Bunlar Poly Haven fotoğraflarından veya referans istemciden alınmış görseller değildir.

Spotify logosu, kullanıcının sağladığı `spotify-2015-vector-logo-seeklogo.zip` içindeki `spotify-2015-seeklogo.png` dosyasından alınmıştır. [Orijinal PNG](assets/icons/spotify-2015-seeklogo.png) değiştirilmeden korunur. Marka ve logo ilgili hak sahibine aittir; sağlanan dosya için ayrıca açık kaynak lisansı varsayılmamıştır.

[Argon](https://github.com/LvStrnggg/Argon) projesinin `738e75f0623581ffbbc116656835352f8df9eb22` sürümü davranış referansı olarak incelendi; kodu kopyalanmadı. [Lisansı](https://github.com/LvStrnggg/Argon/blob/main/LICENSE) tüm hakları saklı tuttuğundan kaynak veya binary dağıtım izni varsayılmaz. Kullanıcının sağladığı `6vb8h8/VapeV4.21-main` recovery/decompile projesi de yalnızca native/Java sorumluluk ayrımı için incelendi; kod, algoritma veya asset kopyalanmadı. Bu referansın CC0 bildirimi üçüncü taraf haklarını kapsamaz.
