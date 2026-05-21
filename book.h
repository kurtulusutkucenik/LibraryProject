#ifndef BOOK_H
#define BOOK_H
#include "common.h"

/* ── Binary kayıt ─────────────────────────────────── */
#pragma pack(push, 1)
typedef struct {
    int        id;
    char       author[MAX_AUTHOR];
    char       title[MAX_TITLE];
    char       genre[MAX_GENRE];   /* TÜR alanı */
    BookStatus status;
    char       created_at[20];
    bool       deleted;
} BookRecord;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    int  id;
    long offset;
} BookIndex;
#pragma pack(pop)

typedef struct BookNode {
    BookRecord       data;
    struct BookNode* left;
    struct BookNode* right;
} BookNode;

typedef struct BookDB* BookDBPtr;

BookDBPtr   book_db_create(void);
void        book_db_destroy(BookDBPtr db);

int         book_add(BookDBPtr db, const char* author, const char* title, const char* genre);
bool        book_delete(BookDBPtr db, int id, const char* author, const char* title);
BookRecord* book_find_by_id(BookDBPtr db, int id);
BookRecord* book_find_by_title(BookDBPtr db, const char* title);
void        book_find_by_author(BookDBPtr db, const char* author);

void  book_list_by_id(BookDBPtr db);
void  book_list_alpha(BookDBPtr db);
void  book_list_by_genre(BookDBPtr db);   /* TÜR SIRALI */

bool  book_set_status(BookDBPtr db, int id, BookStatus s);
int   book_next_id(BookDBPtr db);
bool  book_undo(BookDBPtr db);

void  book_save(BookDBPtr db, const char* path, const char* idx_path);
void  book_load(BookDBPtr db, const char* path, const char* idx_path);
void  book_seed(BookDBPtr db);

int   book_db_count(BookDBPtr db);
#endif