# OneStep – Yapılacaklar

## 1. Eklenecekler

- Inventory Profile
- Freecam

## 2. Kaldırılacaklar

- Aim Assist

## 3. Düzeltilecekler

- Vape Aim Assist, birkaç düzenlemeden sonra ana Aim Assist olacak.
- Mouse Tweaks tamamen Java tarafına geçirilecek.
- Panic modülünün ismi Self Destruct olarak değiştirilecek.

## 4. Geliştirilecekler

- Legit ve Blatant Click Aim Assist artık birleştirilecek ve Click Aim Assist altında toplanacak.
- Cape ve Wings modülleri Java'ya taşınacak. **Resimsiz olan Cape ve Wings kaldırılacak.**
- Hit Particles'ın oluşturduğu partiküller tamamen yeniden oluşturulacak.
- Sky'ın görüntü kalitesi ve oluşturduğu sisler düzenlenecek.
- Shield Breaker, oyuncunun kalkan tutup tutmadığını kontrol edecek ve Java'ya geçirilecek.
- [x] Block ESP tamamen 3D'ye geçirildi: 12 kenarlı kutular, altı yarı saydam yüzey ve görüş alanında kesme.

- [x] Triggerbot karar akışı ve saldırıları Argon davranış referansıyla Java'ya taşındı; mevcut ayarlar ve ortak helper bağlantıları korundu.

- [x] Jump Reset kararları, hasar takibi, zamanlama ve binding sahipliği Argon davranış referansıyla Java'ya taşındı; mevcut ayarlar korundu. Tuş okuması GLFW, basma/bırakma vanilla KeyBinding ile Java'dadır; eski System Space artık Game Space olarak çalışır.



- Auto Totem, Argon AutoInventoryTotem akışına göre Java WorldBridge feature 7’ye taşındı: Blatant/Random kaynak seçimi, offhand/hotbar, Auto Switch ve Auto Open/Stay Open For.

- [x] Argon karşılaştırmalı kontrol: Auto Totem clickSlot PlayerEntity imzası ve sahte envanter tıklama engeli düzeltildi, gerçek tick olayına bağlandı. Jump Reset tuş değiştirme sırasında eski sentetik basışı temizler. Auto Totem/Jump Reset ve bytecode hook kontrolleri eklendi/çalıştırıldı.
