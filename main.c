#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 128
typedef struct { char name[32]; size_t start, count; int active; } FileEntry;
typedef struct { size_t capacity; unsigned char *blocks; FileEntry files[MAX_FILES]; } Allocator;
typedef struct { size_t used, free_blocks, largest_run, free_runs; } Stats;

static int allocator_init(Allocator *a, size_t capacity) {
    memset(a, 0, sizeof(*a));
    if (!capacity || capacity > 1000000) return 0;
    a->blocks = calloc(capacity, 1);
    if (!a->blocks) return 0;
    a->capacity = capacity;
    return 1;
}
static void allocator_destroy(Allocator *a) { free(a->blocks); a->blocks = NULL; }
static int valid_name(const char *name) {
    size_t n = strlen(name);
    if (!n || n >= sizeof(((FileEntry *)0)->name)) return 0;
    for (size_t i=0; i<n; ++i)
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') return 0;
    return 1;
}
static int allocate(Allocator *a, const char *name, size_t count, size_t *start) {
    int slot = -1;
    if (!a->blocks || !valid_name(name) || !count || count > a->capacity) return 0;
    for (int i=0; i<MAX_FILES; ++i) {
        if (a->files[i].active && strcmp(a->files[i].name, name) == 0) return 0;
        if (!a->files[i].active && slot < 0) slot = i;
    }
    if (slot < 0) return 0;
    size_t run = 0;
    for (size_t i=0; i<a->capacity; ++i) {
        run = a->blocks[i] ? 0 : run + 1;
        if (run == count) {
            *start = i + 1 - count;
            for (size_t j=*start; j<=i; ++j) a->blocks[j] = 1;
            FileEntry *f = &a->files[slot];
            strcpy(f->name, name); f->start=*start; f->count=count; f->active=1;
            return 1;
        }
    }
    return 0;
}
static int free_blocks(Allocator *a, const char *name) {
    for (int i=0; i<MAX_FILES; ++i) {
        FileEntry *f = &a->files[i];
        if (f->active && strcmp(f->name, name) == 0) {
            memset(a->blocks + f->start, 0, f->count);
            f->active = 0;
            return 1;
        }
    }
    return 0;
}
static Stats get_stats(const Allocator *a) {
    Stats s = {0,0,0,0};
    size_t run = 0;
    for (size_t i=0; i<a->capacity; ++i) {
        if (a->blocks[i]) { s.used++; run=0; }
        else {
            s.free_blocks++; run++;
            if (run == 1) s.free_runs++;
            if (run > s.largest_run) s.largest_run = run;
        }
    }
    return s;
}
static int parse_size(const char *text, size_t *out) {
    char *end;
    unsigned long long value;
    if (!text[0] || !isdigit((unsigned char)text[0])) return 0;
    errno=0; value=strtoull(text,&end,10);
    if (errno || *end || !value || value > 1000000) return 0;
    *out=(size_t)value;
    return 1;
}
static int read_operation_line(FILE *input, char line[256], int *invalid) {
    size_t n=0; int c,got=0;
    *invalid=0;
    while((c=fgetc(input))!=EOF) {
        got=1;
        if(c=='\n') break;
        if(c=='\0') { *invalid=1; continue; }
        if(n<255) line[n++]=(char)c;
        else *invalid=1;
    }
    if(!got) return 0;
    line[n]='\0';
    return 1;
}
static int run_log_into(FILE *input, FILE *output, Allocator *a) {
    char line[256]; size_t line_no=0;
    int malformed;
    while (read_operation_line(input,line,&malformed)) {
        char cmd[32]={0}, name[64]={0}, count_text[64]={0}, extra[2]={0};
        int n; size_t count, start;
        ++line_no;
        if (malformed) { fprintf(stderr,"Invalid or overlong operation at line %zu\n",line_no); return 1; }
        n=sscanf(line," %31s %63s %63s %1s",cmd,name,count_text,extra);
        if (n <= 0 || cmd[0] == '#') continue;
        if (strcmp(cmd,"ALLOC")==0 && n==3 && parse_size(count_text,&count) && allocate(a,name,count,&start))
            fprintf(output,"ALLOC %s start=%zu blocks=%zu\n",name,start,count);
        else if (strcmp(cmd,"FREE")==0 && n==2 && free_blocks(a,name))
            fprintf(output,"FREE %s\n",name);
        else if (strcmp(cmd,"MAP")==0 && n==1) {
            fputs("MAP ",output);
            for(size_t i=0;i<a->capacity;++i) fputc(a->blocks[i]?'#':'.',output);
            fputc('\n',output);
        } else if (strcmp(cmd,"STATS")==0 && n==1) {
            Stats s=get_stats(a);
            fprintf(output,"STATS used=%zu free=%zu largest_run=%zu free_runs=%zu\n",s.used,s.free_blocks,s.largest_run,s.free_runs);
        } else { fprintf(stderr,"Invalid operation at line %zu\n",line_no); return 1; }
    }
    if (ferror(input) || ferror(output)) { fputs("I/O error\n",stderr); return 1; }
    return 0;
}
static int run_log(FILE *input, FILE *output, Allocator *a) {
    FILE *staged=tmpfile();
    if(!staged) { perror("tmpfile"); return 1; }
    int status=run_log_into(input,staged,a);
    if(!status && (fflush(staged)!=0 || fseek(staged,0,SEEK_SET)!=0)) status=1;
    if(!status) {
        unsigned char bytes[8192]; size_t n;
        while((n=fread(bytes,1,sizeof(bytes),staged))!=0)
            if(fwrite(bytes,1,n,output)!=n) { status=1; break; }
        if(ferror(staged) || ferror(output)) status=1;
        if(!status && fflush(output)!=0) status=1;
    }
    if(fclose(staged)!=0) status=1;
    if(status) fputs("No complete report could be produced\n",stderr);
    return status;
}
#ifndef UNIT_TEST
int main(int argc, char **argv) {
    Allocator a; size_t capacity;
    FILE *input;
    if (argc != 3 || !parse_size(argv[1],&capacity)) {
        fputs("Usage: allocator BLOCK_COUNT operations.txt (1..1000000 blocks)\n",stderr); return 2;
    }
    input=fopen(argv[2],"r");
    if (!input) { perror(argv[2]); return 2; }
    if (!allocator_init(&a,capacity)) { fputs("Cannot allocate block map\n",stderr); fclose(input); return 2; }
    int status=run_log(input,stdout,&a);
    allocator_destroy(&a); fclose(input);
    return status;
}
#endif
