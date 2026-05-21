#include "loan.h"
#include "common.h"

typedef struct LNode {
    LoanRecord    data;
    struct LNode* next;
} LNode;

struct LoanDB {
    BookDBPtr   books;
    MemberDBPtr members;
    LNode*      head;
    int         count;
};

LoanDBPtr loan_db_create(BookDBPtr books, MemberDBPtr members) {
    LoanDBPtr db = (LoanDBPtr)calloc(1, sizeof(struct LoanDB));
    if (db) { db->books = books; db->members = members; }
    return db;
}
void loan_db_destroy(LoanDBPtr db) {
    if (!db) return;
    LNode* c = db->head;
    while (c) { LNode* n = c->next; free(c); c = n; }
    free(db);
}

double loan_calc_fine(int days) {
    if (days <= LOAN_LIMIT_DAYS) return 0.0;
    return (days - LOAN_LIMIT_DAYS) * LOAN_FINE_PER_DAY;
}

bool loan_borrow(LoanDBPtr db, int book_id, const char* tc) {
    if (!db) return false;

    BookRecord*   book = book_find_by_id(db->books, book_id);
    if (!book) {
        printf(CLR_RED "Hata: Kitap (ID: %d) bulunamadi.\n" CLR_RESET, book_id);
        return false;
    }
    if (book->status == BOOK_BORROWED) {
        printf(CLR_RED "Hata: Kitap (ID: %d) zaten oduncte.\n" CLR_RESET, book_id);
        return false;
    }
    if (book->status == BOOK_DAMAGED) {
        printf(CLR_YELLOW "Uyari: Kitap hasarli durumda. Yine de odunc almak istiyor musunuz? (e/h): " CLR_RESET);
        char ch[4]; safe_read(ch, 4, NULL);
        if (ch[0] != 'e' && ch[0] != 'E') return false;
    }

    MemberRecord* mem = member_find(db->members, tc);
    if (!mem) {
        printf(CLR_RED "Hata: TC '%s' ile uye bulunamadi.\n" CLR_RESET, tc);
        return false;
    }

    LNode* node = (LNode*)calloc(1, sizeof(LNode));
    if (!node) return false;

    node->data.book_id = book_id;
    strncpy(node->data.book_title,    book->title,    MAX_TITLE  - 1);
    strncpy(node->data.book_author,   book->author,   MAX_AUTHOR - 1);
    strncpy(node->data.member_tc,     tc,             MAX_TC     - 1);
    strncpy(node->data.member_name,   mem->name,      MAX_NAME   - 1);
    strncpy(node->data.member_surname,mem->surname,   MAX_NAME   - 1);
    timestamp(node->data.borrowed_at, 20);
    node->data.active = true;

    node->next = db->head;
    db->head   = node;
    db->count++;

    book_set_status(db->books, book_id, BOOK_BORROWED);
    printf(CLR_GREEN "Basarili: '%s' kitabi %s %s adli uyeye odunc verildi.\n" CLR_RESET,
        book->title, mem->name, mem->surname);
    return true;
}

bool loan_return(LoanDBPtr db, int book_id, int days_held, double* fine_out) {
    if (!db) return false;
    for (LNode* c = db->head; c; c = c->next) {
        if (c->data.active && c->data.book_id == book_id) {
            c->data.active = false;
            db->count--;
            book_set_status(db->books, book_id, BOOK_OK);
            double fine = loan_calc_fine(days_held);
            if (fine_out) *fine_out = fine;
            return true;
        }
    }
    printf(CLR_RED "Hata: Kitap (ID: %d) odunc listesinde bulunamadi.\n" CLR_RESET, book_id);
    return false;
}

bool loan_member_has(LoanDBPtr db, const char* tc, int book_id) {
    for (LNode* c = db->head; c; c = c->next)
        if (c->data.active && c->data.book_id == book_id &&
            strcmp(c->data.member_tc, tc) == 0)
            return true;
    return false;
}

void loan_print_active(LoanDBPtr db) {
    if (!db) return;
    printf("\n  +------+--------------------------------+------------------------------+---------------+--------------------+\n");
    printf("  | %-4s | %-30s | %-28s | %-13s | %-18s |\n",
        "ID","KITAP ADI","YAZAR","TC NO","ODUNC ALAN");
    printf("  +------+--------------------------------+------------------------------+---------------+--------------------+\n");
    int cnt = 0;
    for (LNode* c = db->head; c; c = c->next) {
        if (!c->data.active) continue;
        char full[MAX_NAME*2+2];
        snprintf(full, sizeof(full), "%s %s", c->data.member_name, c->data.member_surname);
        printf("  | %-4d | %-30.30s | %-28.28s | %-13s | %-18.18s |\n",
            c->data.book_id, c->data.book_title, c->data.book_author,
            c->data.member_tc, full);
        cnt++;
    }
    if (!cnt)
        printf("  | Su an oduncte kitap bulunmamaktadir.                                                                          |\n");
    printf("  +------+--------------------------------+------------------------------+---------------+--------------------+\n");
    printf("  Toplam aktif odunc: %d\n\n", cnt);
}

void loan_print_active_by_member(LoanDBPtr db, const char* tc) {
    if (!db) return;
    printf("\n--- Odunc Alinan Kitaplariniz ---\n");
    int cnt = 0;
    for (LNode* c = db->head; c; c = c->next) {
        if (!c->data.active || strcmp(c->data.member_tc, tc) != 0) continue;
        printf("  [ID: %-4d] %-30s - %s  (Alinma: %s)\n",
            c->data.book_id, c->data.book_title,
            c->data.book_author, c->data.borrowed_at);
        cnt++;
    }
    if (!cnt) printf("  Oduncte kitabiniz bulunmamaktadir.\n");
    printf("---------------------------------\n\n");
}

void loan_save(LoanDBPtr db, const char* path) {
    if (!db) return;
    
    int actual_count = 0;
    for (LNode* c = db->head; c; c = c->next) if (c->data.active) actual_count++;

    size_t buf_sz = sizeof(int) + actual_count * sizeof(LoanRecord);
    uint8_t* buf = malloc(buf_sz);
    if (!buf) return;
    
    memcpy(buf, &actual_count, sizeof(int));
    int offset = sizeof(int);
    
    for (LNode* c = db->head; c; c = c->next) {
        if (c->data.active) {
            memcpy(buf + offset, &c->data, sizeof(LoanRecord));
            offset += sizeof(LoanRecord);
        }
    }
    secure_write_file(path, buf, buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    free(buf);
}

void loan_load(LoanDBPtr db, const char* path) {
    if (!db) return;
    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    if (!data) return;
    
    if (out_sz < sizeof(int)) { free(data); return; }
    int cnt;
    memcpy(&cnt, data, sizeof(int));
    if (cnt < 0 || cnt > 100000 || out_sz != sizeof(int) + (size_t)cnt * sizeof(LoanRecord)) {
        free(data); return;
    }
    
    int offset = sizeof(int);
    for (int i = 0; i < cnt; i++) {
        LoanRecord rec;
        memcpy(&rec, (uint8_t*)data + offset, sizeof(LoanRecord));
        offset += sizeof(LoanRecord);
        
        // Tainted data sanitization
        rec.book_title[MAX_TITLE - 1] = '\0';
        rec.book_author[MAX_AUTHOR - 1] = '\0';
        rec.member_tc[MAX_TC - 1] = '\0';
        rec.member_name[MAX_NAME - 1] = '\0';
        rec.member_surname[MAX_NAME - 1] = '\0';
        rec.borrowed_at[sizeof(rec.borrowed_at) - 1] = '\0';
        
        LNode* node = (LNode*)calloc(1, sizeof(LNode));
        if (!node) break;
        node->data = rec;
        node->next = db->head;
        db->head   = node;
        if (rec.active) db->count++;
    }
    free(data);
}