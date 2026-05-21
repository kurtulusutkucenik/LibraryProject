#include "config.h"

/* Config dosyasi sifreleme icin sabit bir bootstrap key kullanir.
   Diger tum dosyalar config icerisindeki runtime key ile sifreli. */
static const uint8_t CFG_BOOT_KEY[32] = {
    0xC0,0xFF,0xEE,0x01,0xDE,0xAD,0xBE,0xEF,
    0xFE,0xED,0xFA,0xCE,0xCA,0xFE,0xBA,0xBE,
    0x13,0x37,0x42,0x69,0xAB,0xCD,0x00,0xFF,
    0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88
};
static const uint8_t CFG_BOOT_IV[16] = {
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF
};

void config_save(const Config* cfg, const char* path) {
    /* Config dosyasini bootstrap key ile sifrele */
    crypto_set_keys(CFG_BOOT_KEY, CFG_BOOT_IV);
    secure_write_file(path, cfg, sizeof(Config), 0x434F4E46, 2);
}

void config_load(Config* cfg, const char* path) {
    /* Once bootstrap key ile config'i oku */
    crypto_set_keys(CFG_BOOT_KEY, CFG_BOOT_IV);

    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, 0x434F4E46, 2);
    if (!data || out_sz != sizeof(Config)) {
        if (data) free(data);

        printf(CLR_YELLOW "\n[!] Config dosyasi (%s) bulunamadi veya bozuk.\n" CLR_RESET, path);
        printf(CLR_CYAN "Ilk kurulum: Lutfen sistem yoneticisi sifrenizi belirleyin.\n" CLR_RESET);

        char new_pass[128], new_pass2[128];
        do {
            safe_read_password(new_pass,  sizeof(new_pass),  "Yeni Admin Sifresi: ");
            safe_read_password(new_pass2, sizeof(new_pass2), "Tekrar: ");
            if (strcmp(new_pass, new_pass2) != 0)
                printf(CLR_RED "Sifreler eslesmiyor, tekrar deneyin.\n" CLR_RESET);
        } while (strcmp(new_pass, new_pass2) != 0 || strlen(new_pass) == 0);

        generate_salt(cfg->admin_salt);
        hash_password(new_pass, cfg->admin_salt, cfg->admin_password);

        /* Runtime AES key/IV uret ve config'e kaydet */
        crypto_generate_keys(cfg->db_key, cfg->db_iv);
        cfg->key_init = 0xA5;

        printf(CLR_GREEN "Sistem yoneticisi sifresi basariyla ayarlandi!\n" CLR_RESET);
        config_save(cfg, path);

        /* Artik runtime key aktif */
        crypto_set_keys(cfg->db_key, cfg->db_iv);
        return;
    }

    memcpy(cfg, data, sizeof(Config));
    free(data);

    /* Key gecerli mi kontrol et */
    if (cfg->key_init != 0xA5) {
        printf(CLR_RED "[!] Config icindeki sifrelemme anahtari gecersiz, yeniden uretiliyor...\n" CLR_RESET);
        crypto_generate_keys(cfg->db_key, cfg->db_iv);
        cfg->key_init = 0xA5;
        config_save(cfg, path);
    }

    /* Runtime key'i aktif et — artik diger dosyalar bu key ile okunacak */
    crypto_set_keys(cfg->db_key, cfg->db_iv);
}