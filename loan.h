#ifndef LOAN_H
#define LOAN_H
#include "common.h"
#include "book.h"
#include "member.h"

#define LOAN_LIMIT_DAYS 3
#define LOAN_FINE_PER_DAY 1.0

#pragma pack(push, 1)
typedef struct {
    int  book_id;
    char book_title[MAX_TITLE];
    char book_author[MAX_AUTHOR];
    char member_tc[MAX_TC];
    char member_name[MAX_NAME];
    char member_surname[MAX_NAME];
    char borrowed_at[20];
    bool active;
} LoanRecord;
#pragma pack(pop)

typedef struct LoanDB* LoanDBPtr;

LoanDBPtr loan_db_create(BookDBPtr books, MemberDBPtr members);
void      loan_db_destroy(LoanDBPtr db);

bool loan_borrow(LoanDBPtr db, int book_id, const char* tc);
bool loan_return(LoanDBPtr db, int book_id, int days_held, double* fine_out);

void loan_print_active(LoanDBPtr db);
void loan_print_active_by_member(LoanDBPtr db, const char* tc);

// Üyenin belirli bir kitabı ödünçte mi?
bool loan_member_has(LoanDBPtr db, const char* tc, int book_id);

void loan_save(LoanDBPtr db, const char* path);
void loan_load(LoanDBPtr db, const char* path);

double loan_calc_fine(int days);

#endif