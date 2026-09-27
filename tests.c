#define UNIT_TEST
#include "main.c"
#include <stdlib.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "CHECK failed at line %d: %s\n", __LINE__, #condition); exit(1); } } while (0)
#include <string.h>

int main(void) {
    Allocator a;
    size_t start;
    Stats s;
    CHECK(allocator_init(&a, 8));
    CHECK(allocate(&a, "alpha", 3, &start) && start == 0);
    CHECK(allocate(&a, "beta", 2, &start) && start == 3);
    CHECK(!allocate(&a, "alpha", 1, &start));
    CHECK(free_blocks(&a, "alpha"));
    CHECK(allocate(&a, "gamma", 2, &start) && start == 0);
    s = get_stats(&a);
    CHECK(s.used == 4 && s.free_blocks == 4 && s.largest_run == 3 && s.free_runs == 2);
    CHECK(!allocate(&a, "huge", 4, &start));
    CHECK(!free_blocks(&a, "missing"));
    allocator_destroy(&a);
    FILE *ops=tmpfile(), *report=tmpfile();
    CHECK(ops && report);
    CHECK(allocator_init(&a,8));
    fputs("ALLOC x 2\nALLOC x 1\n",ops); rewind(ops);
    CHECK(run_log(ops,report,&a)!=0);
    CHECK(fseek(report,0,SEEK_END)==0 && ftell(report)==0);
    allocator_destroy(&a); fclose(ops); fclose(report);
    ops=tmpfile(); report=tmpfile();
    CHECK(ops && report);
    CHECK(allocator_init(&a,8));
    const char hidden[]="ALLOC a 1\0garbage";
    CHECK(fwrite(hidden,1,sizeof(hidden)-1,ops)==sizeof(hidden)-1);
    rewind(ops);
    CHECK(run_log(ops,report,&a)!=0);
    CHECK(fseek(report,0,SEEK_END)==0 && ftell(report)==0);
    allocator_destroy(&a); fclose(ops); fclose(report);
    puts("allocator tests passed");
    return 0;
}
