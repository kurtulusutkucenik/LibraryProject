#ifndef CONFIG_H
#define CONFIG_H
#include "common.h"

#pragma pack(push, 1)
typedef struct {
    char    admin_password[PASSWORD_HASH_SIZE];
    char    admin_salt[32];
    uint8_t db_key[32];   /* AES-256 key — ilk kurulumda /dev/urandom'dan uretilir */
    uint8_t db_iv[16];    /* AES CTR base IV — ilk kurulumda /dev/urandom'dan uretilir */
    uint8_t key_init;     /* 0xA5 ise key/iv hazir */
} Config;
#pragma pack(pop)

void config_save(const Config* cfg, const char* path);
void config_load(Config* cfg, const char* path);

#endif