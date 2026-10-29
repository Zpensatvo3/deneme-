# Java köprüsü sözleşme kontrolü

Native bağlantı ile gömülü Java sınıfları farklı metotlar beklerse ortak köprü
başlatılamaz. Bu nedenle kaldırılan özelliklerin JNI kayıtları ve metot aramaları
da kaynaklarıyla birlikte kaldırılır.

`scripts/Check-GameBridgeContracts.py`, beş oyun köprüsünün JNI imzalarını gerçek
derlenmiş sınıflarla karşılaştırır. DLL verilirse RCDATA kaynaklarını çalıştırmadan
okuyup derlenmiş sınıflarla karşılaştırır. Kontrol Java derleme aşamasında,
resource derlemesinden önce ve link sonrasında çalışır.

`scripts/Test-HybridBridge.py` hem kaynak hem gömülü sınıfları sınar. Ortak tick
ve Auto Totem ekran bytecode'u ayrıca JVM verifier ile test edilir; ayrıntılar
[Java köprüleri](HYBRID_MIGRATION.md) belgesindedir.

Yeni DLL çalışan oyundaki eski DLL'in yerini otomatik almaz. Yeni çıktıyı temiz
Minecraft oturumunda test etmek gerekir. Offline testler oyun içi kabul değildir.
