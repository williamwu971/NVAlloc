#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libpmem.h>
#include <unistd.h>
#include "memory.h"
#include "sys/mman.h"
#include "x86intrin.h"
#include <sys/time.h>
#include <pthread.h>

#include "nvalloc.h"
#include "ralloc.hpp"
#include "masstree.h"
#include "tbb/tbb.h"

#define NTIMES 1000000
//#define NTIMES 10

int times = NTIMES;

typedef struct foo_s
{
    int64_t a[8];
} foo_t;

static __thread __uint128_t g_lehmer64_state;

static void init_seed(void) {
    g_lehmer64_state = rand();
}

static uint64_t lehmer64() {
    g_lehmer64_state *= 0xda942042e4dd58b5;
    return g_lehmer64_state >> 64;
}

void shuffle(void *array, size_t nmemb, size_t size) {

//    printf("info: shuffling array %p nmemb %lu size %lu\n", array, nmemb, size);
    srand(time(NULL));


    char *buff = (char*)malloc(size);

    for (uint64_t i = 0; i < nmemb - 1; i++) {

        uint64_t j = i + rand() / (RAND_MAX / (nmemb - i) + 1);

        memcpy(buff, array + j * size, size);
        memcpy(array + j * size, array + i * size, size);
        memcpy(array + i * size, buff, size);
    }

    free(buff);
}

int main(int argc, char *argv[])
{

    nvalloc_init();
//    RP_init("NVAlloc",32*1024*1024*1024ULL);

//    struct timeval start;
//    struct timeval end;
//    unsigned long diff;
//    goto bench;
//
//    foo_t **a = (foo_t **)malloc(8 * times);
//
//    gettimeofday(&start, NULL);
//
//    for (int i = 0; i < times; i++)
//    {
//
//        nvalloc_malloc_to(64, (void **)&a[i]);
//
//
//    }
//
//    for (int i = 0; i < times; i++)
//    {
//        nvalloc_free_from((void **)&a[i]);
//
//    }
//
//    _mm_mfence();
//    gettimeofday(&end, NULL);
//
//    diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
//
//    printf("Total time is %ld us\n", diff);
//
//    gettimeofday(&start, NULL);
//
//    for (int i = 0; i < times; i++)
//    {
//        a[i] = (foo_t*)RP_malloc(64);
//
//    }
//
//    for (int i = 0; i < times; i++)
//    {
//        RP_free(a[i]);
//    }
//
//    _mm_mfence();
//    gettimeofday(&end, NULL);
//
//    diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
//
//    printf("Total time is %ld us\n", diff);
//
//
//    return 1;


    std::cout << "Simple Example of P-Masstree" << std::endl;

    uint64_t n = std::atoll(argv[1]);
    uint64_t *keys = new uint64_t[n];

    // Generate keys
    for (uint64_t i = 0; i < n; i++) {
        keys[i] = i + 1;
    }

    int num_thread = atoi(argv[2]);
    tbb::task_scheduler_init init(num_thread);

    printf("operation,n,ops/s\n");
    masstree::masstree *tree = new masstree::masstree();

    {
        // Build tree
//        shuffle(keys,n,sizeof(uint64_t));
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree->getThreadInfo();
            init_seed();
            for (uint64_t i = range.begin(); i != range.end(); i++) {

                void* value=nullptr;

//                value = RP_malloc(64);
                nvalloc_malloc_to(64, (void**)&value);

                memset(value,lehmer64(),64);

                tree->put(keys[i], value, t);
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
//        printf("Throughput: insert,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
//        printf("Elapsed time: insert,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    {
        // Build tree
        shuffle(keys,n,sizeof(uint64_t));
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree->getThreadInfo();
            init_seed();
            for (uint64_t i = range.begin(); i != range.end(); i++) {

                uint64_t* value=nullptr;

//                value = (uint64_t*)RP_malloc(64);
                nvalloc_malloc_to(64, (void**)&value);

                value[0] = keys[i];
                memset(value+1,lehmer64(),56);


                void* old = tree->put_and_return(keys[i], value, t);

                if (old== nullptr){
                    puts("wrong");
                    throw;
                }

//                RP_free(old);
                nvalloc_free_from((void**)&old);
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
        printf("Throughput: update,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
        printf("Elapsed time: update,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    {
        // Lookup
//        shuffle(keys,n,sizeof(uint64_t));
        auto starttime = std::chrono::system_clock::now();
        tbb::parallel_for(tbb::blocked_range<uint64_t>(0, n), [&](const tbb::blocked_range<uint64_t> &range) {
            auto t = tree->getThreadInfo();
            for (uint64_t i = range.begin(); i != range.end(); i++) {
                uint64_t *ret = reinterpret_cast<uint64_t *> (tree->get(keys[i], t));
                if (*ret != keys[i]) {
                    std::cout << "wrong value read: " << *ret << " expected:" << keys[i] << std::endl;
                    throw;
                }
            }
        });
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now() - starttime);
//        printf("Throughput: lookup,%ld,%f ops/us\n", n, (n * 1.0) / duration.count());
//        printf("Elapsed time: lookup,%ld,%f sec\n", n, duration.count() / 1000000.0);
    }

    delete[] keys;
}