// Build: gcc evaluator.c sqlite3.c -O2 -lpthread -lm -o evaluator
// Usage:
//   ./evaluator --db ga.db
//   ./evaluator --db ga.db --loop

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#ifdef _WIN32
#include <windows.h>
static void sleep_ms(int ms){ Sleep(ms); }
#else
#include <time.h>
static void sleep_ms(int ms){
    struct timespec ts = { ms/1000, (ms%1000)*1000000L };
    nanosleep(&ts, NULL);
}
#endif

typedef unsigned char bit_t;
typedef struct {
    bit_t *genes;
    int fitness;
} Chromosome;

static sqlite3 *g_db = NULL;

static void die_sqlite(const char *msg, int rc){
    fprintf(stderr, "[sqlite] %s (rc=%d)\n", msg, rc);
    exit(1);
}

static void db_open(const char *path){
    int rc = sqlite3_open(path, &g_db);
    if(rc != SQLITE_OK) die_sqlite("sqlite3_open failed", rc);
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
}

static void db_close(void){
    if(g_db) sqlite3_close(g_db);
    g_db = NULL;
}

// Represents a claimed chromosome from DB
typedef struct {
    int has_work;
    int gen;
    int idx;
    Chromosome chrom;
    int geneLength;
} Claim;

// Claim one pending chromosome atomically
static Claim claim_one_pending(void){
    Claim c = {0};
    char *errmsg = NULL;
    int rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, &errmsg);
    if(rc != SQLITE_OK) return c; // DB busy, skip this loop

    sqlite3_stmt *sel = NULL;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT gen, idx, chromosome FROM individuals "
        "WHERE status='pending' ORDER BY gen ASC, idx ASC LIMIT 1;",
        -1, &sel, NULL);
    if(rc != SQLITE_OK){ sqlite3_exec(g_db,"ROLLBACK;",NULL,NULL,NULL); die_sqlite("prepare select", rc); }

    rc = sqlite3_step(sel);
    if(rc == SQLITE_ROW){
        c.gen = sqlite3_column_int(sel, 0);
        c.idx = sqlite3_column_int(sel, 1);
        const void *blob = sqlite3_column_blob(sel, 2);
        int blen = sqlite3_column_bytes(sel, 2);
        c.geneLength = blen; // since bit_t = 1 byte
        c.chrom.genes = malloc((size_t)blen);
        memcpy(c.chrom.genes, blob, (size_t)blen);
        c.has_work = 1;
    } else {
        sqlite3_finalize(sel);
        sqlite3_exec(g_db,"ROLLBACK;",NULL,NULL,NULL);
        return c;
    }
    sqlite3_finalize(sel);

    sqlite3_stmt *upd = NULL;
    rc = sqlite3_prepare_v2(g_db,
        "UPDATE individuals SET status='claimed', claimed_ts=strftime('%s','now') "
        "WHERE gen=? AND idx=?;",
        -1, &upd, NULL);
    if(rc != SQLITE_OK){ sqlite3_exec(g_db,"ROLLBACK;",NULL,NULL,NULL); die_sqlite("prepare update", rc); }
    sqlite3_bind_int(upd, 1, c.gen);
    sqlite3_bind_int(upd, 2, c.idx);
    rc = sqlite3_step(upd);
    sqlite3_finalize(upd);
    if(rc != SQLITE_DONE){ sqlite3_exec(g_db,"ROLLBACK;",NULL,NULL,NULL); die_sqlite("update step", rc); }

    rc = sqlite3_exec(g_db, "COMMIT;", NULL, NULL, &errmsg);
    if(rc != SQLITE_OK) die_sqlite("COMMIT", rc);
    return c;
}

static void free_claim(Claim *c){
    if(c && c->chrom.genes){ free(c->chrom.genes); c->chrom.genes=NULL; }
}

// Example fitness: count number of 1 bits
static int fitness(const Chromosome *c, int geneLength){
    int sum = 0;
    for(int i=0;i<geneLength;i++)
        sum += (c->genes[i] ? 1 : 0);
    return sum;
}

static void report_done(int gen, int idx, int fitness){
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE individuals SET status='done', fitness=?, done_ts=strftime('%s','now') "
        "WHERE gen=? AND idx=? AND status='claimed';",
        -1, &st, NULL);
    if(rc != SQLITE_OK) die_sqlite("prepare report_done", rc);
    sqlite3_bind_double(st, 1, (double)fitness);
    sqlite3_bind_int(st, 2, gen);
    sqlite3_bind_int(st, 3, idx);
    rc = sqlite3_step(st);
    sqlite3_finalize(st);
    if(rc != SQLITE_DONE) die_sqlite("report_done step", rc);
}

int main(int argc, char **argv){
    const char *db_path = "ga.db";
    int keep_looping = 0;
    int poll_ms = 250;

    for(int i=1;i<argc;i++){
        if(strcmp(argv[i],"--db")==0 && i+1<argc) db_path = argv[++i];
        else if(strcmp(argv[i],"--loop")==0) keep_looping = 1;
        else if(strcmp(argv[i],"--poll-ms")==0 && i+1<argc) poll_ms = atoi(argv[++i]);
    }

    db_open(db_path);
    printf("[evaluator] connected to %s (loop=%d)\n", db_path, keep_looping);

    for(;;){
        Claim c = claim_one_pending();
        if(!c.has_work){
            if(!keep_looping) break;
            sleep_ms(poll_ms);
            continue;
        }

        // Compute fitness
        c.chrom.fitness = fitness(&c.chrom, c.geneLength);

        // Report back to DB
        report_done(c.gen, c.idx, c.chrom.fitness);

        printf("[evaluator] gen=%d idx=%d len=%d fitness=%d\n",
               c.gen, c.idx, c.geneLength, c.chrom.fitness);

        free_claim(&c);
    }

    db_close();
    printf("[evaluator] done.\n");
    return 0;
}
