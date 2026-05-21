#define _POSIX_C_SOURCE 200809L
#include "common.h"
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "aes.h"

int secure_compare(const void* a, const void* b, size_t n) {
    const unsigned char* p = (const unsigned char*)a;
    const unsigned char* q = (const unsigned char*)b;
    unsigned char diff = 0;
    for (size_t i = 0; i < n; i++) diff |= p[i] ^ q[i];
    return diff == 0;
}

#include "sha256.h"

// Pepper: once KUTUPHANE_PEPPER env degiskeninden okunur,
// bulunamazsa derleme-zamani fallback kullanilir (production'da env set edilmeli)
static const char* get_pepper(void) {
    const char* env = getenv("KUTUPHANE_PEPPER");
    if (env && env[0]) return env;
    return "_OstimSec1995_";  /* fallback — uretimde: export KUTUPHANE_PEPPER=<gizli_deger> */
}

void generate_salt(char salt_out[32]) {
    unsigned char buf[15];
    memset(buf, 0, sizeof(buf));
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t n = read(fd, buf, sizeof(buf));
        close(fd);
        if (n != (ssize_t)sizeof(buf)) {
            /* Kismi okuma — eksik byte'lari time+pid ile doldur */
            for (ssize_t i = n < 0 ? 0 : n; i < (ssize_t)sizeof(buf); i++)
                buf[i] = (unsigned char)(time(NULL) ^ (i * 37));
        }
    } else {
        for (int i = 0; i < (int)sizeof(buf); i++)
            buf[i] = (unsigned char)(time(NULL) ^ (i * 37));
    }
    for (int i = 0; i < (int)sizeof(buf); i++)
        sprintf(salt_out + (i * 2), "%02x", buf[i]);
    salt_out[30] = '\0';
}

void hash_password(const char* input, const char* salt, char output[PASSWORD_HASH_SIZE]) {
    SHA256_CTX ctx;
    uint8_t hash[32];
    const char* pepper = get_pepper();

    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)input, strlen(input));
    if (salt && salt[0])
        sha256_update(&ctx, (const uint8_t*)salt, strlen(salt));
    sha256_update(&ctx, (const uint8_t*)pepper, strlen(pepper));
    sha256_final(&ctx, hash);

    for (int i = 0; i < 32; i++)
        sprintf(output + (i * 2), "%02x", hash[i]);
    output[PASSWORD_HASH_SIZE - 1] = '\0';
}

long safe_strtol(const char* text, long min, long max, bool* ok) {
    char* endptr = NULL;
    errno = 0;
    long value = strtol(text, &endptr, 10);
    if (!ok) return value;
    if (errno == ERANGE || endptr == text || *endptr != '\0' || value < min || value > max) {
        *ok = false;
        return 0;
    }
    *ok = true;
    return value;
}

bool atomic_write_bytes(const char* path, const void* buffer, size_t size) {
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* f = fopen(tmp_path, "wb");
    if (!f) return false;

    if (fwrite(buffer, 1, size, f) != size) {
        fclose(f);
        remove(tmp_path);
        return false;
    }

    fflush(f);
    fsync(fileno(f));
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

// Runtime AES key/IV — config_load sonrası crypto_set_keys ile set edilir
static uint8_t g_db_key[32] = {0};
static uint8_t g_db_iv[16]  = {0};
static bool    g_keys_ready  = false;

void crypto_generate_keys(uint8_t key_out[32], uint8_t iv_out[16]) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) {
        ssize_t rk = read(fd, key_out, 32);
        ssize_t ri = read(fd, iv_out,  16);
        close(fd);
        /* Eksik okuma durumunda kalan byte'lari fallback ile doldur */
        for (ssize_t i = rk < 0 ? 0 : rk; i < 32; i++)
            key_out[i] = (uint8_t)(time(NULL) ^ (i * 31));
        for (ssize_t i = ri < 0 ? 0 : ri; i < 16; i++)
            iv_out[i]  = (uint8_t)(time(NULL) ^ (i * 17));
    } else {
        /* /dev/urandom açılamadı — time tabanlı fallback */
        for (int i = 0; i < 32; i++) key_out[i] = (uint8_t)(time(NULL) ^ (i * 31));
        for (int i = 0; i < 16; i++) iv_out[i]  = (uint8_t)(time(NULL) ^ (i * 17));
    }
}

void crypto_set_keys(const uint8_t key[32], const uint8_t iv[16]) {
    memcpy(g_db_key, key, 32);
    memcpy(g_db_iv,  iv,  16);
    g_keys_ready = true;
}

bool secure_write_file(const char* path, const void* buffer, size_t size, uint32_t magic, uint8_t version) {
    if (!g_keys_ready) {
        printf(CLR_RED "[!] KRITIK: Sifreleme anahtarlari hazir degil!\n" CLR_RESET);
        return false;
    }

    /* Her yazma isleminde taze IV uret — IV reuse eliminasyonu */
    uint8_t file_iv[16];
    {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) {
            ssize_t n = read(fd, file_iv, 16);
            close(fd);
            for (ssize_t i = n < 0 ? 0 : n; i < 16; i++)
                file_iv[i] = (uint8_t)(time(NULL) ^ (i * 53));
        } else {
            for (int i = 0; i < 16; i++)
                file_iv[i] = (uint8_t)(time(NULL) ^ (i * 53));
        }
        /* Base IV ile XOR — tamamen rastgele olmasa bile base'den farkli olur */
        for (int i = 0; i < 16; i++) file_iv[i] ^= g_db_iv[i];
    }

    size_t padded_size = size + 32; /* +32 = SHA-256 */
    uint8_t* enc_buf = malloc(padded_size);
    if (!enc_buf) return false;

    memcpy(enc_buf, buffer, size);

    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)&magic,   sizeof(magic));
    sha256_update(&ctx, (const uint8_t*)&version, sizeof(version));
    sha256_update(&ctx, (const uint8_t*)buffer,   size);
    sha256_final(&ctx, hash);
    memcpy(enc_buf + size, hash, 32);

    AES256_CTR(g_db_key, file_iv, enc_buf, padded_size);

    /* Dosya formatı: [magic(4)] [version(1)] [file_iv(16)] [padded_size(8)] [data] */
    char tmp_path[PATH_MAX];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE* f = fopen(tmp_path, "wb");
    if (!f) { free(enc_buf); return false; }
    fwrite(&magic,       sizeof(magic),       1, f);
    fwrite(&version,     1,                   1, f);
    fwrite(file_iv,      16,                  1, f);   /* IV dosyaya yaziliyor */
    fwrite(&padded_size, sizeof(size_t),      1, f);
    if (fwrite(enc_buf, 1, padded_size, f) != padded_size) {
        fclose(f); remove(tmp_path); free(enc_buf); return false;
    }
    fflush(f); fsync(fileno(f)); fclose(f);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path); free(enc_buf); return false;
    }
    free(enc_buf);
    return true;
}

void* secure_read_file(const char* path, size_t* out_size, uint32_t expected_magic, uint8_t expected_version) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;

    uint32_t magic;
    uint8_t  version;
    uint8_t  file_iv[16];
    size_t   padded_size;

    /* Dosya formatı: [magic(4)] [version(1)] [iv(16)] [padded_size(8)] [data] */
    if (fread(&magic,       sizeof(magic),  1, f) != 1 || magic   != expected_magic  ||
        fread(&version,     1,              1, f) != 1 || version != expected_version ||
        fread(file_iv,      16,             1, f) != 1 ||
        fread(&padded_size, sizeof(size_t), 1, f) != 1) {
        fclose(f); return NULL;
    }

    if (padded_size < 32 || padded_size > 1024 * 1024 * 50) {
        fclose(f); return NULL;
    }

    uint8_t* enc_buf = malloc(padded_size);
    if (!enc_buf) { fclose(f); return NULL; }
    if (fread(enc_buf, 1, padded_size, f) != padded_size) {
        fclose(f); free(enc_buf); return NULL;
    }
    fclose(f);

    AES256_CTR(g_db_key, file_iv, enc_buf, padded_size);

    size_t data_size = padded_size - 32;
    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t*)&magic,   sizeof(magic));
    sha256_update(&ctx, (const uint8_t*)&version, sizeof(version));
    sha256_update(&ctx, enc_buf, data_size);
    sha256_final(&ctx, hash);

    if (memcmp(hash, enc_buf + data_size, 32) != 0) {
        printf(CLR_RED "\n[!] GUVENLIK UYARISI: %s uzerinde degisiklik veya bozulma tespit edildi!\n" CLR_RESET, path);
        free(enc_buf);
        return NULL;
    }

    if (out_size) *out_size = data_size;
    return enc_buf;
}