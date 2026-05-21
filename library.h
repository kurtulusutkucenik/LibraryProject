#ifndef LIBRARY_H
#define LIBRARY_H
#include "book.h"

typedef BookDBPtr LibraryPtr;

LibraryPtr  library_create(void);
void        library_destroy(LibraryPtr lib);

int         library_add(LibraryPtr lib, const char* author, const char* title, const char* genre);
bool        library_remove(LibraryPtr lib, int id, const char* author, const char* title);
BookRecord* library_search_by_id(LibraryPtr lib, int id);
BookRecord* library_search_by_title(LibraryPtr lib, const char* title);
void        library_search_by_author(LibraryPtr lib, const char* author);

void library_list_by_id(LibraryPtr lib);
void library_list_alpha(LibraryPtr lib);
void library_list_by_genre(LibraryPtr lib);

int  library_next_id(LibraryPtr lib);
bool library_set_status(LibraryPtr lib, int id, BookStatus status);
bool library_undo(LibraryPtr lib);

void library_save(LibraryPtr lib, const char* path, const char* idx_path);
void library_load(LibraryPtr lib, const char* path, const char* idx_path);
void library_seed(LibraryPtr lib);
#endif