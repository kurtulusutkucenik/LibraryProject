#include "library.h"

LibraryPtr library_create(void)  { return book_db_create(); }
void library_destroy(LibraryPtr lib) { book_db_destroy(lib); }

int library_add(LibraryPtr lib, const char* author, const char* title, const char* genre) {
    return book_add(lib, author, title, genre);
}
bool library_remove(LibraryPtr lib, int id, const char* author, const char* title) {
    return book_delete(lib, id, author, title);
}
BookRecord* library_search_by_id(LibraryPtr lib, int id)          { return book_find_by_id(lib, id); }
BookRecord* library_search_by_title(LibraryPtr lib, const char* t){ return book_find_by_title(lib, t); }
void library_search_by_author(LibraryPtr lib, const char* a)      { book_find_by_author(lib, a); }
void library_list_by_id(LibraryPtr lib)    { book_list_by_id(lib); }
void library_list_alpha(LibraryPtr lib)    { book_list_alpha(lib); }
void library_list_by_genre(LibraryPtr lib) { book_list_by_genre(lib); }
int  library_next_id(LibraryPtr lib)       { return book_next_id(lib); }
bool library_set_status(LibraryPtr lib, int id, BookStatus s) { return book_set_status(lib, id, s); }
bool library_undo(LibraryPtr lib)          { return book_undo(lib); }
void library_save(LibraryPtr lib, const char* p, const char* i) { book_save(lib, p, i); }
void library_load(LibraryPtr lib, const char* p, const char* i) { book_load(lib, p, i); }
void library_seed(LibraryPtr lib) { book_seed(lib); }