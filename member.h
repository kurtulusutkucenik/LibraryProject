#ifndef MEMBER_H
#define MEMBER_H

#include "common.h"

#define MEMBER_PASSWORD_HASH_SIZE PASSWORD_HASH_SIZE

#pragma pack(push, 1)
typedef struct {
    int    id;
    char   name[MAX_NAME];
    char   surname[MAX_NAME];
    char   tc_no[MAX_TC];
    char   salt[32];
    char   password_hash[MEMBER_PASSWORD_HASH_SIZE];
    bool   deleted;
} MemberRecord;
#pragma pack(pop)

typedef struct MemberDB* MemberDBPtr;

MemberDBPtr member_db_create(void);
void        member_db_destroy(MemberDBPtr db);

// döndürür: 1=ok, 0=hata
int  member_add(MemberDBPtr db, const char* tc, const char* name,
                const char* surname, const char* password);
bool member_delete(MemberDBPtr db, const char* tc,
                   const char* name, const char* surname);
// Kimlik doğrulama: tc + isim + soyisim eşleşmeli
MemberRecord* member_find(MemberDBPtr db, const char* tc);
bool          member_auth(MemberDBPtr db, const char* tc,
                          const char* name, const char* surname,
                          const char* password);
bool          member_change_pass(MemberDBPtr db, const char* tc,
                                 const char* old_pass, const char* new_pass);

bool member_check_password(const MemberRecord* m, const char* password);
void member_set_password(MemberRecord* m, const char* password);

void member_list_sorted(MemberDBPtr db);  // TC'ye göre sıralı
MemberRecord* member_find_full(MemberDBPtr db, const char* tc,
                               const char* name, const char* surname);

void member_save(MemberDBPtr db, const char* path);
void member_load(MemberDBPtr db, const char* path);

// Seed
void member_seed(MemberDBPtr db);

int member_db_count(MemberDBPtr db);

#endif