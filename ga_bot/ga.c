
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>


#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>   // <-- needed for Sleep(ms)
#else
    #include <getopt.h>
#endif
#include <sqlite3.h>

#if defined(_WIN32) || defined(_WIN64)
// Windows-compatible getline implementation
size_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!lineptr || !n || !stream) return -1;
    int c;
    size_t pos = 0;
    if (*lineptr == NULL || *n == 0) {
        *n = 128;
        *lineptr = malloc(*n);
        if (*lineptr == NULL) return -1;
    }
    while ((c = fgetc(stream)) != EOF) {
        if (pos + 1 >= *n) {
            *n *= 2;
            char *tmp = realloc(*lineptr, *n);
            if (!tmp) return -1;
            *lineptr = tmp;
        }
        (*lineptr)[pos++] = (char)c;
        if (c == '\n') break;
    }
    if (pos == 0 && c == EOF) return -1;
    (*lineptr)[pos] = '\0';
    return (size_t)pos;
}
#endif


// --- add near your SQLite helpers ---
static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts; ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

typedef unsigned char bit_t; /* 0 or 1 */

typedef struct {
    bit_t *genes;
    int fitness;
} Chromosome;
void sortPop(Chromosome** in, int population);
Chromosome *createChromosome(int L);
typedef struct {
    int generation;
    int population;
    int geneLength;
    int elitism;
    int generations;
    int saveEvery;
    double mutation;
} Hyper;

// ---------- SQLite helpers (NEW) ----------
static sqlite3 *g_db = NULL;

static void die_sqlite(const char *msg, int rc) {
    fprintf(stderr, "SQLite error: %s (rc=%d)\n", msg, rc);
    exit(1);
}

static void db_open(const char *path) {
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) die_sqlite("sqlite3_open failed", rc);
    // pragma: durable enough, still fast
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
}

static void db_close(void) {
    if (g_db) sqlite3_close(g_db);
    g_db = NULL;
}

static void db_init_schema(void) {
    const char *sql =
        "CREATE TABLE IF NOT EXISTS generations ("
        "  gen INTEGER PRIMARY KEY,"
        "  created_ts INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS individuals ("
        "  gen INTEGER NOT NULL,"
        "  idx INTEGER NOT NULL,"
        "  chromosome BLOB NOT NULL,"
        "  status TEXT NOT NULL DEFAULT 'pending',"   /* pending|claimed|done */
        "  fitness REAL,"
        "  claimed_ts INTEGER,"
        "  done_ts INTEGER,"
        "  PRIMARY KEY(gen, idx)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_indiv_status ON individuals(status);";
    int rc = sqlite3_exec(g_db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) die_sqlite("init schema failed", rc);
}

// Insert/replace all individuals for a generation as pending with their chromosome bytes
static void db_insert_generation(int gen, Chromosome **pop, int popSize, int geneLength) {
    int rc;
    sqlite3_stmt *ins = NULL, *ins_gen = NULL;

    rc = sqlite3_exec(g_db, "BEGIN IMMEDIATE;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) die_sqlite("BEGIN failed", rc);

    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR IGNORE INTO generations(gen, created_ts) VALUES(?, strftime('%s','now'));",
        -1, &ins_gen, NULL);
    if (rc != SQLITE_OK) die_sqlite("prepare gen insert failed", rc);
    sqlite3_bind_int(ins_gen, 1, gen);
    rc = sqlite3_step(ins_gen);
    if (rc != SQLITE_DONE && rc != SQLITE_CONSTRAINT) die_sqlite("gen insert step failed", rc);
    sqlite3_finalize(ins_gen);

    rc = sqlite3_prepare_v2(g_db,
        "INSERT OR REPLACE INTO individuals(gen, idx, chromosome, status) "
        "VALUES(?, ?, ?, 'pending');",
        -1, &ins, NULL);
    if (rc != SQLITE_OK) die_sqlite("prepare indiv insert failed", rc);

    for (int i = 0; i < popSize; ++i) {
        Chromosome *c = pop[i];
        sqlite3_bind_int(ins, 1, gen);
        sqlite3_bind_int(ins, 2, i);
        // store raw bytes of your bit array
        sqlite3_bind_blob(ins, 3, (const void*)c->genes, (int)geneLength * (int)sizeof(bit_t), SQLITE_STATIC);
        rc = sqlite3_step(ins);
        if (rc != SQLITE_DONE) die_sqlite("individual insert step failed", rc);
        sqlite3_reset(ins);
        sqlite3_clear_bindings(ins);
    }
    sqlite3_finalize(ins);

    rc = sqlite3_exec(g_db, "COMMIT;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) die_sqlite("COMMIT failed", rc);
}

// Update fitness for one individual and mark done
static void db_update_fitness(int gen, int idx, int fitness) {
    sqlite3_stmt *upd = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "UPDATE individuals "
        "SET status='done', fitness=?, done_ts=strftime('%s','now') "
        "WHERE gen=? AND idx=?;",
        -1, &upd, NULL);
    if (rc != SQLITE_OK) die_sqlite("prepare update failed", rc);
    sqlite3_bind_double(upd, 1, (double)fitness);
    sqlite3_bind_int(upd, 2, gen);
    sqlite3_bind_int(upd, 3, idx);
    rc = sqlite3_step(upd);
    sqlite3_finalize(upd);
    if (rc != SQLITE_DONE) die_sqlite("update fitness step failed", rc);
}

// wait until COUNT(done) == popsize
static void db_wait_for_generation_done(int gen, int popsize, int poll_ms) {
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*) FROM individuals WHERE gen=? AND status='done';",
        -1, &st, NULL);
    if (rc != SQLITE_OK) die_sqlite("prepare wait stmt failed", rc);

    for (;;) {
        sqlite3_reset(st);
        sqlite3_clear_bindings(st);
        sqlite3_bind_int(st, 1, gen);
        rc = sqlite3_step(st);
        if (rc != SQLITE_ROW) die_sqlite("wait step failed", rc);
        int done = sqlite3_column_int(st, 0);
        if (done >= popsize) { sqlite3_finalize(st); return; }
        sleep_ms(poll_ms);
    }
}

// load fitness back into in-memory population after external eval
static void db_load_fitnesses_for_gen(int gen, Chromosome **pop, int popsize) {
    sqlite3_stmt *st = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT idx, fitness FROM individuals WHERE gen=? AND status='done' ORDER BY idx;",
        -1, &st, NULL);
    if (rc != SQLITE_OK) die_sqlite("prepare pull fitness failed", rc);

    sqlite3_bind_int(st, 1, gen);
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        int idx = sqlite3_column_int(st, 0);
        double fit = sqlite3_column_double(st, 1);
        if (idx >= 0 && idx < popsize && pop[idx]) pop[idx]->fitness = (int)(fit + 0.5);
    }
    if (rc != SQLITE_DONE) die_sqlite("pull fitness step failed", rc);
    sqlite3_finalize(st);

    // keep your sort order consistent for selection
    sortPop(pop, popsize);
}

#include <sys/stat.h>

static int file_exists(const char *p){
    struct stat st;
    return (p && stat(p, &st) == 0 && S_ISREG(st.st_mode));
}

// Returns 1 if there is any row in DB, 0 if empty
static int db_has_any_rows(void){
    sqlite3_stmt *st=NULL; int rc, any=0;
    rc = sqlite3_prepare_v2(g_db, "SELECT 1 FROM individuals LIMIT 1;", -1, &st, NULL);
    if(rc==SQLITE_OK && sqlite3_step(st)==SQLITE_ROW) any=1;
    if(st) sqlite3_finalize(st);
    return any;
}

// Get latest generation number present in the DB; returns 1 on success
static int db_get_latest_gen(int *out_gen){
    sqlite3_stmt *st=NULL; int rc;
    rc = sqlite3_prepare_v2(g_db, "SELECT MAX(gen) FROM individuals;", -1, &st, NULL);
    if(rc!=SQLITE_OK) die_sqlite("prepare MAX(gen)", rc);
    rc = sqlite3_step(st);
    if(rc==SQLITE_ROW && !sqlite3_column_type(st,0)==SQLITE_NULL){
        *out_gen = sqlite3_column_int(st,0);
        sqlite3_finalize(st);
        return 1;
    }
    sqlite3_finalize(st);
    return 0;
}

// Count done vs total for a generation
static void db_counts_for_gen(int gen, int *out_total, int *out_done){
    sqlite3_stmt *st=NULL; int rc;
    *out_total=0; *out_done=0;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*), SUM(status='done') FROM individuals WHERE gen=?;", -1, &st, NULL);
    if(rc!=SQLITE_OK) die_sqlite("prepare counts", rc);
    sqlite3_bind_int(st,1,gen);
    if(sqlite3_step(st)==SQLITE_ROW){
        *out_total = sqlite3_column_int(st,0);
        *out_done  = sqlite3_column_int(st,1);
    }
    sqlite3_finalize(st);
}

// Get pop size and geneLength for a generation
static void db_get_shape_for_gen(int gen, int *popsize, int *geneLength){
    sqlite3_stmt *st=NULL; int rc;
    *popsize=0; *geneLength=0;
    // pop size
    rc = sqlite3_prepare_v2(g_db,
        "SELECT COUNT(*) FROM individuals WHERE gen=?;", -1, &st, NULL);
    if(rc!=SQLITE_OK) die_sqlite("prepare popsize", rc);
    sqlite3_bind_int(st,1,gen);
    if(sqlite3_step(st)==SQLITE_ROW) *popsize = sqlite3_column_int(st,0);
    sqlite3_finalize(st);
    // geneLength from first row blob size
    rc = sqlite3_prepare_v2(g_db,
        "SELECT LENGTH(chromosome) FROM individuals WHERE gen=? ORDER BY idx LIMIT 1;", -1, &st, NULL);
    if(rc!=SQLITE_OK) die_sqlite("prepare geneLength", rc);
    sqlite3_bind_int(st,1,gen);
    if(sqlite3_step(st)==SQLITE_ROW) *geneLength = sqlite3_column_int(st,0);
    sqlite3_finalize(st);
}

// Ensure in-memory population is allocated to match (re)loaded shape
static void ensure_population(Chromosome ***pop, int *cur_popsize, int new_popsize, int new_geneLength){
    if(*pop && *cur_popsize==new_popsize) return; // assume chromosomes already sized
    // free old if present (simple/free-only version; adjust as you need)
    if(*pop){
        for(int i=0;i<*cur_popsize;i++){
            if((*pop)[i]){
                free((*pop)[i]->genes);
                free((*pop)[i]);
            }
        }
        free(*pop);
    }
    *pop = (Chromosome**)malloc((size_t)new_popsize*sizeof(Chromosome*));
    for(int i=0;i<new_popsize;i++){
        (*pop)[i] = createChromosome(new_geneLength);
    }
    *cur_popsize = new_popsize;
}

// Load chromosomes + fitness from DB into memory
static void db_load_population_for_gen(int gen, Chromosome **pop, int popsize, int geneLength){
    sqlite3_stmt *st=NULL; int rc;
    rc = sqlite3_prepare_v2(g_db,
        "SELECT idx, chromosome, fitness FROM individuals WHERE gen=? ORDER BY idx;", -1, &st, NULL);
    if(rc!=SQLITE_OK) die_sqlite("prepare load pop", rc);
    sqlite3_bind_int(st,1,gen);
    while((rc=sqlite3_step(st))==SQLITE_ROW){
        int idx = sqlite3_column_int(st,0);
        const void *blob = sqlite3_column_blob(st,1);
        int blen = sqlite3_column_bytes(st,1);
        double fit = sqlite3_column_type(st,2)==SQLITE_NULL ? 0.0 : sqlite3_column_double(st,2);
        if(idx>=0 && idx<popsize && pop[idx]){
            int tocopy = blen < geneLength ? blen : geneLength;
            memcpy(pop[idx]->genes, blob, (size_t)tocopy);
            if (tocopy < geneLength) memset(pop[idx]->genes + tocopy, 0, (size_t)(geneLength - tocopy));
            pop[idx]->fitness = (int)(fit + 0.5);
        }
    }
    if(rc!=SQLITE_DONE) die_sqlite("step load pop", rc);
    sqlite3_finalize(st);
    sortPop(pop, popsize);
}


void printChromosomeSingle(const Chromosome *c, int L)
{
    printf("fitness=%d genes=", c->fitness);

    /* print gene bits as characters; safe if genes is an array of bit_t bytes */
    if (c->genes) {
        for (int j = 0; j < L; ++j) {
            printf(c->genes[j] ? "1" : "0");
        }
    } else {
        printf("<no-genes>");
    }
    printf("\n");
}
void printChromosome(const Chromosome *c, int idx, int L)
{
    if (!c) {
        if (idx >= 0) printf("[%d] <NULL>\n", idx);
        else printf("<NULL>\n");
        return;
    }

    if (idx >= 0) printf("[%d] ", idx);
    printf("fitness=%d genes=", c->fitness);

    /* print gene bits as characters; safe if genes is an array of bit_t bytes */
    if (c->genes) {
        for (int j = 0; j < L; ++j) {
            printf(c->genes[j] ? "1" : "0");
        }
    } else {
        printf("<no-genes>");
    }
    printf("\n");
}

/* Print whole population.
   Prints a header with population stats, then each individual.
*/
void printPopulation(Chromosome **pop, int pop_size, int L)
{
    if (!pop) {
        printf("<population is NULL>\n");
        return;
    }
    if (pop_size <= 0) {
        printf("<empty population (size=%d)>\n", pop_size);
        return;
    }

    /* compute simple stats: avg fitness and best */
    double avg = 0.0;
    int best_idx = 0;
    int best_fit = -1;
    for (int i = 0; i < pop_size; ++i) {
        if (!pop[i]) continue;
        avg += (double)pop[i]->fitness;
        if (pop[i]->fitness > best_fit) { best_fit = pop[i]->fitness; best_idx = i; }
    }
    avg /= (double)pop_size;

    printf("Population size=%d geneLength=%d avg_fitness=%.4f best_idx=%d best_fitness=%d\n",
            pop_size, L, avg, best_idx, best_fit);

    for (int i = 0; i < pop_size; ++i) {
        printChromosome(pop[i], i, L);
    }
}

//allocates memory
Chromosome *createChromosome(int L)
{
    Chromosome *c = malloc(sizeof(Chromosome));
    c->genes = calloc((size_t)L, sizeof(bit_t));
    c->fitness = 0;
    return c;
}

void freeChromosome(Chromosome* c)
{
    if (!c) return;
    free(c->genes);
    free(c);
}

void copyChromosome(Chromosome *out, const Chromosome *src, int length) {
    memcpy(out->genes, src->genes, (size_t)length * sizeof(bit_t));
    out->fitness = src->fitness;
}

void randomChromosome(Chromosome *ind, int geneLength) {
    for (int i = 0; i < geneLength; ++i) {
        ind->genes[i] = (rand() < RAND_MAX / 2) ? 1 : 0;
    }
}

int fitness(const Chromosome *ind, int geneLength) {
    int sum = 0;
    for (int i = 0; i < geneLength; ++i) sum += (ind->genes[i] ? 1 : 0);
    return sum;
}

int compareChromosomeFit(const void* a, const void* b)
{
    Chromosome *A = *(Chromosome **)a;
    Chromosome *B = *(Chromosome **)b;
    if (A->fitness > B->fitness) return -1;
    if (A->fitness < B->fitness) return 1;
    return 0;
}
void sortPop(Chromosome** in, int population)
{
    qsort(in, (size_t)population, sizeof(Chromosome*), compareChromosomeFit);
}
//evaluate and sort 
int evaluate(Chromosome** pop, int (*fitfunc)(const Chromosome *, int), int popSize, int geneLength)
{
    for(int i=0;i<popSize;i++){
        pop[i]->fitness = fitfunc(pop[i], geneLength);
    }
    sortPop(pop,popSize);
}

int evaluate_sqlite(Chromosome** pop, int (*fitfunc)(const Chromosome *, int),
                    int popSize, int geneLength, int generation)
{
    for (int i = 0; i < popSize; i++) {
        pop[i]->fitness = fitfunc(pop[i], geneLength);
        // NEW: persist fitness to DB
        if (g_db) db_update_fitness(generation, i, pop[i]->fitness);
    }
    sortPop(pop, popSize);
    return 0;
}


void crossover(const Chromosome *a, const Chromosome *b, Chromosome *c1, int geneLength) {
    int point = rand() % geneLength;
    point = point < 2 ? 2 : point;
    int p = 1 + rand() % (point - 1);
    for (int i = 0; i < p; ++i) {
        c1->genes[i] = a->genes[i];
    }
    for (int i = p; i < point; ++i) {
        c1->genes[i] = b->genes[i];
    }
}

void mutate(Chromosome *c, int length, double mr) {
    for (int i = 0; i < length; ++i) {
        double r = (double)rand() / (double)RAND_MAX;
        if (r < mr) c->genes[i] = c->genes[i] ? 0 : 1;
    }
}


//a and b will be less than elites
int selectParents(int elites, int *a, int *b) {
    if (elites < 2) return -1;
    int x = rand() % elites;
    int y = rand() % (elites - 1);
    if (y >= x) y += 1;
    *a = x;
    *b = y;
    return 0;
}

//in-place operation of reproduction
//leave elitism Chromosomes as is
void reproduce(Chromosome** pop, int elitism, double mr, int popSize, int geneLength)
{
    for(int i=elitism;i<popSize;i++)
    {
        int a, b;
        selectParents(elitism, &a, &b);
        crossover(pop[a],pop[b],pop[i],geneLength);
        mutate(pop[i],geneLength,mr);
        pop[i]->fitness = 0;
    }
}

void createPopulation(Chromosome** pop, int population, int geneLength)
{
    for (int i = 0; i < population; ++i) {
        pop[i] = createChromosome(geneLength);
        randomChromosome(pop[i],geneLength); //for testing
    }
}

int savePopulation(const char *path, Chromosome **pop, Hyper* hyperparm)
{
    if (!path || !pop || hyperparm->population <= 0 || hyperparm->geneLength <= 1) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# generation=%d population=%d geneLength=%d elitism=%d generations=%d saveEvery=%d mutation=%f\n", 
            hyperparm->generation, //current generation
            hyperparm->population,
            hyperparm->geneLength, 
            hyperparm->elitism,
            hyperparm->generations,
            hyperparm->saveEvery,
            hyperparm->mutation);

    for (int i = 0; i < hyperparm->population; ++i) {
        Chromosome *c = pop[i];
        if (!c || !c->genes) {
            if (fprintf(f, "0 ") < 0) { fclose(f); return -1; }
            for (int g = 0; g < hyperparm->geneLength; ++g) if (fputc('0', f) == EOF) { fclose(f); return -1; }
            if (fputc('\n', f) == EOF) { fclose(f); return -1; }
            continue;
        }
        if (fprintf(f, "%d ", c->fitness) < 0) { fclose(f); return -1; }
        for (int g = 0; g < hyperparm->geneLength; ++g) {
            if (fputc(c->genes[g] ? '1' : '0', f) == EOF) { fclose(f); return -1; }
        }
        if (fputc('\n', f) == EOF) { fclose(f); return -1; }
    }

    fclose(f);
    return 0;
}

static inline void trim_inplace(char *s) {
    if (!s) return;
    /* left trim */
    char *p = s;
    while (*p && isspace((unsigned char)*p)) ++p;
    if (p != s) memmove(s, p, strlen(p) + 1);

    /* right trim */
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n-1])) { s[n-1] = '\0'; --n; }
}

//wish there was a library for this
static void getIntAttribute(const char *p, const char *attribute, int *out)
{
    if (!p || !attribute || !out) return;
    size_t alen = strlen(attribute);
    const char *key = p;

    /* keep searching until we find exact "attribute=" */
    while ((key = strstr(key, attribute)) != NULL) {
        if (key[alen] == '=') {                 /* exact match "attribute=" */
            const char *valp = key + alen + 1;  /* char after '=' */
            while (*valp && isspace((unsigned char)*valp)) ++valp;
            char *end = NULL;
            long v = strtol(valp, &end, 10);
            if (end != valp) {
                *out = (int)v;                  /* write whatever was parsed */
            }
            return;                              /* done (found the attribute) */
        }
        key++;                                   /* continue scanning */
    }
}

//wish there was a library for this
void getDoubleAttribute(const char *p, const char *attribute, double *out)
{
    if (!p || !attribute || !out) return;
    size_t alen = strlen(attribute);
    const char *key = p;

    while ((key = strstr(key, attribute)) != NULL) {
        if (key[alen] == '=') break;
        key++;
    }
    if (!key) return;

    const char *valp = key + alen + 1;
    while (*valp && isspace((unsigned char)*valp)) ++valp;

    char *end = NULL;
    double d = strtod(valp, &end);
    *out = d;
}

// line that starts with # is a header and has all the hyperparameters
static void parseHeader(const char *line, Hyper* hyperparm) {
    if (!line) return;
    const char *p = line;
    while (isspace((unsigned char)*p)) ++p;
    if (*p != '#') return; /* not a header/comment line */

    getIntAttribute(p,"generation", &hyperparm->generation);
    getIntAttribute(p,"population", &hyperparm->population);
    getIntAttribute(p,"geneLength", &hyperparm->geneLength);
    getIntAttribute(p,"elitism", &hyperparm->elitism);
    getIntAttribute(p,"generations", &hyperparm->generations);
    getIntAttribute(p,"saveEvery", &hyperparm->saveEvery);
    getDoubleAttribute(p,"mutation", &hyperparm->mutation);
}
// read chromosome line
int parseDataLine(char *line, int *out_fit, char **out_bits) {
    trim_inplace(line);
    if (line[0] == '\0' || line[0] == '#') return -1;

    /* check if line starts with digits (fitness) followed by space */
    char *p = line;
    while (*p && isdigit((unsigned char)*p)) ++p;
    if (p != line && isspace((unsigned char)*p)) {
        char save = *p;
        *p = '\0';
        int fit = atoi(line);
        *p = save;
        /* skip spaces */
        while (*p && isspace((unsigned char)*p)) ++p;
        if (*p == '\0') return -1; /* no bitstring */
        *out_fit = fit;
        *out_bits = p;
        return 0;
    }

    /* otherwise the line is assumed to be just the bitstring */
    *out_fit = -1;
    *out_bits = line;
    return 0;
}

int loadPopulation(const char *path, Chromosome **pop, Hyper* hyperparm)
{
    if (!path || !pop) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char *line = NULL;
    size_t len = 0;
    size_t read;
    int filled = 0;


    while ((read = getline(&line, &len, f)) != -1) {
        trim_inplace(line);
        if (line[0] == '#' || line[0] == '\0') {
            /* try parse header to pick up geneLength/population if present */
            parseHeader(line, hyperparm); /* vec_cap used temporarily for population */
            continue;
        }
        int fit; char *bits;
        if (parseDataLine(line, &fit, &bits) != 0) continue;
        if (hyperparm->generations == 0 || hyperparm->population == 0)
        {
            printf("ERROR: Could not read genelength or popsize");
            return -1;
        }
        /* fill existing Chromosome pop[filled] */
        Chromosome *c = pop[filled];
        if (!c) {
            /* allocate one if caller didn't */
            c = createChromosome(hyperparm->geneLength);
            if (!c) continue;
            pop[filled] = c;
        }
        for (int i = 0; i < hyperparm->geneLength; ++i) c->genes[i] = (bits[i] == '1') ? 1 : 0;
        c->fitness = fit;
        filled++;
    }

    free(line);
    fclose(f);
    return filled;
}


void removeNumericSuffix(char *s) {
    if (!s) return;

    size_t len = strlen(s);
    while (len > 0 && isdigit((unsigned char)s[len - 1])) {
        len--;
    }
    // len is now index of first char AFTER the kept prefix
    s[len] = '\0';
    printf("success");
}

int main(int argc, char **argv)
{
    srand((unsigned)time(NULL));

    // defaults (some of these will be overridden by DB if present)
    char path[100] = "checkpoint";
    int resume = 0;
    int geneLength = 64;
    int population = 10;
    int elitism = 5;
    int generations = 1000;
    int generation = 1;
    int saveEvery = 100;
    double mutation = 0.02;

    const char *db_path = "ga.db";
    int use_external_eval = 0;
    int poll_ms = 250;

    // minimal flag parsing
    for (int i = 1; i < argc; ++i){
        if (strcmp(argv[i], "--db")==0 && i+1<argc) db_path = argv[++i];
        else if (strcmp(argv[i], "--external")==0) use_external_eval = 1;
        else if (strcmp(argv[i], "--poll-ms")==0 && i+1<argc) poll_ms = atoi(argv[++i]);
        else if (strcmp(argv[i], "--resume")==0 && i+1<argc) resume = atoi(argv[++i]);
        else if (strcmp(argv[i], "--checkpoint")==0 && i+1<argc) strncpy(path, argv[++i], sizeof(path));
        // keep your other positional args if you like
    }

    Hyper hyperparm = {
        generation, population, geneLength, elitism, generations, saveEvery, mutation
    };

    // Open DB (create if missing) and ensure schema
    db_open(db_path);
    db_init_schema();

    Chromosome **pop = NULL;
    int cur_pop = 0;

    if (file_exists(db_path) && db_has_any_rows()){
        // ---- Resume from latest generation in DB ----
        int latest = 0;
        db_get_latest_gen(&latest);
        int db_pop=0, db_L=0; db_get_shape_for_gen(latest, &db_pop, &db_L);
        if (db_pop <= 0 || db_L <= 0){
            fprintf(stderr, "DB exists but has no valid individuals; seeding anew.\n");
        } else {
            // adopt DB shape and hyper parameters
            hyperparm.generation = latest;
            hyperparm.population = population = db_pop;
            hyperparm.geneLength = geneLength = db_L;
            ensure_population(&pop, &cur_pop, population, geneLength);
            db_load_population_for_gen(latest, pop, population, geneLength);

            int total=0, done=0; db_counts_for_gen(latest, &total, &done);
            if (done < total){
                printf("[resume] gen=%d is incomplete (%d/%d done)\n", latest, done, total);
                if (use_external_eval){
                    // wait for external workers to finish it
                    db_wait_for_generation_done(latest, population, poll_ms);
                    db_load_fitnesses_for_gen(latest, pop, population);
                } else {
                    // finish locally
                    evaluate_sqlite(pop, fitness, population, geneLength, latest);
                }
            } else {
                printf("[resume] gen=%d is complete. Continuing.\n", latest);
            }
        }
    }

    // If still no population (new DB), seed a fresh one
    if (!pop){
        ensure_population(&pop, &cur_pop, population, geneLength);
        createPopulation(pop, population, geneLength);
        // insert seed generation (hyperparm.generation)
        db_insert_generation(hyperparm.generation, pop, population, geneLength);
        if (use_external_eval){
            db_wait_for_generation_done(hyperparm.generation, population, poll_ms);
            db_load_fitnesses_for_gen(hyperparm.generation, pop, population);
        } else {
            evaluate_sqlite(pop, fitness, population, geneLength, hyperparm.generation);
        }
        savePopulation(path, pop, &hyperparm);
    }

    printf("Training using DB='%s' (mode=%s)\n", db_path, use_external_eval ? "EXTERNAL" : "LOCAL");
    printf("\tGene Length: %d\n", hyperparm.geneLength);
    printf("\tPopulation: %d\n", hyperparm.population);
    printf("\tElitism: %d\n", hyperparm.elitism);
    printf("\tGenerations: %d\n", hyperparm.generations);
    printf("\tSave Every: %d\n", hyperparm.saveEvery);
    printf("\tMutation Rate: %f\n", hyperparm.mutation);

    if (hyperparm.elitism < 3){
        printf("Elitism of %d is too low (elitism >= 3)\n", hyperparm.elitism);
        db_close();
        return 0;
    }

    // Start from next generation after whatever we’re at
    int start = hyperparm.generation + 1;
    for (int i = start; i <= hyperparm.generations; i++){
        reproduce(pop, hyperparm.elitism, hyperparm.mutation, hyperparm.population, hyperparm.geneLength);

        // Insert the new generation
        db_insert_generation(i, pop, hyperparm.population, hyperparm.geneLength);

        if (use_external_eval){
            db_wait_for_generation_done(i, hyperparm.population, poll_ms);
            db_load_fitnesses_for_gen(i, pop, hyperparm.population);
        } else {
            evaluate_sqlite(pop, fitness, hyperparm.population, hyperparm.geneLength, i);
        }

        if (i % hyperparm.saveEvery == 0){
            char buf[256];
            snprintf(buf, sizeof buf, "%s-%d", path, i);
            hyperparm.generation = i;
            savePopulation(buf, pop, &hyperparm);
        }
    }

    db_close();

    printf("Trained through generation %d\n", hyperparm.generations);
    printf("Population:\n---------------------------------------------------------\n");
    printPopulation(pop, hyperparm.population, hyperparm.geneLength);
    return 0;
}

