📚 Kütüphane Yönetim Sistemi
Güvenli dosya şifreleme, iki seviyeli kullanıcı yönetimi ve gerçek zamanlı log kaydını bir arada sunan terminal tabanlı kütüphane yönetim uygulaması.

📋 İçindekiler

Özellikler
Proje Yapısı
Güvenlik Mimarisi
Derleme ve Çalıştırma
İlk Kurulum
Kullanım Kılavuzu
Veri Dosyaları
Teknik Detaylar


Özellikler
Kitap Yönetimi

Kitap ekleme, silme ve arama (ID, başlık, yazar)
BST (Binary Search Tree) tabanlı hızlı erişim
Alfabetik, ID ve türe göre sıralama
Kitap durumu takibi: Rafta / Ödünçte / Hasarlı
Son 10 işlem için geri alma (Undo) desteği

Üye Yönetimi

TC kimlik numarası doğrulamalı üye kaydı
Güvenli şifre saklama (SHA-256 + salt + pepper)
Üye girişi: TC + Ad/Soyad + Şifre üçlü doğrulama

Ödünç Sistemi

Kitap ödünç alma ve iade işlemleri
Gecikme günü bazlı otomatik ceza hesabı
Aktif ödünçlerin listelenmesi

Çalışma Alanı Yönetimi

20 çalışma alanı (T1–T20)
Üye başına tek rezervasyon sınırı
Admin ve üye ayrı yetkilerle alan yönetimi
Bellek bütünlüğü için canary değer koruması

Güvenlik ve Log

Admin ve üye işlemleri için ayrı log zincirleri
Append-only audit log dosyaları
Tüm veri dosyaları AES-256-CTR ile şifreli
SHA-256 MAC ile veri bütünlüğü doğrulaması


Proje Yapısı
.
├── main.c          # Giriş noktası, menü sistemi, global state
├── book.c/h        # Kitap veritabanı (BST, CRUD, undo yığını)
├── member.c/h      # Üye veritabanı (bağlı liste, kimlik doğrulama)
├── loan.c/h        # Ödünç ve iade yönetimi, ceza hesabı
├── workspace.c/h   # Çalışma alanı rezervasyon sistemi
├── log.c/h         # Döngüsel log tamponu, audit dosyası
├── config.c/h      # Sistem konfigürasyonu, admin şifre yönetimi
├── common.c/h      # Ortak yardımcı fonksiyonlar, I/O, crypto arayüzü
├── aes.c/h         # AES-256-CTR şifreleme implementasyonu
├── sha256.c/h      # SHA-256 hash implementasyonu
└── data/           # Çalışma zamanında oluşturulan şifreli veri dosyaları

Güvenlik Mimarisi
Şifre Yönetimi
Kullanıcı şifreleri düz metin olarak asla saklanmaz. Aşağıdaki pipeline uygulanır:
SHA-256(şifre + salt + pepper) → hex string → veri tabanına kayıt

Salt: Her kullanıcı için /dev/urandom'dan üretilen 30 karakter rastgele değer
Pepper: KUTUPHANE_PEPPER ortam değişkeninden okunur; ayarlanmamışsa derleme zamanı fallback kullanılır
Sabit zamanlı karşılaştırma: secure_compare() ile zamanlama saldırılarına karşı koruma


Üretim ortamı için:
bashexport KUTUPHANE_PEPPER=<gizli_ve_uzun_deger>

Seed Kullanıcı Şifreleri
AdTCŞifreAhmet Yılmaz12345678901ahmet123Fatma Kaya23456789012fatma456Mehmet Demir34567890123mehmet789Ayşe Çelik45678901234ayse321Mustafa Şahin56789012345musta00
Veri Dosyası Şifreleme
Tüm veri dosyaları şu format ile diskte şifreli tutulur:
[magic(4 byte)] [version(1 byte)] [IV(16 byte)] [veri boyutu(8 byte)] [şifreli veri + SHA-256 MAC(32 byte)]

Algoritma: AES-256-CTR
IV yönetimi: Her yazma işleminde /dev/urandom'dan taze IV üretilir (IV yeniden kullanımı önlenir)
Bütünlük: SHA-256 MAC, şifreli verinin sonuna eklenir; okumada doğrulanır
Config dosyası: Bootstrap key ile şifrelenir; diğer tüm dosyalar config içindeki runtime key ile
Atomik yazma: rename() tabanlı atomic write ile yarım dosya bırakılmaz

Diğer Güvenlik Önlemleri

Tüm kullanıcı girdileri uzunluk sınırlı safe_read() ile alınır
Diskten okunan veriler null-termination ile sanitize edilir
Workspace slot'larında canary değerleri ile bellek bütünlüğü korunur
Başarısız admin girişlerinde 3 denemeden sonra sistem kapatılır


Derleme ve Çalıştırma
Gereksinimler

GCC veya uyumlu C99 derleyicisi
POSIX uyumlu sistem (Linux / macOS)
make (opsiyonel)

Derleme
bashgcc -o kutuphane \
    main.c book.c member.c loan.c workspace.c \
    log.c config.c common.c aes.c sha256.c \
    -Wall -Wextra -O2
veya Makefile ile:
bashmake
Çalıştırma
bash./kutuphane

İlk Kurulum
Program ilk çalıştırıldığında data/kutuphane.cfg dosyası bulunamaz ve ilk kurulum sihirbazı başlar:

Admin şifresi iki kez girilir ve doğrulanır
Sistem rastgele bir AES-256 runtime anahtarı üretir
Config dosyası bootstrap key ile şifrelenip kaydedilir
Seed kitaplar (40 adet), seed üyeler (5 kişi) ve 20 çalışma alanı otomatik oluşturulur


Kullanım Kılavuzu
Ana Menü
╔════════════════════════════════════╗
║    KUTUPHANE YÖNETİM SİSTEMİ      ║
╠════════════════════════════════════╣
║  1) Kutuphane Gorevlisi Girisi     ║
║  2) Uye Girisi                     ║
║  0) Cikis                          ║
╚════════════════════════════════════╝
Kütüphaneci (Admin) Menüsü
Tuşİşlev1Kitap ekle2Kitap sil (ID + yazar + başlık doğrulaması gerekir)3Kitap ara (ID veya yazar adına göre)4Kitapları sırala (alfabetik / ID / türe göre)5Üye ekle6Üye sil7Üyeleri listele (TC'ye göre sıralı)sÜye ara8Aktif ödünçleri görüntüle9Son 10 admin işlem logupAdmin şifre değiştiruSon kitap işlemini geri al (Undo)tTüm çalışma alanlarını listelenBoş çalışma alanlarını listelefRezerv çalışma alanlarını listeleaÇalışma alanı rezerv etdÇalışma alanı teslim al0Kaydet ve çık
Üye Menüsü
Tuşİşlev1Kitap ödünç al2Kitap iade et (gecikme cezası hesaplanır)3Kitapları sırala ve listele4Kitap ara (başlık veya yazar)5Şifre değiştir6Son 10 kişisel işlem logutTüm çalışma alanlarını görüntülenBoş çalışma alanlarını listeleaÇalışma alanı rezerv etdÇalışma alanı teslim et0Kaydet ve çık
Ödünç ve Ceza Kuralları

Ödünç süresi: 14 gün
Günlük gecikme cezası: 2.00 TL
Hasarlı kitap ödünç alınırken ek onay istenir


Veri Dosyaları
Program data/ dizini altında aşağıdaki dosyaları oluşturur:
Dosyaİçerikkutuphane.cfgAdmin şifresi (hash), AES runtime anahtarıbooks.dbKitap kayıtları (BST inorder sıralı)books.idxKitap ID indeksimembers.dbÜye kayıtlarıloans.dbAktif ödünç kayıtlarılog_admin.dbAdmin log tamponulog_user.dbÜye log tamponuworkspace.dbÇalışma alanı durumlarıaudit_admin.logAdmin audit log (düz metin, append-only)audit_user.logÜye audit log (düz metin, append-only)

Tüm .db dosyaları AES-256-CTR ile şifrelidir. Sadece .log dosyaları düz metin olarak tutulur.


Teknik Detaylar
Veri Yapıları
ModülYapıNedenbook.cBinary Search Tree (BST)ID bazlı O(log n) aramamember.cBağlı listeÜye sayısı az, sıralama için insertion sort yeterliloan.cBağlı listeAktif ödünç sayısı sınırlılog.cDairesel tampon (ring buffer)Son LOG_KEEP kaydı sabit bellekte tutarworkspace.cSabit boyutlu diziMAX_WORKSPACE=20, indeks erişimi O(1)
Kriptografi

AES-256-CTR: aes.c içinde sıfırdan yazılmış, dış bağımlılık yok
SHA-256: sha256.c içinde sıfırdan yazılmış, dış bağımlılık yok
Çift katman şifreleme: Config → bootstrap key; veri dosyaları → runtime key

Bellek Yönetimi

BST için dinamik yığın (heap); yığın taşması riski yok (BST_STACK_MAX = 8192)
Tüm malloc sonuçları NULL kontrolüyle güvence altında
Dosya yükleme sırasında tainted data sanitization uygulanır
