
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <getopt.h>

typedef unsigned char bit_t; /* 0 or 1 */

typedef struct {
    bit_t *genes;
    int fitness;
} Chromosome;

typedef struct {
    int generation;
    int population;
    int geneLength;
    int elitism;
    int generations;
    int saveEvery;
    double mutation;
} Hyper;

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
    srand(time(NULL));
    char path[100] = "checkpoint";
    int resume = 0;
    int geneLength = 64;
    int population = 10;
    int elitism = 5;
    int generations = 1000;
    int generation = 1;
    int saveEvery = 100;
    double mutation = 0.02;

    Hyper hyperparm = {
        generation, //this technically isnt a hyperparamter
        population,
        geneLength,
        elitism,
        generations, //this is the total generation count
        saveEvery,
        mutation
    };
    
    //import argparse
    if(argc > 1)
    {
        //arguments are 
        //continue checkpointPath genelength popsize elitism generation saveEvery mutation
        if(argc >= 2)
        {
            resume = atoi(argv[1]);
        }
        if(argc >= 3)
        {
            strncpy(path, argv[2], sizeof(path));
        }
        if(argc >= 4)
        {
            hyperparm.geneLength = atoi(argv[3]);
        }
        if(argc >= 5)
        {
            hyperparm.population = atoi(argv[4]);
        }
        if(argc >= 6)
        {
            hyperparm.elitism = atoi(argv[5]);
        }
        if(argc >= 7)
        {
            hyperparm.generations = atoi(argv[6]);
        }
        if(argc >= 8)
        {
            hyperparm.saveEvery = atoi(argv[7]);
        }
        if(argc >= 9)
        {
            char* endptr;
            hyperparm.mutation = strtod(argv[8], &endptr);
        }
    }
    Chromosome **pop = malloc((size_t)population * sizeof(Chromosome*));
    if(resume)
    {
        FILE *f = fopen(path, "r");
        if (!f)
        {
            printf("Checkpoint %s does not exist\n", path);
            return 0;
        }
        loadPopulation(path, pop, &hyperparm);
    }
    printf("%s training using %s\n", resume ? "Resume" : "New", path);
    removeNumericSuffix(path);
    printf("\tGene Length: %d\n", hyperparm.geneLength);
    printf("\tPopulation: %d\n", hyperparm.population);
    printf("\tElitism: %d\n", hyperparm.elitism);
    printf("\tGenerations: %d\n", hyperparm.generations);
    printf("\tSave Every: %d\n", hyperparm.saveEvery);
    printf("\tMutation Rate: %f\n", hyperparm.mutation);
    if(hyperparm.elitism < 3)
    {
        printf("Elitism of %d is too low (elitism >= 3)\n", hyperparm.elitism);
        return 0;
    }
    createPopulation(pop, population, hyperparm.geneLength);
    evaluate(pop, fitness, population, hyperparm.geneLength);
    savePopulation(path, pop, &hyperparm);
    //loop
    printf("Starting from generation %d\n", hyperparm.generation);
    int start = hyperparm.generation+1;
    for(int i = start;i<=hyperparm.generations;i++)
    {
        reproduce(pop, hyperparm.elitism, hyperparm.mutation, hyperparm.population, hyperparm.geneLength);
        evaluate(pop, fitness, hyperparm.population ,hyperparm.geneLength);
        if(i%saveEvery==0)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "%s-%d", path, i);
            hyperparm.generation = i;
            savePopulation(buf, pop, &hyperparm);
        }
    }
    printf("Trained for %d generations\n", hyperparm.generations);
    printf("Population:\n---------------------------------------------------------\n");
    printPopulation(pop, population, geneLength);
    return 0;
}