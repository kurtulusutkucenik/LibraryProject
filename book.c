#include "book.h"
#include <time.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

/* ── Undo yığını ──────────────────────────────────── */
#define UNDO_STACK_SIZE 10
typedef enum { UNDO_ADD, UNDO_DEL } UndoType;
typedef struct { UndoType type; BookRecord rec; } UndoEntry;

struct BookDB {
    BookNode*  root;
    int        count;
    int        max_id;
    UndoEntry  undo_stack[UNDO_STACK_SIZE];
    int        undo_top;
};

/* ── BST yardımcıları ─────────────────────────────── */
#define BST_STACK_MAX 8192   /* 8192 kitaba kadar guvenli */

static BookNode* node_new(BookRecord* rec) {
    BookNode* n = (BookNode*)calloc(1, sizeof(BookNode));
    if (n) n->data = *rec;
    return n;
}
static BookNode* bst_insert(BookNode* root, BookNode* node) {
    if (!root) return node;
    if (node->data.id < root->data.id) root->left  = bst_insert(root->left,  node);
    else                               root->right = bst_insert(root->right, node);
    return root;
}
static BookNode* bst_find(BookNode* root, int id) {
    if (!root) return NULL;
    if (id == root->data.id) return root;
    return id < root->data.id ? bst_find(root->left, id) : bst_find(root->right, id);
}
static void bst_free(BookNode* n) {
    if (!n) return;
    bst_free(n->left); bst_free(n->right); free(n);
}

/* Guvenli iteratif inorder — dinamik stack */
static void bst_inorder(BookNode* root, void (*visit)(BookRecord*)) {
    if (!root) return;
    BookNode** stk = (BookNode**)malloc(BST_STACK_MAX * sizeof(BookNode*));
    if (!stk) return;
    int sp = 0;
    BookNode* cur = root;
    while (cur || sp > 0) {
        while (cur && sp < BST_STACK_MAX) { stk[sp++] = cur; cur = cur->left; }
        if (sp == 0) break;
        cur = stk[--sp];
        if (!cur->data.deleted) visit(&cur->data);
        cur = cur->right;
    }
    free(stk);
}
/* ── Sıralama için dinamik buffer ────────────────── */
#define BOOK_SORT_MAX BST_STACK_MAX

static int  alpha_cmp (const void* a, const void* b) { return strcmp(((BookRecord*)a)->title,  ((BookRecord*)b)->title); }
static int  genre_cmp (const void* a, const void* b) {
    int c = strcmp(((BookRecord*)a)->genre, ((BookRecord*)b)->genre);
    return c ? c : strcmp(((BookRecord*)a)->title, ((BookRecord*)b)->title);
}

/* collect callback için thread-local olmayan basit struct */
typedef struct { BookRecord* buf; int cnt; int cap; } AlphaBuf;
static AlphaBuf g_alpha = {NULL, 0, 0};
static void alpha_collect(BookRecord* r) {
    if (g_alpha.cnt < g_alpha.cap) g_alpha.buf[g_alpha.cnt++] = *r;
}


/* ── Oluştur / Yok et ─────────────────────────────── */
BookDBPtr book_db_create(void) {
    BookDBPtr db = (BookDBPtr)calloc(1, sizeof(struct BookDB));
    if (db) db->undo_top = -1;
    return db;
}
void book_db_destroy(BookDBPtr db) { if (!db) return; bst_free(db->root); free(db); }
int  book_next_id(BookDBPtr db) { return db ? db->max_id + 1 : 1; }

/* ── Ekle ─────────────────────────────────────────── */
int book_add(BookDBPtr db, const char* author, const char* title, const char* genre) {
    if (!db) return -1;
    BookRecord rec = {0};
    rec.id = book_next_id(db);
    strncpy(rec.author, author, MAX_AUTHOR - 1);
    strncpy(rec.title,  title,  MAX_TITLE  - 1);
    strncpy(rec.genre,  genre ? genre : "Genel", MAX_GENRE - 1);
    rec.deleted = false;
    /* Kitap durumu: /dev/urandom'dan 1 byte ile %5 hasar olasiligi */
    {
        uint8_t rnd = 0;
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd >= 0) { read(fd, &rnd, 1); close(fd); }
        else         { rnd = (uint8_t)(time(NULL) ^ rec.id); }
        rec.status = (rnd < 13) ? BOOK_DAMAGED : BOOK_OK;  /* 13/256 ≈ %5 */
    }
    timestamp(rec.created_at, 20);

    BookNode* node = node_new(&rec);
    db->root = bst_insert(db->root, node);
    db->count++;
    if (rec.id > db->max_id) db->max_id = rec.id;

    int top = (db->undo_top + 1) % UNDO_STACK_SIZE;
    db->undo_stack[top] = (UndoEntry){ UNDO_ADD, rec };
    db->undo_top = top;
    return rec.id;
}

/* ── Sil ──────────────────────────────────────────── */
bool book_delete(BookDBPtr db, int id, const char* author, const char* title) {
    if (!db) return false;
    BookNode* node = bst_find(db->root, id);
    if (!node || node->data.deleted) return false;
    if (strcmp(node->data.author, author) != 0) return false;
    if (strcmp(node->data.title,  title)  != 0) return false;
    if (node->data.status == BOOK_BORROWED) {
        printf(CLR_RED "  Hata: Kitap su an oduncte, silinemez.\n" CLR_RESET);
        return false;
    }
    int top = (db->undo_top + 1) % UNDO_STACK_SIZE;
    db->undo_stack[top] = (UndoEntry){ UNDO_DEL, node->data };
    db->undo_top = top;
    node->data.deleted = true;
    db->count--;
    return true;
}

/* ── Bul ──────────────────────────────────────────── */
BookRecord* book_find_by_id(BookDBPtr db, int id) {
    if (!db) return NULL;
    BookNode* n = bst_find(db->root, id);
    return (n && !n->data.deleted) ? &n->data : NULL;
}
BookRecord* book_find_by_title(BookDBPtr db, const char* title) {
    if (!db) return NULL;
    BookNode** stack = (BookNode**)malloc(BST_STACK_MAX * sizeof(BookNode*));
    if (!stack) return NULL;
    int sp = 0;
    BookNode* cur = db->root;
    BookRecord* result = NULL;
    while (cur || sp > 0) {
        while (cur && sp < BST_STACK_MAX) { stack[sp++] = cur; cur = cur->left; }
        if (sp == 0) break;
        cur = stack[--sp];
        if (!cur->data.deleted && strcmp(cur->data.title, title) == 0) {
            result = &cur->data;
            break;
        }
        cur = cur->right;
    }
    free(stack);
    return result;
}
void book_find_by_author(BookDBPtr db, const char* author) {
    if (!db) return;
    BookNode** stack = (BookNode**)malloc(BST_STACK_MAX * sizeof(BookNode*));
    if (!stack) return;
    int sp = 0, found = 0;
    BookNode* cur = db->root;
    printf("\n  +------+--------------------------------+--------------------------------+----------------+----------+\n");
    printf("  | %-4s | %-30s | %-30s | %-14s | %-8s |\n","ID","YAZAR","KITAP ADI","TUR","DURUM");
    printf("  +------+--------------------------------+--------------------------------+----------------+----------+\n");
    while (cur || sp > 0) {
        while (cur && sp < BST_STACK_MAX) { stack[sp++] = cur; cur = cur->left; }
        if (sp == 0) break;
        cur = stack[--sp];
        if (!cur->data.deleted && strcmp(cur->data.author, author) == 0) {
            const char* d = cur->data.status==BOOK_BORROWED?"Oduncte":
                            cur->data.status==BOOK_DAMAGED ?"Hasarli":"Rafta";
            printf("  | %-4d | %-30.30s | %-30.30s | %-14.14s | %-8s |\n",
                cur->data.id, cur->data.author, cur->data.title, cur->data.genre, d);
            found++;
        }
        cur = cur->right;
    }
    free(stack);
    if (!found) printf("  | Bu yazara ait kitap bulunamadi.                                                                              |\n");
    printf("  +------+--------------------------------+--------------------------------+----------------+----------+\n\n");
}

/* ── Listeleme başlık / altbilgi ─────────────────── */
static void print_header(void) {
    printf("\n  +------+--------------------------------+--------------------------------+----------------+-------------------+\n");
    printf("  | %-4s | %-30s | %-30s | %-14s | %-17s |\n","ID","YAZAR","KITAP ADI","TUR","DURUM");
    printf("  +------+--------------------------------+--------------------------------+----------------+-------------------+\n");
}
static void print_footer(int cnt) {
    printf("  +------+--------------------------------+--------------------------------+----------------+-------------------+\n");
    printf("  Toplam: %d kitap\n\n", cnt);
}
static void print_record(BookRecord* r) {
    const char* durum;
    if (r->status == BOOK_BORROWED) durum = CLR_YELLOW "Oduncte" CLR_RESET;
    else if (r->status == BOOK_DAMAGED)  durum = CLR_RED "Hasarli" CLR_RESET;
    else                                 durum = CLR_GREEN "Rafta" CLR_RESET;
    printf("  | %-4d | %-30.30s | %-30.30s | %-14.14s | %-26s |\n",
        r->id, r->author, r->title, r->genre, durum);
}

void book_list_by_id(BookDBPtr db) {
    if (!db) return;
    print_header();
    int cnt = 0;
    BookNode** stack = (BookNode**)malloc(BST_STACK_MAX * sizeof(BookNode*));
    if (!stack) return;
    int sp = 0;
    BookNode* cur = db->root;
    while (cur || sp > 0) {
        while (cur && sp < BST_STACK_MAX) { stack[sp++] = cur; cur = cur->left; }
        if (sp == 0) break;
        cur = stack[--sp];
        if (!cur->data.deleted) { print_record(&cur->data); cnt++; }
        cur = cur->right;
    }
    free(stack);
    print_footer(cnt);
}

void book_list_alpha(BookDBPtr db) {
    if (!db) return;
    g_alpha.buf = (BookRecord*)malloc(BOOK_SORT_MAX * sizeof(BookRecord));
    if (!g_alpha.buf) return;
    g_alpha.cnt = 0; g_alpha.cap = BOOK_SORT_MAX;
    bst_inorder(db->root, alpha_collect);
    qsort(g_alpha.buf, (size_t)g_alpha.cnt, sizeof(BookRecord), alpha_cmp);
    print_header();
    for (int i = 0; i < g_alpha.cnt; i++) print_record(&g_alpha.buf[i]);
    print_footer(g_alpha.cnt);
    free(g_alpha.buf); g_alpha.buf = NULL;
}

void book_list_by_genre(BookDBPtr db) {
    if (!db) return;
    g_alpha.buf = (BookRecord*)malloc(BOOK_SORT_MAX * sizeof(BookRecord));
    if (!g_alpha.buf) return;
    g_alpha.cnt = 0; g_alpha.cap = BOOK_SORT_MAX;
    bst_inorder(db->root, alpha_collect);
    qsort(g_alpha.buf, (size_t)g_alpha.cnt, sizeof(BookRecord), genre_cmp);
    print_header();
    const char* last_genre = "";
    for (int i = 0; i < g_alpha.cnt; i++) {
        if (strcmp(g_alpha.buf[i].genre, last_genre) != 0) {
            printf("  |" CLR_CYAN " >> TUR: %-118.118s" CLR_RESET "|\n", g_alpha.buf[i].genre);
            last_genre = g_alpha.buf[i].genre;
        }
        print_record(&g_alpha.buf[i]);
    }
    print_footer(g_alpha.cnt);
    free(g_alpha.buf); g_alpha.buf = NULL;
}

/* ── Durum ────────────────────────────────────────── */
bool book_set_status(BookDBPtr db, int id, BookStatus s) {
    BookNode* n = bst_find(db->root, id);
    if (!n || n->data.deleted) return false;
    n->data.status = s; return true;
}

/* ── Undo ─────────────────────────────────────────── */
bool book_undo(BookDBPtr db) {
    if (!db || db->undo_top < 0) { printf("  Geri alinacak islem yok.\n"); return false; }
    UndoEntry* e = &db->undo_stack[db->undo_top];
    db->undo_top = (db->undo_top - 1 + UNDO_STACK_SIZE) % UNDO_STACK_SIZE;
    if (e->type == UNDO_ADD) {
        BookNode* n = bst_find(db->root, e->rec.id);
        if (n) { n->data.deleted = true; db->count--; }
        printf("  Geri alindi: '%s' ekleme islemi iptal edildi.\n", e->rec.title);
    } else {
        BookNode* n = bst_find(db->root, e->rec.id);
        if (n) { n->data.deleted = false; db->count++; }
        printf("  Geri alindi: '%s' silme islemi iptal edildi.\n", e->rec.title);
    }
    return true;
}

/* ── Kayıt ────────────────────────────────────────── */
void book_save(BookDBPtr db, const char* path, const char* idx_path) {
    if (!db) return;

    /* Dinamik save buffer — BST_STACK_MAX kitaba kadar */
    BookRecord* save_buf = (BookRecord*)malloc(BST_STACK_MAX * sizeof(BookRecord));
    if (!save_buf) return;
    int save_cnt = 0;

    BookNode** stack = (BookNode**)malloc(BST_STACK_MAX * sizeof(BookNode*));
    if (!stack) { free(save_buf); return; }
    int sp = 0;
    BookNode* cur = db->root;
    while (cur || sp > 0) {
        while (cur && sp < BST_STACK_MAX) { stack[sp++] = cur; cur = cur->left; }
        if (sp == 0) break;
        cur = stack[--sp];
        if (save_cnt < BST_STACK_MAX) save_buf[save_cnt++] = cur->data;
        cur = cur->right;
    }
    free(stack);

    size_t buf_sz = sizeof(int) + (size_t)save_cnt * sizeof(BookRecord);
    uint8_t* buf = malloc(buf_sz);
    if (buf) {
        memcpy(buf, &save_cnt, sizeof(int));
        for (int i = 0; i < save_cnt; i++)
            memcpy(buf + sizeof(int) + (size_t)i * sizeof(BookRecord), &save_buf[i], sizeof(BookRecord));
        secure_write_file(path, buf, buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
        free(buf);
    }

    size_t idx_buf_sz = sizeof(int) + (size_t)save_cnt * sizeof(BookIndex);
    uint8_t* idx_buf = malloc(idx_buf_sz);
    if (idx_buf) {
        memcpy(idx_buf, &save_cnt, sizeof(int));
        for (int i = 0; i < save_cnt; i++) {
            BookIndex idx = { save_buf[i].id, (long)(sizeof(int) + (size_t)i * sizeof(BookRecord)) };
            memcpy(idx_buf + sizeof(int) + (size_t)i * sizeof(BookIndex), &idx, sizeof(BookIndex));
        }
        secure_write_file(idx_path, idx_buf, idx_buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
        free(idx_buf);
    }
    free(save_buf);
}

void book_load(BookDBPtr db, const char* path, const char* idx_path) {
    (void)idx_path;
    if (!db) return;
    
    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    if (!data) return;
    
    if (out_sz < sizeof(int)) { free(data); return; }
    int cnt = 0; memcpy(&cnt, data, sizeof(int));
    if (cnt < 0 || cnt > BST_STACK_MAX || out_sz != sizeof(int) + (size_t)cnt * sizeof(BookRecord)) { free(data); return; }
    
    int offset = sizeof(int);
    for (int i = 0; i < cnt; i++) {
        BookRecord rec;
        memcpy(&rec, (uint8_t*)data + offset, sizeof(BookRecord));
        offset += sizeof(BookRecord);
        
        // Tainted data sanitization
        rec.author[MAX_AUTHOR - 1] = '\0';
        rec.title[MAX_TITLE - 1] = '\0';
        rec.genre[MAX_GENRE - 1] = '\0';
        
        BookNode* node = node_new(&rec);
        db->root = bst_insert(db->root, node);
        if (!rec.deleted) db->count++;
        if (rec.id > db->max_id) db->max_id = rec.id;
    }
    
    free(data);
}

/* ── Seed (40 kitap, türlü) ───────────────────────── */
void book_seed(BookDBPtr db) {
    struct { const char* author; const char* title; const char* genre; } books[] = {
        /* Roman */
        {"Orhan Pamuk",            "Kar",                           "Roman"},
        {"Orhan Pamuk",            "Benim Adim Kirmizi",            "Roman"},
        {"Orhan Pamuk",            "Istanbul",                      "Roman"},
        {"Orhan Pamuk",            "Masumiyet Muzesi",              "Roman"},
        {"Sabahattin Ali",         "Icimizde Seytan",               "Roman"},
        {"Sabahattin Ali",         "Kuyucakli Yusuf",               "Roman"},
        {"Sabahattin Ali",         "Kuerck Mantolu Madonna",        "Roman"},
        {"Dostoevski",             "Sucve Ceza",                    "Roman"},
        {"Dostoevski",             "Karamazov Kardesler",           "Roman"},
        {"Dostoevski",             "Budala",                        "Roman"},
        {"Leo Tolstoy",            "Savas ve Baris",                "Roman"},
        {"Leo Tolstoy",            "Anna Karenina",                 "Roman"},
        {"Gabriel Garcia Marquez", "Yuz Yil Yalnizligi",           "Roman"},
        {"Gabriel Garcia Marquez", "Ask ve Olum Senfonisi",         "Roman"},
        {"Ahmet Hamdi Tanpinar",   "Huzur",                        "Roman"},
        {"Ahmet Hamdi Tanpinar",   "Saatleri Ayarlama Enstitusu",  "Roman"},
        /* Distopya */
        {"George Orwell",          "1984",                         "Distopya"},
        {"George Orwell",          "Hayvan Ciftligi",              "Distopya"},
        {"Aldous Huxley",          "Cesur Yeni Dunya",             "Distopya"},
        /* Varoluşçuluk / Felsefe */
        {"Albert Camus",           "Yabanci",                      "Felsefe"},
        {"Albert Camus",           "Veba",                         "Felsefe"},
        {"Jean-Paul Sartre",       "Bulanti",                      "Felsefe"},
        {"Friedrich Nietzsche",    "Zerdust Boyle Dedi",           "Felsefe"},
        /* Fantastik / Bilim Kurgu */
        {"Franz Kafka",            "Donusum",                      "Fantastik"},
        {"Franz Kafka",            "Dava",                         "Fantastik"},
        {"J.R.R. Tolkien",         "Yuzuklerin Efendisi",          "Fantastik"},
        {"Frank Herbert",          "Dune",                         "Bilim Kurgu"},
        {"Isaac Asimov",           "Vakif",                        "Bilim Kurgu"},
        /* Tarih */
        {"Yuval Noah Harari",      "Sapiens",                      "Tarih"},
        {"Yuval Noah Harari",      "Homo Deus",                    "Tarih"},
        {"Haluk Sahin",            "Osmanli Tarihi",               "Tarih"},
        /* Psikoloji */
        {"Sigmund Freud",          "Ruya Yorumu",                  "Psikoloji"},
        {"Viktor Frankl",          "Insanin Anlam Arayisi",        "Psikoloji"},
        {"Carl Jung",              "Analitik Psikoloji",           "Psikoloji"},
        /* Siir */
        {"Nazim Hikmet",           "Memleketimden Insan Manzaralari","Siir"},
        {"Cemal Sureya",           "Uzak",                         "Siir"},
        /* Dedektif */
        {"Agatha Christie",        "Ve Sonra Hic Kalmadi",         "Dedektif"},
        {"Agatha Christie",        "Nil Uzerinde Olum",            "Dedektif"},
        /* Macera */
        {"Jules Verne",            "Dunya Etrafinda 80 Gunde",     "Macera"},
        {"Alexandre Dumas",        "Monte Cristo Kontu",           "Macera"},
    };
    int n = (int)(sizeof(books)/sizeof(books[0]));
    for (int i = 0; i < n; i++)
        book_add(db, books[i].author, books[i].title, books[i].genre);
}

int book_db_count(BookDBPtr db) { return db ? db->count : 0; }