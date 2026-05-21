#include "log.h"

struct LogDB {
    LogEntry entries[LOG_KEEP];
    int      head;   // bir sonraki yazılacak yer
    int      count;  // 0..LOG_KEEP
    char     audit_path[256]; // append-only audit dosyasi
};

LogDBPtr log_db_create(void) {
    return (LogDBPtr)calloc(1, sizeof(struct LogDB));
}
void log_db_destroy(LogDBPtr db) { free(db); }

void log_set_audit_path(LogDBPtr db, const char* path) {
    if (!db || !path) return;
    strncpy(db->audit_path, path, sizeof(db->audit_path) - 1);
    db->audit_path[sizeof(db->audit_path) - 1] = '\0';
}

static void audit_append(LogDBPtr db, const char* ts, const char* msg) {
    if (!db || !db->audit_path[0]) return;
    FILE* f = fopen(db->audit_path, "a");
    if (!f) return;
    fprintf(f, "[%s] %s\n", ts, msg);
    fflush(f);
    fclose(f);
}

void log_push(LogDBPtr db, const char* msg) {
    if (!db || !msg) return;
    LogEntry* e = &db->entries[db->head];
    strncpy(e->msg, msg, MAX_LOG_MSG - 1);
    e->msg[MAX_LOG_MSG - 1] = '\0';
    timestamp(e->ts, 20);
    db->head = (db->head + 1) % LOG_KEEP;
    if (db->count < LOG_KEEP) db->count++;
    audit_append(db, e->ts, msg);
}

void log_print(LogDBPtr db) {
    if (!db || db->count == 0) {
        printf("  Kayitli islem bulunmamaktadir.\n");
        return;
    }
    printf("\n  --- Son %d Islem ---\n", db->count);
    int start = (db->head - db->count + LOG_KEEP) % LOG_KEEP;
    for (int i = 0; i < db->count; i++) {
        int idx = (start + i) % LOG_KEEP;
        printf("  [%d] %s  >>  %s\n", i+1, db->entries[idx].ts, db->entries[idx].msg);
    }
    printf("\n");
}

void log_save(LogDBPtr db, const char* path) {
    if (!db) return;
    size_t buf_sz = sizeof(int)*2 + sizeof(LogEntry) * LOG_KEEP;
    uint8_t* buf = malloc(buf_sz);
    if (!buf) return;
    
    memcpy(buf, &db->head, sizeof(int));
    memcpy(buf + sizeof(int), &db->count, sizeof(int));
    memcpy(buf + sizeof(int)*2, db->entries, sizeof(LogEntry) * LOG_KEEP);
    
    secure_write_file(path, buf, buf_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    free(buf);
}

void log_load(LogDBPtr db, const char* path) {
    if (!db) return;
    size_t out_sz = 0;
    void* data = secure_read_file(path, &out_sz, DATA_FILE_MAGIC, DATA_FILE_VERSION);
    if (!data) return;
    
    if (out_sz != sizeof(int)*2 + sizeof(LogEntry) * LOG_KEEP) {
        free(data); return;
    }
    
    memcpy(&db->head, data, sizeof(int));
    memcpy(&db->count, (uint8_t*)data + sizeof(int), sizeof(int));
    memcpy(db->entries, (uint8_t*)data + sizeof(int)*2, sizeof(LogEntry) * LOG_KEEP);
    
    if (db->head < 0 || db->head >= LOG_KEEP) db->head = 0;
    if (db->count < 0 || db->count > LOG_KEEP) db->count = 0;
    
    for (int i=0; i<LOG_KEEP; i++) {
        db->entries[i].msg[MAX_LOG_MSG - 1] = '\0';
        db->entries[i].ts[19] = '\0';
    }
    
    free(data);
}