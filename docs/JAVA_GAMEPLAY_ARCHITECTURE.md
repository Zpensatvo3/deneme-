# Oyun modüllerinin Java geçişi

Hedef: görsel olmayan oyun modüllerinin karar ve oyun işlemlerini Java client
thread'inde toplamak. C++ modül kaydı, mevcut menü, ayarlar, odak kontrolü ve
DLL/JVM yaşam döngüsünü yönetmeye devam eder. Arkadaş ve Anti Bot filtreleri
görsel modüllerle paylaşıldığı için geçiş sırasında tek bir ortak kaynak olarak
korunmalıdır.

Her modülün bağımsız bir Java denetleyicisi vardır. C++ güncel ayar snapshot'ını
ve etkinlik durumunu köprü üzerinden iletir. Denetleyici oyun nesnelerini yalnızca
client thread'inde okur/yazar; nesne sahipliğini, oturum değişimini ve iptali
kontrol eder. Kapanışta yalnızca kendi sahiplendiği durumu temizler. Başarısız
bir işlem yeniden oynatılmaz ve aynı sürümde ikinci bir native uygulama çalışmaz.

Sürümler tam olarak 1.21.11, 26.1, 26.1.1 ve 26.1.2 olarak seçilir. Ortak
denetleyici farklı isimleri sürüm eşlemeleri üzerinden çözer; bilinmeyen sürüm
ve eksik bağlar reddedilir. Her sürüm için kaynak/gömülü Java sınıfları, JNI
sözleşmeleri ve oyun içi davranış ayrı doğrulanmalıdır.

Sprint, Fast Place, No Jump Delay ve Fast EXP için dört hedefin tamamında Java tek uygulamadır.
26.x isimleri resmi sınıf dosyaları ve izole JVM senaryolarıyla kontrol edilir.
Sonraki adımlar diğer eşya/envanter modülleri ve combat
denetleyicileridir. Bir modülün native yolu ancak ilgili sürümler için Java yolu
ve temizlik davranışı doğrulandıktan sonra kaldırılır.

Offline testler gerçek Windows DLL derlemesi veya oyun içi kabul değildir.
