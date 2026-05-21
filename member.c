#include "member.h"
#include "common.h"

// Bağlı liste (üye sayısı az, BST gerekmez)
typedef struct MNode {
    MemberRecord   data;
    struct MNode*  next;
} MNode;

struct MemberDB {
    MNode* head;
    int    count;
};

MemberDBPtr member_db_create(void) {
    MemberDBPtr db = (MemberDBPtr)calloc(1, sizeof(struct MemberDB));
    return db;
}

void member_db_destroy(MemberDBPtr db) {
    if (!db) return;
    MNode* c = db->head;
    while (c) { MNode* n = c->next; free(c); c = n; }
    free(db);
}

static void hash_and_store_password(MemberRecord* m, const char* password) {
    generate_salt(m->salt);
    hash_password(password, m->salt, m->password_hash);
}

bool member_check_password(const MemberRecord* m, const char* password) {
    if (!m || !password || password[0] == '\0') return false;
    char candidate[MEMBER_PASSWORD_HASH_SIZE];
    hash_password(password, m->salt, candidate);
    return secure_compare(candidate, m->password_hash, MEMBER_PASSWORD_HASH_SIZE - 1);
}

void member_set_password(MemberRecord* m, const char* password) {
    if (!m) return;
    hash_and_store_password(m, password);
}

int member_add(MemberDBPtr db, const char* tc, const char* name,
               const char* surname, const char* password) {
    if (!db || !tc || !name || !surname || !password) return 0;
    if (strlen(tc) != 11 || !is_digits_only(tc)) {
        printf(CLR_RED "Hata: TC 11 haneli rakam olmali.\n" CLR_RESET);
        return 0;
    }
    if (member_find(db, tc)) {
        printf(CLR_RED "Hata: Bu TC zaten kayitli.\n" CLR_RESET);
        return 0;
    }
    MNode* node = (MNode*)calloc(1, sizeof(MNode));
    if (!node) return 0;
    node->data.id = db->count + 1;
    strncpy(node->data.tc_no, tc, MAX_TC - 1);
    node->data.tc_no[MAX_TC - 1] = '\0';
    strncpy(node->data.name, name, MAX_NAME - 1);
    node->data.name[MAX_NAME - 1] = '\0';
    strncpy(node->data.surname, surname, MAX_NAME - 1);
    node->data.surname[MAX_NAME - 1] = '\0';
    node->data.deleted = false;
    hash_and_store_password(&node->data, password);
    node->next = db->head;
    db->head = node;
    db->count++;
    return 1;
}

bool member_delete(MemberDBPtr db, const char* tc,
                   const char* name, const char* surname) {
    if (!db) return false;
    for (MNode* c = db->head; c; c = c->next) {
        if (c->data.deleted) continue;
        if (strcmp(c->data.tc_no, tc) == 0 &&
            strcmp(c->data.name, name) == 0 &&
            strcmp(c->data.surname, surname) == 0) {
            c->data.deleted = true;
            db->count--;
            return true;
        }
    }
    return false;
}

MemberRecord* member_find(MemberDBPtr db, const char* tc) {
    if (!db || !tc) return NULL;
    for (MNode* c = db->head; c; c = c->next)
        if (!c->data.deleted && strcmp(c->data.tc_no, tc) == 0)
            return &c->data;
    return NULL;
}

MemberRecord* member_find_full(MemberDBPtr db, const char* tc,
                               const char* name, const char* surname) {
    MemberRecord* m = member_find(db, tc);
    if (!m) return NULL;
    if (strcmp(m->name, name) != 0) return NULL;
    if (strcmp(m->surname, surname) != 0) return NULL;
    return m;
}

bool member_auth(MemberDBPtr db, const char* tc, const char* name,
                 const char* surname, const char* password) {
    MemberRecord* m = member_find_full(db, tc, name, surname);
    if (!m) return false;
    return member_check_password(m, password);
}

bool member_change_pass(MemberDBPtr db, const char* tc,
                        const char* old_pass, const char* new_pass) {
    MemberRecord* m = member_find(db, tc);
    if (!m) return false;
    if (!member_check_password(m, old_pass)) return false;
    member_set_password(m, new_pass);
    return true;
}

// TC'ye göre basit insertion sort
void member_list_sorted(MemberDBPtr db) {
    if (!db || !db->head) {
        printf("  Uye listesi bos.\n");
        return;
    }

    MemberRecord* arr = (MemberRecord*)malloc(MAX_MEMBERS * sizeof(MemberRecord));
    if (!arr) { printf("  Bellek hatasi.\n"); return; }
    int cnt = 0;
    for (MNode* c = db->head; c; c = c->next)
        if (!c->data.deleted && cnt < MAX_MEMBERS)
            arr[cnt++] = c->data;

    for (int i = 1; i < cnt; i++) {
        MemberRecord key = arr[i];
        int j = i - 1;
        while (j >= 0 && strcmp(arr[j].tc_no, key.tc_no) > 0) {
            arr[j+1] = arr[j];
            j--;
        }
        arr[j+1] = key;
    }

    printf("\n  +-----+---------------------------------------------+---------------------------------------------+-------+----------+\n");
    printf("  | ID  | AD                                          | SOYAD                                       | TC NO | SIFRE    |\n");
    printf("  +-----+---------------------------------------------+---------------------------------------------+-------+----------+\n");
    for (int i = 0; i < cnt; i++) {
        printf("  | %-3d | %-43.43s | %-43.43s | %-5.5s | ******* |\n",
               arr[i].id, arr[i].name, arr[i].surname, arr[i].tc_no);
    }
    printf("  +-----+---------------------------------------------+---------------------------------------------+-------+----------+\n");
    printf("  Toplam: %d uye\n\n", cnt);
    free(arr);
}

void member_save(MemberDBPtr db, const char* path) {
    if (!db) return;
    int actual_count = 0;
    for (MNode* c = db->head; c; c = c->next) if (!c->data.deleted) actual_count++;

    size_t buf_sz = sizeof(int) + actual_count * sizeof(MemberRecord);
    uint8_t* buf = malloc(buf_sz);
    if (!buf) return;
    
    memcpy(buf, &actual_count, sizeof(int));
    int offset = sizeof(int);
    
    for (MNode* c = db->head; c; c = c->next) {
        if (!c->data.deleted) {
            memcpy(buf + offset, &c->data, sizeof(MemberRecord));
            offset += sizeof(MemberRecord);
        }
    }

    secure_write_file(path, buf, buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    free(buf);
}

void member_load(MemberDBPtr db, const char* path) {
    if (!db) return;
    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    if (!data) return;
    
    if (out_sz < sizeof(int)) { free(data); return; }
    
    int cnt;
    memcpy(&cnt, data, sizeof(int));
    if (cnt < 0 || cnt > MAX_MEMBERS || out_sz != sizeof(int) + cnt * sizeof(MemberRecord)) {
        free(data); return;
    }
    
    int offset = sizeof(int);
    for (int i = 0; i < cnt; i++) {
        MemberRecord rec;
        memcpy(&rec, (uint8_t*)data + offset, sizeof(MemberRecord));
        offset += sizeof(MemberRecord);
        
        // Tainted data sanitization! (Vulnerabilities 3: Null termination enforcement)
        rec.name[MAX_NAME - 1] = '\0';
        rec.surname[MAX_NAME - 1] = '\0';
        rec.tc_no[MAX_TC - 1] = '\0';
        rec.password_hash[MEMBER_PASSWORD_HASH_SIZE - 1] = '\0';
        rec.salt[31] = '\0';
        
        MNode* node = (MNode*)calloc(1, sizeof(MNode));
        if (!node) break;
        node->data = rec;
        node->next = db->head;
        db->head = node;
        db->count++;
    }
    free(data);
}

void member_seed(MemberDBPtr db) {
    struct { const char* tc; const char* name; const char* sn; const char* pw; } m[] = {
        {"12345678901", "Ahmet", "Yilmaz", "ahmet123"},
        {"23456789012", "Fatma", "Kaya", "fatma456"},
        {"34567890123", "Mehmet", "Demir", "mehmet789"},
        {"45678901234", "Ayse", "Celik", "ayse321"},
        {"56789012345", "Mustafa", "Sahin", "musta00"},
    };
    int n = sizeof(m) / sizeof(m[0]);
    for (int i = 0; i < n; i++)
        member_add(db, m[i].tc, m[i].name, m[i].sn, m[i].pw);
}

int member_db_count(MemberDBPtr db) { return db ? db->count : 0; }