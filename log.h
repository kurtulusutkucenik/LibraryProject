#ifndef LOG_H
#define LOG_H
#include "common.h"

#pragma pack(push, 1)
typedef struct {
    char msg[MAX_LOG_MSG];
    char ts[20];
} LogEntry;
#pragma pack(pop)

typedef struct LogDB* LogDBPtr;

LogDBPtr log_db_create(void);
void     log_db_destroy(LogDBPtr db);

void log_push(LogDBPtr db, const char* msg);
void log_print(LogDBPtr db);

void log_save(LogDBPtr db, const char* path);
void log_load(LogDBPtr db, const char* path);

/* Silinemeyen audit log — her push'ta ayri dosyaya eklenir */
void log_set_audit_path(LogDBPtr db, const char* path);

#endif