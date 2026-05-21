#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <conio.h>
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define fsync _commit
#define fileno _fileno
#else
#include <termios.h>
#include <unistd.h>
#endif
#include <stdint.h>

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_MEMBERS 1000

/* ── Dosya yolları ──────────────────────────────────── */
#define FILE_BOOKS       "data/kitaplar.bin"
#define FILE_BOOKS_IDX   "data/kitaplar.idx"
#define FILE_MEMBERS     "data/uyeler.bin"
#define FILE_LOANS       "data/odunc.bin"
#define FILE_LOG_ADMIN   "data/log_admin.bin"
#define FILE_LOG_USER    "data/log_user.bin"
#define FILE_CONFIG      "data/config.bin"
#define FILE_WORKSPACE   "data/calisma_alanlari.bin"

/* ── Boyutlar ──────────────────────────────────────── */
#define MAX_TITLE      150
#define MAX_AUTHOR     100
#define MAX_NAME        60
#define MAX_TC          13
#define MAX_PASS        64
#define MAX_LOG_MSG    256
#define LOG_KEEP        10
#define MAX_GENRE       50
#define MAX_WSNAME      32
#define MAX_WORKSPACE   100

/* ── Güvenlik sabitleri ────────────────────────────── */
#define DATA_FILE_MAGIC   0x4C49424BU
#define DATA_FILE_VERSION 2
#define PASSWORD_HASH_SIZE 65

/* ── Kitap durumları ─────────────────────────────────*/
typedef enum {
    BOOK_OK       = 0,
    BOOK_DAMAGED  = 1,
    BOOK_BORROWED = 2
} BookStatus;

/* ── Renk makroları ──────────────────────────────── */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_CYAN    "\033[36m"
#define CLR_MAGENTA "\033[35m"

/* ── Güvenli satır okuma ──────────────────────────── */
static inline void safe_read(char* buf, int size, const char* prompt) {
    if (prompt) { printf("%s", prompt); fflush(stdout); }
    if (fgets(buf, size, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        } else if (len == (size_t)(size - 1)) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
        }
        buf[strcspn(buf, "\r")] = '\0';
    }
}

/* ── Şifre gizli okuma (echo kapalı, yıldız gösterir) */
static inline void safe_read_password(char* buf, int size, const char* prompt) {
    if (prompt) { printf("%s", prompt); fflush(stdout); }
    int len = 0;
    int c;
#ifdef _WIN32
    while ((c = _getch()) != '\r' && c != '\n' && c != EOF) {
        if (c == '\b' || c == 127) { /* backspace */
            if (len > 0) { len--; printf("\b \b"); fflush(stdout); }
        } else if (c == 3) { /* Ctrl+C */
            exit(1);
        } else if (len < size - 1) {
            buf[len++] = (char)c;
            printf("*"); fflush(stdout);
        }
    }
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    while ((c = getchar()) != '\n' && c != EOF) {
        if (c == 127 || c == 8) { /* backspace */
            if (len > 0) { len--; printf("\b \b"); fflush(stdout); }
        } else if (len < size - 1) {
            buf[len++] = (char)c;
            printf("*"); fflush(stdout);
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    buf[len] = '\0';
    printf("\n");
}

/* ── Sadece rakam mı? ─────────────────────────────── */
static inline bool is_digits_only(const char* s) {
    if (!s || !*s) return false;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s)) return false;
    return true;
}

/* ── Zaman damgası ────────────────────────────────── */
static inline void timestamp(char* buf, int size) {
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* ── Güvenlik fonksiyonları ───────────────────────── */
int secure_compare(const void* a, const void* b, size_t n);
void generate_salt(char salt_out[32]);
void hash_password(const char* input, const char* salt, char output[PASSWORD_HASH_SIZE]);
long safe_strtol(const char* text, long min, long max, bool* ok);
bool atomic_write_bytes(const char* path, const void* buffer, size_t size);
bool secure_write_file(const char* path, const void* buffer, size_t size, uint32_t magic, uint8_t version);
void* secure_read_file(const char* path, size_t* out_size, uint32_t expected_magic, uint8_t expected_version);

/* Runtime AES key/IV — config yuklenince set edilir */
void     crypto_set_keys(const uint8_t key[32], const uint8_t iv[16]);
void     crypto_generate_keys(uint8_t key_out[32], uint8_t iv_out[16]);

#endif