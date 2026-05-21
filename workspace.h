#ifndef WORKSPACE_H
#define WORKSPACE_H
#include "common.h"
#include "member.h"

/* ── Çalışma alanı kaydı ────────────────────────────── */
#define WS_CANARY1_VAL 0xDEADBEEFU
#define WS_CANARY2_VAL 0xCAFEBABEU

#pragma pack(push, 1)
typedef struct {
    char     name[MAX_WSNAME];
    uint32_t canary1;                   /* Bellek bütünlük kontrolü */
    bool     reserved;
    uint32_t canary2;                   /* Bellek bütünlük kontrolü */
    char     member_tc[MAX_TC];
    char     member_fullname[MAX_NAME * 2];
} WorkspaceRecord;
#pragma pack(pop)

typedef struct WorkspaceDB* WorkspaceDBPtr;

WorkspaceDBPtr workspace_db_create(void);
void           workspace_db_destroy(WorkspaceDBPtr db);

/* Seed: T1..T20 oluştur, bazıları dolu */
void workspace_seed(WorkspaceDBPtr db, MemberDBPtr members);

/* Sorgular */
int  workspace_find_index(WorkspaceDBPtr db, const char* name); /* -1 = yok */
bool workspace_is_valid(WorkspaceDBPtr db, const char* name);
bool workspace_is_reserved(WorkspaceDBPtr db, const char* name);

/* İşlemler */
bool workspace_reserve(WorkspaceDBPtr db, const char* name,
                       const char* tc, const char* fullname);
bool workspace_release(WorkspaceDBPtr db, const char* name,
                       const char* tc, const char* fullname);

/* Listeleme */
void workspace_list_all(WorkspaceDBPtr db);           /* admin: tüm detay */
void workspace_list_all_user(WorkspaceDBPtr db);      /* üye: REZERV/Boş  */
void workspace_list_empty(WorkspaceDBPtr db);
void workspace_list_reserved(WorkspaceDBPtr db);      /* admin only */

/* Kayıt */
void workspace_save(WorkspaceDBPtr db, const char* path);
void workspace_load(WorkspaceDBPtr db, const char* path);

int workspace_count(WorkspaceDBPtr db);
bool workspace_member_has_reservation(WorkspaceDBPtr db, const char* tc);

/* Canary bütünlük kontrolü — false dönerse bellek bozulması var */
bool workspace_check_canaries(WorkspaceDBPtr db);
#endif