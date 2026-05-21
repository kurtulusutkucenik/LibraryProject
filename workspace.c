#include "workspace.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#include <unistd.h>
#endif
#include <limits.h>

struct WorkspaceDB {
    WorkspaceRecord slots[MAX_WORKSPACE];
    int             count;
};

WorkspaceDBPtr workspace_db_create(void) {
    WorkspaceDBPtr db = (WorkspaceDBPtr)calloc(1, sizeof(struct WorkspaceDB));
    return db;
}

void workspace_db_destroy(WorkspaceDBPtr db) {
    free(db);
}

/* T1..T20 oluştur; T3, T7, T12, T15, T18 seed üyelerle dolu */
void workspace_seed(WorkspaceDBPtr db, MemberDBPtr members) {
    if (!db || db->count > 0) return;

    struct { const char* tc; const char* full; } seeded[] = {
        {"12345678901", "Ahmet Yilmaz"},
        {"23456789012", "Fatma Kaya"},
        {"34567890123", "Mehmet Demir"},
        {"45678901234", "Ayse Celik"},
        {"56789012345", "Mustafa Sahin"},
    };
    int reserved_slots[] = {3, 7, 12, 15, 18};
    int res_owner[]       = {0, 1, 2,  3,  4 };

    for (int i = 1; i <= 20 && db->count < MAX_WORKSPACE; i++) {
        WorkspaceRecord* r = &db->slots[db->count++];
        snprintf(r->name, MAX_WSNAME, "T%d", i);
        r->canary1   = WS_CANARY1_VAL;
        r->reserved  = false;
        r->canary2   = WS_CANARY2_VAL;
        r->member_tc[0] = '\0';
        r->member_fullname[0] = '\0';

        for (int j = 0; j < 5; j++) {
            if (reserved_slots[j] == i) {
                r->reserved = true;
                strncpy(r->member_tc, seeded[res_owner[j]].tc, MAX_TC - 1);
                r->member_tc[MAX_TC - 1] = '\0';
                strncpy(r->member_fullname, seeded[res_owner[j]].full,
                        sizeof(r->member_fullname) - 1);
                r->member_fullname[sizeof(r->member_fullname) - 1] = '\0';
                break;
            }
        }
    }
    (void)members;
}

int workspace_find_index(WorkspaceDBPtr db, const char* name) {
    if (!db || !name) return -1;
    for (int i = 0; i < db->count; i++)
        if (strcasecmp(db->slots[i].name, name) == 0) return i;
    return -1;
}

bool workspace_is_valid(WorkspaceDBPtr db, const char* name) {
    return workspace_find_index(db, name) >= 0;
}

bool workspace_is_reserved(WorkspaceDBPtr db, const char* name) {
    int idx = workspace_find_index(db, name);
    if (idx < 0) return false;
    return db->slots[idx].reserved;
}

bool workspace_member_has_reservation(WorkspaceDBPtr db, const char* tc) {
    if (!db || !tc) return false;
    for (int i = 0; i < db->count; i++) {
        if (db->slots[i].reserved && strcmp(db->slots[i].member_tc, tc) == 0) {
            return true;
        }
    }
    return false;
}

bool workspace_reserve(WorkspaceDBPtr db, const char* name,
                       const char* tc, const char* fullname) {
    int idx = workspace_find_index(db, name);
    if (idx < 0) return false;
    if (db->slots[idx].reserved) return false;
    if (workspace_member_has_reservation(db, tc)) return false;

    /* Canary kontrolü */
    if (db->slots[idx].canary1 != WS_CANARY1_VAL ||
        db->slots[idx].canary2 != WS_CANARY2_VAL) {
        printf(CLR_RED "[!] GUVENLIK: slot canary bozuk, rezerv islemi iptal edildi!\n" CLR_RESET);
        return false;
    }

    /* Fullname kayit oncesi normalize et (basta/sonda bosluk olmaz) */
    char stored_fullname[MAX_NAME * 2];
    strncpy(stored_fullname, fullname, sizeof(stored_fullname) - 1);
    stored_fullname[sizeof(stored_fullname) - 1] = '\0';

    db->slots[idx].canary1  = WS_CANARY1_VAL;
    db->slots[idx].reserved = true;
    db->slots[idx].canary2  = WS_CANARY2_VAL;
    strncpy(db->slots[idx].member_tc, tc, MAX_TC - 1);
    db->slots[idx].member_tc[MAX_TC - 1] = '\0';
    strncpy(db->slots[idx].member_fullname, stored_fullname,
            sizeof(db->slots[idx].member_fullname) - 1);
    db->slots[idx].member_fullname[sizeof(db->slots[idx].member_fullname) - 1] = '\0';
    return true;
}

bool workspace_release(WorkspaceDBPtr db, const char* name,
                       const char* tc, const char* fullname) {
    int idx = workspace_find_index(db, name);
    if (idx < 0) return false;
    if (!db->slots[idx].reserved) return false;
    if (strcmp(db->slots[idx].member_tc, tc) != 0) return false;
    if (strcasecmp(db->slots[idx].member_fullname, fullname) != 0) return false;

    /* Canary kontrolü */
    if (db->slots[idx].canary1 != WS_CANARY1_VAL ||
        db->slots[idx].canary2 != WS_CANARY2_VAL) {
        printf(CLR_RED "[!] GUVENLIK: slot canary bozuk, teslim islemi iptal edildi!\n" CLR_RESET);
        return false;
    }
    db->slots[idx].reserved = false;
    db->slots[idx].canary1  = WS_CANARY1_VAL;
    db->slots[idx].canary2  = WS_CANARY2_VAL;
    db->slots[idx].member_tc[0] = '\0';
    db->slots[idx].member_fullname[0] = '\0';
    return true;
}

/* Admin: tüm detay */
void workspace_list_all(WorkspaceDBPtr db) {
    if (!db) return;
    printf("\n  +----------+-------------+------------------------------+\n");
    printf("  | %-8s | %-11s | %-28s |\n", "ALAN ADI", "DURUM", "REZERV EDEN");
    printf("  +----------+-------------+------------------------------+\n");
    for (int i = 0; i < db->count; i++) {
        WorkspaceRecord* r = &db->slots[i];
        if (r->reserved) {
            char info[MAX_NAME * 2 + MAX_TC + 4];
            snprintf(info, sizeof(info), "%s %s", r->member_fullname, r->member_tc);
            printf("  | %-8s | " CLR_RED "%-11s" CLR_RESET " | %-28.28s |\n",
                   r->name, "REZERV", info);
        } else {
            printf("  | %-8s | " CLR_GREEN "%-11s" CLR_RESET " |                              |\n",
                   r->name, "Bos");
        }
    }
    printf("  +----------+-------------+------------------------------+\n");
    printf("  Toplam: %d alan\n\n", db->count);
}

/* Üye: sadece REZERV/Boş */
void workspace_list_all_user(WorkspaceDBPtr db) {
    if (!db) return;
    printf("\n  +----------+-------------+\n");
    printf("  | %-8s | %-11s |\n", "ALAN ADI", "DURUM");
    printf("  +----------+-------------+\n");
    for (int i = 0; i < db->count; i++) {
        WorkspaceRecord* r = &db->slots[i];
        if (r->reserved)
            printf("  | %-8s | " CLR_RED "%-11s" CLR_RESET " |\n", r->name, "REZERV");
        else
            printf("  | %-8s | " CLR_GREEN "%-11s" CLR_RESET " |\n", r->name, "Bos");
    }
    printf("  +----------+-------------+\n");
    printf("  Toplam: %d alan\n\n", db->count);
}

void workspace_list_empty(WorkspaceDBPtr db) {
    if (!db) return;
    printf("\n  +----------+\n");
    printf("  | %-8s |\n", "BOS ALAN");
    printf("  +----------+\n");
    int cnt = 0;
    for (int i = 0; i < db->count; i++) {
        if (!db->slots[i].reserved) {
            printf("  | " CLR_GREEN "%-8s" CLR_RESET " |\n", db->slots[i].name);
            cnt++;
        }
    }
    if (!cnt) printf("  | Bos alan yok.   |\n");
    printf("  +----------+\n");
    printf("  Bos alan sayisi: %d\n\n", cnt);
}

void workspace_list_reserved(WorkspaceDBPtr db) {
    if (!db) return;
    printf("\n  +----------+--------------------------+\n");
    printf("  | %-8s | %-24s |\n", "REZERV", "UYE AD-SOYAD");
    printf("  +----------+--------------------------+\n");
    int cnt = 0;
    for (int i = 0; i < db->count; i++) {
        if (db->slots[i].reserved) {
            printf("  | " CLR_RED "%-8s" CLR_RESET " | %-24.24s |\n",
                   db->slots[i].name, db->slots[i].member_fullname);
            cnt++;
        }
    }
    if (!cnt) printf("  | Rezerv alan yok.                 |\n");
    printf("  +----------+--------------------------+\n");
    printf("  Rezerv alan sayisi: %d\n\n", cnt);
}

void workspace_save(WorkspaceDBPtr db, const char* path) {
    if (!db) return;
    size_t buf_sz = sizeof(int) + db->count * sizeof(WorkspaceRecord);
    uint8_t* buf = malloc(buf_sz);
    if (!buf) return;
    memcpy(buf, &db->count, sizeof(int));
    memcpy(buf + sizeof(int), db->slots, db->count * sizeof(WorkspaceRecord));
    
    secure_write_file(path, buf, buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    free(buf);
}

void workspace_load(WorkspaceDBPtr db, const char* path) {
    if (!db) return;
    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    if (!data) return;
    
    if (out_sz < sizeof(int)) { free(data); return; }
    int cnt; memcpy(&cnt, data, sizeof(int));
    if (cnt < 0 || cnt > MAX_WORKSPACE || out_sz != sizeof(int) + cnt * sizeof(WorkspaceRecord)) {
        free(data); return;
    }
    
    memcpy(db->slots, (uint8_t*)data + sizeof(int), cnt * sizeof(WorkspaceRecord));
    db->count = cnt;

    for (int i = 0; i < cnt; i++) {
        db->slots[i].name[MAX_WSNAME - 1] = '\0';
        db->slots[i].member_tc[MAX_TC - 1] = '\0';
        db->slots[i].member_fullname[sizeof(db->slots[i].member_fullname) - 1] = '\0';
        /* Diskten yuklenen kayitlarda canary yoksa yenile */
        if (db->slots[i].canary1 != WS_CANARY1_VAL) db->slots[i].canary1 = WS_CANARY1_VAL;
        if (db->slots[i].canary2 != WS_CANARY2_VAL) db->slots[i].canary2 = WS_CANARY2_VAL;
    }
    free(data);
}

int workspace_count(WorkspaceDBPtr db) {
    return db ? db->count : 0;
}

bool workspace_check_canaries(WorkspaceDBPtr db) {
    if (!db) return false;
    for (int i = 0; i < db->count; i++) {
        if (db->slots[i].canary1 != WS_CANARY1_VAL ||
            db->slots[i].canary2 != WS_CANARY2_VAL) {
            printf(CLR_RED "\n[!] GUVENLIK UYARISI: workspace slot[%d] ('%s') bellek bozulmasi tespit edildi!\n"
                   CLR_RESET, i, db->slots[i].name);
            return false;
        }
    }
    return true;
}