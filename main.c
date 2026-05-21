
//Sistemdeki şifreler raporda 4.3(Sifre Yonetimi) kısmında ulaşabilirsiniz.


#include "common.h"
#include "book.h"
#include "member.h"
#include "loan.h"
#include "log.h"
#include "config.h"
#include "workspace.h"

/* ═══════════════════════════════════════════════════
   Global state
   ═══════════════════════════════════════════════════ */
static BookDBPtr      g_books      = NULL;
static MemberDBPtr    g_members    = NULL;
static LoanDBPtr      g_loans      = NULL;
static LogDBPtr       g_log_admin  = NULL;
static LogDBPtr       g_log_user   = NULL;
static Config         g_cfg;
static WorkspaceDBPtr g_workspace  = NULL;

/* ═══════════════════════════════════════════════════
   Yardımcılar
   ═══════════════════════════════════════════════════ */
static void save_all(void) {
    book_save(g_books, FILE_BOOKS, FILE_BOOKS_IDX);
    member_save(g_members, FILE_MEMBERS);
    loan_save(g_loans, FILE_LOANS);
    log_save(g_log_admin, FILE_LOG_ADMIN);
    log_save(g_log_user,  FILE_LOG_USER);
    config_save(&g_cfg, FILE_CONFIG);
    workspace_save(g_workspace, FILE_WORKSPACE);
}

static int read_id(const char* prompt) {
    char buf[32];
    while (1) {
        safe_read(buf, 32, prompt);
        if (is_digits_only(buf)) return atoi(buf);
        printf(CLR_RED "  Hata: Sadece rakam girilebilir!\n" CLR_RESET);
    }
}

static void press_enter_main(void) {
    printf("\n  " CLR_CYAN "[ Ana menu icin ENTER'a basin... ]" CLR_RESET);
    fflush(stdout);
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

static void press_enter(void) {
    printf("\n  [ Devam etmek icin ENTER'a basin... ]");
    fflush(stdout);
    int c; while ((c = getchar()) != '\n' && c != EOF);
}

static void clear_screen(void) { printf("\033[2J\033[H"); }

/* Devam Et / Ana Menü seçim ekranı
   Döndürür: true = devam et, false = ana menüye dön */
static bool devam_sor(void) {
    printf("\n  a) Isleme Devam Et\n");
    printf("  b) Ana Menuye Geri Don\n");
    char ch[4]; safe_read(ch, 4, "  Seciminiz: ");
    return !(ch[0]=='b' || ch[0]=='B');
}

/* ═══════════════════════════════════════════════════
   Kitap sıralama alt menüsü
   ═══════════════════════════════════════════════════ */
static void kitap_siralama_menu(void) {
    char ch[4];
    while (1) {
        printf("\n  +-------------------------------+\n");
        printf("  | Siralama Bicimi               |\n");
        printf("  | 1) Alfabetik                  |\n");
        printf("  | 2) ID Numarasina Gore         |\n");
        printf("  | 3) Turune Gore                |\n");
        printf("  | 0) Geri                       |\n");
        printf("  +-------------------------------+\n");
        safe_read(ch, 4, "  Seciminiz: ");
        if      (ch[0]=='1') book_list_alpha(g_books);
        else if (ch[0]=='2') book_list_by_id(g_books);
        else if (ch[0]=='3') book_list_by_genre(g_books);
        else if (ch[0]=='0') return;
        else printf("  Gecersiz secim.\n");
    }
}

/* 
----------- ÇALIŞMA ALANI FONKSİYONLARI ---------------
*/

/* --- Admin: Tüm çalışma alanlarını listele --- */
static void admin_ws_listele(void) {
    if (!devam_sor()) return;
    workspace_list_all(g_workspace);
    press_enter_main();
}

/* --- Admin: Boş çalışma alanlarını listele --- */
static void admin_ws_bos_listele(void) {
    if (!devam_sor()) return;
    workspace_list_empty(g_workspace);
    press_enter_main();
}

/* --- Admin: Rezerv çalışma alanlarını listele --- */
static void admin_ws_rezerv_listele(void) {
    if (!devam_sor()) return;
    workspace_list_reserved(g_workspace);
    press_enter_main();
}

/* --- Admin: Çalışma alanı rezerv et --- */
static void admin_ws_rezerv_et(void) {
    if (!devam_sor()) return;

    while (1) {
        workspace_list_empty(g_workspace);
        char wsname[MAX_WSNAME];
        safe_read(wsname, MAX_WSNAME, "  Rezerve etmek istediginiz alan adi: ");

        if (!workspace_is_valid(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' gecersiz bir alan adi.\n" CLR_RESET, wsname);
            continue;
        }
        if (workspace_is_reserved(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' alani dolu, baska bir alan secin.\n" CLR_RESET, wsname);
            continue;
        }

        /* Alan boş — TC iste */
        char tc[MAX_TC];
        safe_read(tc, MAX_TC, "  TC Kimlik No: ");
        MemberRecord* m = member_find(g_members, tc);
        if (!m) {
            printf(CLR_RED "  Hata: TC '%s' sistemde kayitli degil.\n" CLR_RESET, tc);
            press_enter_main();
            return;
        }

        /* Ad soyad iste */
        char name[MAX_NAME], sn[MAX_NAME];
        safe_read(name, MAX_NAME, "  Ad          : ");
        safe_read(sn,   MAX_NAME, "  Soyad       : ");
        if (strcmp(m->name, name)!=0 || strcmp(m->surname, sn)!=0) {
            printf(CLR_RED "  Hata: TC No ve Ad/Soyad uyusmuyor.\n" CLR_RESET);
            press_enter_main();
            return;
        }

        char fullname[MAX_NAME*2+2];
        snprintf(fullname, sizeof(fullname), "%s %s", name, sn);
        workspace_reserve(g_workspace, wsname, tc, fullname);
        printf(CLR_GREEN "  Basarili: '%s' alani %s adina rezerve edildi.\n"
               CLR_RESET, wsname, fullname);
        char logmsg[MAX_LOG_MSG];
        snprintf(logmsg, MAX_LOG_MSG, "Calisma alani rezerv: %s -> %s", wsname, fullname);
        log_push(g_log_admin, logmsg);
        press_enter_main();
        return;
    }
}

/* --- Admin: Çalışma alanı teslimi --- */
static void admin_ws_teslim(void) {
    if (!devam_sor()) return;

    while (1) {
        char wsname[MAX_WSNAME];
        safe_read(wsname, MAX_WSNAME, "  Teslim alinacak alan adi (Cikis icin 0): ");
        if (strcmp(wsname, "0") == 0) return;

        if (!workspace_is_valid(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' gecersiz bir alan adi.\n" CLR_RESET, wsname);
            continue;
        }
        if (!workspace_is_reserved(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' alani zaten bos.\n" CLR_RESET, wsname);
            continue;
        }

        /* Geçerli ve dolu alan — TC ve ad/soyad iste */
        char tc[MAX_TC], name[MAX_NAME], sn[MAX_NAME];
        safe_read(tc,   MAX_TC,   "  TC Kimlik No: ");
        safe_read(name, MAX_NAME, "  Ad          : ");
        safe_read(sn,   MAX_NAME, "  Soyad       : ");
        char fullname[MAX_NAME*2+2];
        snprintf(fullname, sizeof(fullname), "%s %s", name, sn);

        if (workspace_release(g_workspace, wsname, tc, fullname)) {
            printf(CLR_GREEN "  Basarili: '%s' alani bosaltildi.\n" CLR_RESET, wsname);
            char logmsg[MAX_LOG_MSG];
            snprintf(logmsg, MAX_LOG_MSG, "Calisma alani teslim: %s <- %s", wsname, fullname);
            log_push(g_log_admin, logmsg);
            press_enter_main();
            return;
        } else {
            printf(CLR_RED "  Hata: TC/Ad/Soyad bilgileri bu alan ile uyusmuyor.\n" CLR_RESET);
            char q[4]; safe_read(q, 4, "  Ana Menuye Donmek icin 0, tekrar icin Enter: ");
            if (q[0]=='0') { press_enter_main(); return; }
        }
    }
}

/* ═══════════════════════════════════════════════════
   ──────── KÜTÜPHANECİ SİSTEMİ ────────
   ═══════════════════════════════════════════════════ */

static void admin_kitap_ekle(void) {
    char author[MAX_AUTHOR], title[MAX_TITLE], genre[MAX_GENRE];
    int next_id = book_next_id(g_books);
    printf("\n  -- Kitap Ekle --\n");
    printf("  Otomatik atanacak ID: %d\n", next_id);
    safe_read(author, MAX_AUTHOR, "  Yazar Adi   : ");
    safe_read(title,  MAX_TITLE,  "  Kitap Adi   : ");
    safe_read(genre,  MAX_GENRE,  "  Tur         : ");
    if (!author[0] || !title[0]) { printf("  Bos birakilamaz.\n"); return; }
    if (!genre[0]) strncpy(genre, "Genel", MAX_GENRE-1);
    int id = book_add(g_books, author, title, genre);
    printf(CLR_GREEN "  Eklendi: [ID:%d] %s - %s (%s)\n" CLR_RESET, id, author, title, genre);
    char logmsg[MAX_LOG_MSG];
    snprintf(logmsg, MAX_LOG_MSG, "Kitap eklendi: [ID:%d] %.60s", id, title);
    log_push(g_log_admin, logmsg);
}

static void admin_kitap_sil(void) {
    printf("\n  -- Kitap Sil --\n");
    while (1) {
        int id = read_id("  Kitap ID    : ");
        char author[MAX_AUTHOR], title[MAX_TITLE];
        safe_read(author, MAX_AUTHOR, "  Yazar Adi   : ");
        safe_read(title,  MAX_TITLE,  "  Kitap Adi   : ");
        if (book_delete(g_books, id, author, title)) {
            printf(CLR_GREEN "  Silindi: [ID:%d] %s - %s\n" CLR_RESET, id, author, title);
            char logmsg[MAX_LOG_MSG];
            snprintf(logmsg, MAX_LOG_MSG, "Kitap silindi: [ID:%d] %.60s", id, title);
            log_push(g_log_admin, logmsg);
            return;
        } else {
            printf(CLR_RED "  Bilgiler yanlis ya da kitap bulunamadi.\n" CLR_RESET);
            char q[4]; safe_read(q, 4, "  Cikis icin 0, tekrar icin Enter: ");
            if (q[0]=='0') return;
        }
    }
}

static void admin_kitap_ara(void) {
    printf("\n  -- Kitap Ara --\n");
    int id = read_id("  Kitap ID    : ");
    BookRecord* b = book_find_by_id(g_books, id);
    if (b) {
        const char* d = b->status==BOOK_BORROWED?"Oduncte":b->status==BOOK_DAMAGED?"Hasarli":"Rafta";
        printf(CLR_GREEN "\n  Bulundu:\n" CLR_RESET);
        printf("  ID:%-4d | Yazar: %-28s | Ad: %-28s | Tur: %-14s | Durum: %s\n",
            b->id, b->author, b->title, b->genre, d);
        log_push(g_log_admin, "Kitap arama yapildi (admin)");
        return;
    }
    printf(CLR_YELLOW "  Bu ID ile kitap bulunamadi.\n" CLR_RESET);
    char author[MAX_AUTHOR];
    safe_read(author, MAX_AUTHOR, "  Yazar adi girin (bos=iptal): ");
    if (!author[0]) return;
    book_find_by_author(g_books, author);
    int id2 = read_id("  Listeden Kitap ID girin (0=iptal): ");
    if (id2==0) return;
    BookRecord* b2 = book_find_by_id(g_books, id2);
    if (b2) printf(CLR_GREEN "  Bulundu: [ID:%d] %s - %s (%s)\n" CLR_RESET, b2->id, b2->author, b2->title, b2->genre);
    else     printf(CLR_RED "  Kitap bulunamadi.\n" CLR_RESET);
    log_push(g_log_admin, "Kitap arama yapildi (admin)");
}

static void admin_uye_ekle(void) {
    printf("\n  -- Uye Ekle --\n");
    char tc[MAX_TC], name[MAX_NAME], sn[MAX_NAME], pw[MAX_PASS];
    safe_read(name, MAX_NAME, "  Ad          : ");
    safe_read(sn,   MAX_NAME, "  Soyad       : ");
    safe_read(tc,   MAX_TC,   "  TC Kimlik No: ");
    safe_read_password(pw, MAX_PASS, "  Uye Sifresi : ");
    if (member_add(g_members, tc, name, sn, pw)) {
        printf(CLR_GREEN "  Uye eklendi: %s %s (%s)\n" CLR_RESET, name, sn, tc);
        char logmsg[MAX_LOG_MSG];
        snprintf(logmsg, MAX_LOG_MSG, "Uye eklendi: %s %s TC:%s", name, sn, tc);
        log_push(g_log_admin, logmsg);
    }
}

static void admin_uye_sil(void) {
    printf("\n  -- Uye Sil --\n");
    while (1) {
        char tc[MAX_TC], name[MAX_NAME], sn[MAX_NAME];
        safe_read(name, MAX_NAME, "  Ad          : ");
        safe_read(sn,   MAX_NAME, "  Soyad       : ");
        safe_read(tc,   MAX_TC,   "  TC Kimlik No: ");
        if (member_delete(g_members, tc, name, sn)) {
            printf(CLR_GREEN "  Uye silindi: %s %s (%s)\n" CLR_RESET, name, sn, tc);
            char logmsg[MAX_LOG_MSG];
            snprintf(logmsg, MAX_LOG_MSG, "Uye silindi: %s %s TC:%s", name, sn, tc);
            log_push(g_log_admin, logmsg);
            return;
        } else {
            printf(CLR_RED "  Bilgiler uyusmuyor veya uye bulunamadi.\n" CLR_RESET);
            char q[4]; safe_read(q, 4, "  Cikis icin 0, tekrar icin Enter: ");
            if (q[0]=='0') return;
        }
    }
}

static void admin_uye_ara(void) {
    printf("\n  -- Uye Ara --\n");
    while (1) {
        char tc[MAX_TC], name[MAX_NAME], sn[MAX_NAME];
        safe_read(name, MAX_NAME, "  Ad          : ");
        safe_read(sn,   MAX_NAME, "  Soyad       : ");
        safe_read(tc,   MAX_TC,   "  TC Kimlik No: ");
        MemberRecord* m = member_find_full(g_members, tc, name, sn);
        if (m) {
            printf(CLR_GREEN "\n  Uye Bulundu:\n" CLR_RESET);
            printf("  TC: %-13s | Ad: %-18s | Soyad: %-18s\n", m->tc_no, m->name, m->surname);
            log_push(g_log_admin, "Uye arama yapildi (admin)");
            return;
        } else {
            printf(CLR_RED "  Bilgiler uyusmuyor.\n" CLR_RESET);
            char q[4]; safe_read(q, 4, "  Cikis icin 0, tekrar icin Enter: ");
            if (q[0]=='0') return;
        }
    }
}

static void admin_sifre_degistir(void) {
    printf("\n  -- Admin Sifre Degistir --\n");
    while (1) {
        char old_pw[MAX_PASS];
        char cand_hash[PASSWORD_HASH_SIZE];
        safe_read_password(old_pw, MAX_PASS, "  Eski Sifre  : ");
        hash_password(old_pw, g_cfg.admin_salt, cand_hash);
        if (!secure_compare(cand_hash, g_cfg.admin_password, PASSWORD_HASH_SIZE - 1)) {
            printf(CLR_RED "  Yanlis sifre. Sistemden cikis yapiliyor.\n" CLR_RESET);
            save_all(); exit(0);
        }
        while (1) {
            char np1[MAX_PASS], np2[MAX_PASS];
            safe_read_password(np1, MAX_PASS, "  Yeni Sifre       : ");
            safe_read_password(np2, MAX_PASS, "  Yeni Sifre(Tekrar): ");
            if (strcmp(np1, np2)==0) {
                generate_salt(g_cfg.admin_salt);
                hash_password(np1, g_cfg.admin_salt, g_cfg.admin_password);
                printf(CLR_GREEN "  Sifre basariyla degistirildi.\n" CLR_RESET);
                log_push(g_log_admin, "Admin sifresi degistirildi");
                return;
            }
            printf(CLR_RED "  Sifreler uyusmuyor.\n" CLR_RESET);
        }
    }
}

/* ─── Admin ana döngüsü ─────────────────────────── */
static void admin_menu(void) {
    char ch[4];
    while (1) {
        clear_screen();
        printf("\n  ╔═════════════════════════════════════════════╗\n");
        printf("  ║         KUTUPHANECI SİSTEMİ                 ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  1) Kitap Ekle                              ║\n");
        printf("  ║  2) Kitap Sil                               ║\n");
        printf("  ║  3) Kitap Ara                               ║\n");
        printf("  ║  4) Kitaplari Sirala ve Listele             ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  5) Uye Ekle                                ║\n");
        printf("  ║  6) Uye Sil                                 ║\n");
        printf("  ║  7) Uyeleri Sirala                          ║\n");
        printf("  ║  s) Uye Ara                                 ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  8) Odunc Alinan Kitaplari Goruntule        ║\n");
        printf("  ║  9) Log Kaydi (Son 10 Islem)                ║\n");
        printf("  ║  p) Admin Sifre Degistir                    ║\n");
        printf("  ║  u) Geri Al (Undo)                          ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  t) Calisma Alanlarini Listele              ║\n");
        printf("  ║  n) Bos Calisma Alanlarini Listele          ║\n");
        printf("  ║  f) Rezerv Calisma Alanlarini Listele       ║\n");
        printf("  ║  a) Calisma Alani Rezerv Et                 ║\n");
        printf("  ║  d) Calisma Alani Teslimi                   ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  0) Cikis                                   ║\n");
        printf("  ╚═════════════════════════════════════════════╝\n");
        safe_read(ch, 4, "  Seciminiz: ");

        if      (ch[0]=='1') { admin_kitap_ekle(); press_enter(); }
        else if (ch[0]=='2') { admin_kitap_sil();  press_enter(); }
        else if (ch[0]=='3') { admin_kitap_ara();  press_enter(); }
        else if (ch[0]=='4') { kitap_siralama_menu(); }
        else if (ch[0]=='5') { admin_uye_ekle();   press_enter(); }
        else if (ch[0]=='6') { admin_uye_sil();    press_enter(); }
        else if (ch[0]=='7') { member_list_sorted(g_members); log_push(g_log_admin,"Uye listesi goruntulendi"); press_enter(); }
        else if (ch[0]=='s'||ch[0]=='S') { admin_uye_ara(); press_enter(); }
        else if (ch[0]=='8') { loan_print_active(g_loans); log_push(g_log_admin,"Odunc listesi goruntulendi"); press_enter(); }
        else if (ch[0]=='9') { log_print(g_log_admin); press_enter(); }
        else if (ch[0]=='p'||ch[0]=='P') { admin_sifre_degistir(); press_enter(); }
        else if (ch[0]=='u'||ch[0]=='U') { book_undo(g_books); log_push(g_log_admin,"Undo islemi yapildi"); press_enter(); }
        /* Çalışma alanı seçenekleri */
        else if (ch[0]=='t'||ch[0]=='T') { admin_ws_listele(); }
        else if (ch[0]=='n'||ch[0]=='N') { admin_ws_bos_listele(); }
        else if (ch[0]=='f'||ch[0]=='F') { admin_ws_rezerv_listele(); }
        else if (ch[0]=='a'||ch[0]=='A') { admin_ws_rezerv_et(); }
        else if (ch[0]=='d'||ch[0]=='D') { admin_ws_teslim(); }
        else if (ch[0]=='0') { save_all(); printf("  Kaydedildi. Cikis yapiliyor...\n"); return; }
        else { printf("  Gecersiz secim!\n"); press_enter(); }
    }
}

/* ═══════════════════════════════════════════════════
   ──────────────  ÜYE SİSTEMİ  ──────────────
   ═══════════════════════════════════════════════════ */

static void uye_kitap_odunc_al(const char* tc) {
    printf("\n  -- Kitap Odunc Al --\n");
    if (!devam_sor()) return;

    while (1) {
        book_list_by_id(g_books);

        char buf[32];
        printf("\n  Odunc alinacak Kitap ID (Cikis icin 0): ");
        fflush(stdout);
        safe_read(buf, 32, NULL);
        if (buf[0] == '0' && buf[1] == '\0') return;
        if (!is_digits_only(buf)) {
            printf(CLR_RED "  Hata: Sadece rakam girilebilir!\n" CLR_RESET);
            continue;
        }
        int id = atoi(buf);
        if (id == 0) return;

        BookRecord* b = book_find_by_id(g_books, id);
        if (!b) {
            printf(CLR_RED "  Hata: Bu ID ile kitap bulunamadi. Tekrar deneyin veya 0 ile cikisin.\n" CLR_RESET);
            continue;
        }
        if (b->status == BOOK_BORROWED) {
            printf(CLR_RED "  Hata: [ID:%d] '%s' adli kitap su an baska bir uye tarafindan oduncte!\n" CLR_RESET, b->id, b->title);
            printf(CLR_YELLOW "  Lutfen baska bir kitap secin.\n" CLR_RESET);
            continue;
        }
        if (loan_borrow(g_loans, id, tc)) {
            printf(CLR_GREEN "\n  ✓ Onay: '%s' kitabi basariyla odunc alindi!\n" CLR_RESET, b->title);
            printf(CLR_CYAN "  Odunc suresi: %d gun. Gec iade: gunluk %.2f TL ceza uygulanir.\n" CLR_RESET,
                   LOAN_LIMIT_DAYS, LOAN_FINE_PER_DAY);
            char logmsg[MAX_LOG_MSG];
            snprintf(logmsg, MAX_LOG_MSG, "Odunc alindi: [ID:%d] %s", id, b->title);
            log_push(g_log_user, logmsg);
        }
        return;
    }
}

static void uye_kitap_iade_et(const char* tc) {
    printf("\n  -- Kitap Iade Et --\n");
    if (!devam_sor()) return;
    loan_print_active_by_member(g_loans, tc);
    int id = read_id("  Iade edilecek Kitap ID: ");
    BookRecord* b = book_find_by_id(g_books, id);
    if (!b) { printf(CLR_RED "  Kitap bulunamadi.\n" CLR_RESET); return; }
    if (!loan_member_has(g_loans, tc, id)) {
        printf(CLR_RED "  Bu kitap sizin odunc listenizde degil.\n" CLR_RESET); return;
    }
    int days = read_id("  Kac gun elinizde kaldi: ");
    double fine = 0;
    if (loan_return(g_loans, id, days, &fine)) {
        if (fine > 0)
            printf(CLR_YELLOW "\n  Gecikme: %d gun | Ceza: %.2f TL\n" CLR_RESET, days - LOAN_LIMIT_DAYS, fine);
        else
            printf(CLR_GREEN "\n  Zamaninda iade. Borcunuz: 0.00 TL\n" CLR_RESET);
        char logmsg[MAX_LOG_MSG];
        snprintf(logmsg, MAX_LOG_MSG, "Iade edildi: [ID:%d] %s (%.0f gun, ceza:%.2fTL)", id, b->title, (double)days, fine);
        log_push(g_log_user, logmsg);
    }
}

static void uye_kitap_ara(void) {
    printf("\n  -- Kitap Ara --\n");
    if (!devam_sor()) return;
    char title[MAX_TITLE];
    safe_read(title, MAX_TITLE, "  Kitap Adi  : ");
    BookRecord* b = book_find_by_title(g_books, title);
    if (b) {
        const char* d = b->status==BOOK_BORROWED?"Oduncte":b->status==BOOK_DAMAGED?"Hasarli":"Rafta";
        printf(CLR_GREEN "  Bulundu: [ID:%-4d] %-30s - %-30s | Tur: %-14s | %s\n" CLR_RESET, b->id, b->author, b->title, b->genre, d);
        log_push(g_log_user, "Kitap arama yapildi");
        press_enter_main(); return;
    }
    printf(CLR_YELLOW "  Kitap adi ile bulunamadi.\n" CLR_RESET);
    char author[MAX_AUTHOR];
    safe_read(author, MAX_AUTHOR, "  Yazar Adi  : ");
    if (!author[0]) { press_enter_main(); return; }
    book_find_by_author(g_books, author);
    int id2 = read_id("  Listeden Kitap ID girin (0=iptal): ");
    if (id2==0) { press_enter_main(); return; }
    BookRecord* b2 = book_find_by_id(g_books, id2);
    if (b2) printf(CLR_GREEN "  Bulundu: [ID:%-4d] %-30s - %-30s | %s\n" CLR_RESET, b2->id, b2->author, b2->title, b2->genre);
    else     printf(CLR_RED "  Islem basarisiz.\n" CLR_RESET);
    log_push(g_log_user, "Kitap arama yapildi");
    press_enter_main();
}

static void uye_sifre_degistir(MemberRecord* m) {
    printf("\n  -- Uye Sifre Degistir --\n");
    if (!devam_sor()) return;
    char old_pw[MAX_PASS];
    safe_read_password(old_pw, MAX_PASS, "  Eski Sifre        : ");
    if (!member_check_password(m, old_pw)) {
        printf(CLR_RED "  Yanlis sifre. Ana menuye donuluyor.\n" CLR_RESET);
        press_enter_main(); return;
    }
    while (1) {
        char np1[MAX_PASS], np2[MAX_PASS];
        safe_read_password(np1, MAX_PASS, "  Yeni Sifre        : ");
        safe_read_password(np2, MAX_PASS, "  Yeni Sifre(Tekrar): ");
        if (strcmp(np1,np2)==0) {
            member_set_password(m, np1);
            printf(CLR_GREEN "  Sifre degistirildi.\n" CLR_RESET);
            log_push(g_log_user, "Uye sifresi degistirildi");
            press_enter_main(); return;
        }
        printf(CLR_RED "  Sifreler uyusmuyor. Tekrar deneyin.\n" CLR_RESET);
    }
}

static void uye_kitap_sirala_listele(void) {
    printf("\n  -- Kitaplari Sirala ve Listele --\n");
    if (!devam_sor()) return;
    kitap_siralama_menu();
}

/* ── Üye çalışma alanı fonksiyonları ──────────────── */

static void uye_ws_listele(void) {
    if (!devam_sor()) return;
    workspace_list_all_user(g_workspace);
    press_enter_main();
}

static void uye_ws_bos_listele(void) {
    if (!devam_sor()) return;
    workspace_list_empty(g_workspace);
    press_enter_main();
}

static void uye_ws_rezerv_et(const char* session_tc, MemberRecord* m) {
    /* FAIL-SAFE: Üye zaten rezerve ettiyse kesinlikle ikinci rezerv yapamaz */
    if (workspace_member_has_reservation(g_workspace, session_tc)) {
        printf(CLR_RED "\n  Hata: Zaten aktif bir rezervasyonunuz var.\n" CLR_RESET);
        printf(CLR_YELLOW "  Oncelikle mevcut rezervasyonunuzu teslim etmeniz gerekiyor.\n" CLR_RESET);
        workspace_list_reserved(g_workspace);
        press_enter_main();
        return;
    }
    if (!devam_sor()) return;

    while (1) {
        workspace_list_empty(g_workspace);
        char wsname[MAX_WSNAME];
        safe_read(wsname, MAX_WSNAME, "  Rezerve etmek istediginiz alan adi (Cikis icin 0): ");
        if (strcmp(wsname, "0") == 0) return;

        if (!workspace_is_valid(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' gecersiz bir alan adi.\n" CLR_RESET, wsname);
            continue;
        }
        if (workspace_is_reserved(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' alani dolu.\n" CLR_RESET, wsname);
            continue;
        }

        /* Alan boş — TC iste */
        char tc[MAX_TC];
        while (1) {
            safe_read(tc, MAX_TC, "  TC Kimlik No (Cikis icin 0): ");
            if (strcmp(tc, "0") == 0) return;
            if (strcmp(tc, session_tc) != 0) {
                printf(CLR_RED "  Hata: Sadece kendi adiniza rezerv yapabilirsiniz! Yeniden girin.\n" CLR_RESET);
            } else {
                break;
            }
        }
        MemberRecord* found_m = member_find(g_members, tc);
        if (!found_m) {
            printf(CLR_RED "  Hata: TC '%s' sistemde kayitli degil.\n" CLR_RESET, tc);
            press_enter_main(); return;
        }

        /* Ad soyad iste */
        char name[MAX_NAME], sn[MAX_NAME];
        safe_read(name, MAX_NAME, "  Ad          : ");
        safe_read(sn,   MAX_NAME, "  Soyad       : ");
        if (strcmp(found_m->name, name)!=0 || strcmp(found_m->surname, sn)!=0) {
            printf(CLR_RED "  Hata: TC No ve Ad/Soyad uyusmuyor.\n" CLR_RESET);
            press_enter_main(); return;
        }

        /* Şifre iste */
        char pw[MAX_PASS];
        safe_read_password(pw, MAX_PASS, "  Sifre       : ");
        if (!member_check_password(found_m, pw)) {
            printf(CLR_RED "  Hata: Sifre yanlis.\n" CLR_RESET);
            press_enter_main(); return;
        }

        char fullname[MAX_NAME*2+2];
        snprintf(fullname, sizeof(fullname), "%s %s", name, sn);
        workspace_reserve(g_workspace, wsname, tc, fullname);
        printf(CLR_GREEN "  Basarili: '%s' alani %s adina rezerve edildi.\n" CLR_RESET, wsname, fullname);
        char logmsg[MAX_LOG_MSG];
        snprintf(logmsg, MAX_LOG_MSG, "Uye calisma alani rezerv: %s -> %s", wsname, fullname);
        log_push(g_log_user, logmsg);
        press_enter_main();
        return;
    }
    (void)session_tc; (void)m;
}

static void uye_ws_teslim(const char* session_tc) {
    if (!devam_sor()) return;

    /* 1. TC doğrulama — sadece kendi TC'si kabul edilir */
    char tc[MAX_TC];
    while (1) {
        safe_read(tc, MAX_TC, "  TC Kimlik No (Cikis icin 0): ");
        if (strcmp(tc, "0") == 0) return;
        if (strcmp(tc, session_tc) != 0) {
            printf(CLR_RED "  Hata: Sadece kendi hesabiniza ait islemleri yapabilirsiniz!\n" CLR_RESET);
            printf(CLR_YELLOW "  Iptal etmek icin 0 girin.\n" CLR_RESET);
        } else {
            break;
        }
    }
    MemberRecord* m = member_find(g_members, tc);
    if (!m) {
        printf(CLR_RED "  Hata: TC numarasi yanlis veya sistemde kayitli degil.\n" CLR_RESET);
        press_enter_main(); return;
    }

    /* FAIL-SAFE: Bu TC'nin aktif rezervasyonu var mı? */
    if (!workspace_member_has_reservation(g_workspace, tc)) {
        printf(CLR_RED "  Hata: Bu TC numarasina ait aktif bir rezervasyon bulunamadi.\n" CLR_RESET);
        press_enter_main(); return;
    }

    /* 2. Ad / Soyad */
    char name[MAX_NAME], sn[MAX_NAME];
    safe_read(name, MAX_NAME, "  Ad          : ");
    safe_read(sn,   MAX_NAME, "  Soyad       : ");
    if (strcmp(m->name, name)!=0 || strcmp(m->surname, sn)!=0) {
        printf(CLR_RED "  Hata: TC No ve Ad/Soyad uyusmuyor.\n" CLR_RESET);
        press_enter_main(); return;
    }

    /* 3. Şifre — 3 hak */
        int deneme = 0;
    while (deneme < 3) {
        char pw[MAX_PASS];
        safe_read_password(pw, MAX_PASS, "  Sifre       : ");
        if (member_check_password(m, pw)) break;
        deneme++;
        if (deneme < 3)
            printf(CLR_RED "  Yanlis sifre! (%d/3)\n" CLR_RESET, deneme);
        else {
            printf(CLR_RED "  3 yanlis deneme! Giris ekranina yonlendiriliyor.\n" CLR_RESET);
            press_enter_main(); return;
        }
    }

    /* 4. Alan adı döngüsü */
    while (1) {
        char wsname[MAX_WSNAME];
        safe_read(wsname, MAX_WSNAME, "  Calisma alani adi (Cikis icin 0): ");
        if (strcmp(wsname, "0") == 0) {
            press_enter_main();
            return;
        }
        if (!workspace_is_valid(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' gecersiz bir alan adi.\n" CLR_RESET, wsname);
            continue;
        }
        if (!workspace_is_reserved(g_workspace, wsname)) {
            printf(CLR_RED "  Hata: '%s' alani zaten bos.\n" CLR_RESET, wsname);
            continue;
        }

        /* FAIL-SAFE: Bu alan gerçekten bu üyeye ait mi? */
        int idx = workspace_find_index(g_workspace, wsname);
        if (idx < 0) { continue; }
        /* workspace_release içinde TC + fullname kontrolü yapılıyor;
           ek olarak burada TC'yi önceden kontrol edelim */
        char fullname[MAX_NAME*2+2];
        snprintf(fullname, sizeof(fullname), "%s %s", name, sn);

        if (workspace_release(g_workspace, wsname, tc, fullname)) {
            printf(CLR_GREEN "  Basarili: '%s' alani bosaltildi.\n" CLR_RESET, wsname);
            char logmsg[MAX_LOG_MSG];
            snprintf(logmsg, MAX_LOG_MSG, "Uye calisma alani teslim: %s <- %s", wsname, fullname);
            log_push(g_log_user, logmsg);
        } else {
            printf(CLR_RED "  Hata: Bu alan size ait degil veya bilgiler uyusmuyor.\n" CLR_RESET);
        }
        press_enter_main(); return;
    }
}

/* ─── Üye ana döngüsü ──────────────────────────── */
static void uye_menu(MemberRecord* m) {
    char ch[4];
    while (1) {
        clear_screen();
        printf("\n  ╔═════════════════════════════════════════════╗\n");
        printf("  ║             UYE SİSTEMİ                     ║\n");
        printf("  ║  Hosgeldiniz, %-28s ║\n", m->name);
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  1) Kitap Odunc Al                          ║\n");
        printf("  ║  2) Kitap Iade Et                           ║\n");
        printf("  ║  3) Kitaplari Sirala ve Listele             ║\n");
        printf("  ║  4) Kitap Ara                               ║\n");
        printf("  ║  5) Uye Sifre Degistir                      ║\n");
        printf("  ║  6) Log Kaydi (Son 10 Islem)                ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  t) Calisma Alanlarini Listele              ║\n");
        printf("  ║  n) Bos Calisma Alanlarini Listele          ║\n");
        printf("  ║  a) Calisma Alani Rezerv Et                 ║\n");
        printf("  ║  d) Calisma Alani Teslimi                   ║\n");
        printf("  ╠═════════════════════════════════════════════╣\n");
        printf("  ║  0) Cikis                                   ║\n");
        printf("  ╚═════════════════════════════════════════════╝\n");
        safe_read(ch, 4, "  Seciminiz: ");

        if      (ch[0]=='1') { uye_kitap_odunc_al(m->tc_no); }
        else if (ch[0]=='2') { uye_kitap_iade_et(m->tc_no); press_enter(); }
        else if (ch[0]=='3') { uye_kitap_sirala_listele(); }
        else if (ch[0]=='4') { uye_kitap_ara(); }
        else if (ch[0]=='5') { uye_sifre_degistir(m); }
        else if (ch[0]=='6') { printf("\n"); log_print(g_log_user); char tmp[4]; safe_read(tmp,4,"  m) Ana Menuye Don: "); }
        /* Çalışma alanı */
        else if (ch[0]=='t'||ch[0]=='T') { uye_ws_listele(); }
        else if (ch[0]=='n'||ch[0]=='N') { uye_ws_bos_listele(); }
        else if (ch[0]=='a'||ch[0]=='A') { uye_ws_rezerv_et(m->tc_no, m); }
        else if (ch[0]=='d'||ch[0]=='D') { uye_ws_teslim(m->tc_no); }
        else if (ch[0]=='0') { save_all(); printf("  Cikis yapiliyor...\n"); return; }
        else { printf("  Gecersiz secim!\n"); press_enter(); }
    }
}

/* ═══════════════════════════════════════════════════
   Giriş menüleri
   ═══════════════════════════════════════════════════ */
static void giris_kutuphaneci(void) {
    int denemeler = 0;
    while (denemeler < 3) {
        char pw[MAX_PASS];
        safe_read_password(pw, MAX_PASS, "  Admin Sifresi: ");

        if (pw[0] == '\0') {
            printf(CLR_RED "  Hata: Sifre bos olamaz.\n" CLR_RESET);
            continue;
        }

        char cand_hash[PASSWORD_HASH_SIZE];
        hash_password(pw, g_cfg.admin_salt, cand_hash);
        if (secure_compare(cand_hash, g_cfg.admin_password, PASSWORD_HASH_SIZE - 1)) {
            printf(CLR_GREEN "  Giris basarili.\n" CLR_RESET);
            admin_menu();
            return;
        }

        denemeler++;
        printf(CLR_RED "  Yanlis sifre! (%d/3)\n" CLR_RESET, denemeler);
    }

    printf(CLR_RED "  3 yanlis deneme! Sistemden cikiliyor.\n" CLR_RESET);
    save_all();
    exit(0);
}

static void giris_uye(void) {
    char tc[MAX_TC];
    safe_read(tc, MAX_TC, "  TC Kimlik No: ");
    MemberRecord* m = member_find(g_members, tc);
    if (!m) {
        printf(CLR_RED "  TC numarasi bulunamadi.\n" CLR_RESET);
        return;
    }

    char name[MAX_NAME], sn[MAX_NAME];
    safe_read(name, MAX_NAME, "  Ad          : ");
    safe_read(sn,   MAX_NAME, "  Soyad       : ");
    if (strcasecmp(m->name, name) != 0 || strcasecmp(m->surname, sn) != 0) {
        printf(CLR_RED "  Ad/Soyad yanlis.\n" CLR_RESET);
        return;
    }

    char pw[MAX_PASS];
    safe_read_password(pw, MAX_PASS, "  Sifre       : ");
    if (pw[0] == '\0') {
        printf(CLR_RED "  Sifre bos olamaz.\n" CLR_RESET);
        return;
    }

    if (!member_auth(g_members, tc, name, sn, pw)) {
        printf(CLR_RED "  Giris basarisiz: TC / Ad / Soyad veya Sifre hatali.\n" CLR_RESET);
        return;
    }

    printf(CLR_GREEN "  Hosgeldiniz, %s %s!\n" CLR_RESET, m->name, m->surname);
    uye_menu(m);
}

/* ═══════════════════════════════════════════════════
   main
   ═══════════════════════════════════════════════════ */
int main(void) {
    system("mkdir -p data");

    g_books      = book_db_create();
    g_members    = member_db_create();
    g_log_admin  = log_db_create();
    g_log_user   = log_db_create();
    g_workspace  = workspace_db_create();

    log_set_audit_path(g_log_admin, "data/audit_admin.log");
    log_set_audit_path(g_log_user,  "data/audit_user.log");

    book_load(g_books,    FILE_BOOKS, FILE_BOOKS_IDX);
    member_load(g_members, FILE_MEMBERS);
    config_load(&g_cfg,   FILE_CONFIG);
    log_load(g_log_admin, FILE_LOG_ADMIN);
    log_load(g_log_user,  FILE_LOG_USER);
    workspace_load(g_workspace, FILE_WORKSPACE);

    if (book_db_count(g_books)      == 0) book_seed(g_books);
    if (member_db_count(g_members)  == 0) member_seed(g_members);
    if (workspace_count(g_workspace)== 0) workspace_seed(g_workspace, g_members);

    /* Workspace bellek bütünlük kontrolü */
    if (!workspace_check_canaries(g_workspace)) {
        printf(CLR_RED "[!] Calisma alani veritabani bozuk! Cikiliyor.\n" CLR_RESET);
        save_all();
        return 1;
    }

    g_loans = loan_db_create(g_books, g_members);
    loan_load(g_loans, FILE_LOANS);

    char ch[4];
    while (1) {
        clear_screen();
        printf("\n  ╔════════════════════════════════════╗\n");
        printf("  ║    KUTUPHANE YÖNETİM SİSTEMİ      ║\n");
        printf("  ╠════════════════════════════════════╣\n");
        printf("  ║  1) Kutuphane Gorevlisi Girisi     ║\n");
        printf("  ║  2) Uye Girisi                     ║\n");
        printf("  ║  0) Cikis                          ║\n");
        printf("  ╚════════════════════════════════════╝\n");
        safe_read(ch, 4, "  Seciminiz: ");

        if      (ch[0]=='1') { giris_kutuphaneci(); }
        else if (ch[0]=='2') { giris_uye(); }
        else if (ch[0]=='0') { save_all(); printf("  Iyi gunler!\n"); break; }
        else { printf("  Gecersiz secim!\n"); press_enter(); }
    }

    loan_db_destroy(g_loans);
    book_db_destroy(g_books);
    member_db_destroy(g_members);
    log_db_destroy(g_log_admin);
    log_db_destroy(g_log_user);
    workspace_db_destroy(g_workspace);
    return 0;
}