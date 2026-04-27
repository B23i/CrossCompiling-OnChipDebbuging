#define _POSIX_C_SOURCE 200809L

/* Debug Mode Configuration */
#define DEBUG_MODE 1
#define BREAKPOINT_ENABLED 1

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <setjmp.h>

/* Debug Macros */
#if DEBUG_MODE
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[DEBUG] %s:%d - " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) do {} while (0)
#endif

#if DEBUG_MODE && BREAKPOINT_ENABLED
#if defined(__aarch64__)
#define DEBUG_BREAK() do { __asm__ volatile("brk #0"); } while (0)
#elif defined(__arm__)
#define DEBUG_BREAK() do { __asm__ volatile("bkpt #0"); } while (0)
#else
#define DEBUG_BREAK() do { __builtin_trap(); } while (0)
#endif
#else
#define DEBUG_BREAK() do {} while (0)
#endif

#define ERROR_PRINT(fmt, ...) \
    fprintf(stderr, "[ERROR] %s:%d - " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

typedef struct
{
    const char *name;
    long period_ms;
    long exec_ms;
    int iterations;
} TaskConfig;


typedef struct 
{
    TaskConfig cfg;
    long long max_jitter_us;
    long long sum_jitter_us;
    long long max_exec_us;
    long long sum_exec_us;
    int deadline_miss;
} TaskStats;


static long long ts_to_us(const struct timespec *t){
    return (long long)t->tv_sec * 1000000LL + (long long)t->tv_nsec / 1000LL;
}

static struct timespec us_to_ts(long long us){
    struct timespec t;
    t.tv_sec = (time_t)(us / 1000000LL);
    t.tv_nsec = (long)((us % 1000000LL) * 1000LL);
    return t;
}


static struct timespec now_mono(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t;
}


static void sleep_until_abs(const struct timespec *abs_time) {
    int rc;
    do {
        rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, abs_time, NULL);
    } while (rc == EINTR);
    
    if (rc != 0) {
        ERROR_PRINT("clock_nanosleep failed: %s", strerror(rc));
        DEBUG_BREAK();
    }
}

static void *task_thread(void *arg) {
    /**
     * @brief Görev yapılandırma parametrelerini mikro saniye cinsine dönüştürür.
     *
     * @details
     * `arg` işaretçisi, görev istatistiklerini ve konfigürasyonunu taşıyan bir yapıya dönüştürülür.
     * Ardından periyot (`period_ms`) ve hedef çalışma süresi (`exec_ms`) milisaniyeden
     * mikro saniyeye çevrilerek zamanlama hesaplarında kullanılacak daha hassas değerlere atanır.
     *
     * - `period_us`: Görevin çağrılma aralığı (mikro saniye).
     * - `exec_us_target`: Görevin hedef işlem süresi (mikro saniye).
     *
     * `1000LL` kullanımı, çarpımın 64-bit tamsayı (`long long`) hassasiyetinde yapılmasını
     * sağlayarak taşma riskini azaltır.
     */
    TaskStats *s = (TaskStats *)arg;
    DEBUG_PRINT("Task [%s] started - Period: %ld ms, Exec: %ld ms, Iterations: %d",
                s->cfg.name, s->cfg.period_ms, s->cfg.exec_ms, s->cfg.iterations);
    
    long long period_us = s->cfg.period_ms * 1000LL;
    long long exec_us_target = s->cfg.exec_ms * 1000LL;

    struct timespec t0 = now_mono();
    long long next_release_us = ts_to_us(&t0) + period_us;

    for (int i = 0; i < s->cfg.iterations; i++){
        struct timespec release_ts = us_to_ts(next_release_us);
        sleep_until_abs(&release_ts);

        struct timespec actual_start = now_mono();
        long long actual_start_us = ts_to_us(&actual_start);

        long long jitter_us = actual_start_us - next_release_us;
        if (jitter_us < 0) jitter_us = 0;

        if (strcmp(s->cfg.name, "T1_10ms") == 0){
            struct timespec work_ts = us_to_ts(11000);
            nanosleep(&work_ts, NULL);
        } else {
            struct timespec work_ts = us_to_ts(exec_us_target);
            nanosleep(&work_ts, NULL);
        }

        struct timespec actual_end = now_mono();
        long long actual_end_us = ts_to_us(&actual_end);
        long long exec_time_us = actual_end_us - actual_start_us;

        long long deadline_us = next_release_us + period_us;
        if (actual_end_us > deadline_us){
            s->deadline_miss++;
            ERROR_PRINT("[%s] Deadline miss at iteration %d! Jitter: %lld us, Exec: %lld us",
                        s->cfg.name, i + 1, jitter_us, exec_time_us);
            DEBUG_BREAK();
        }

        if (jitter_us > s->max_jitter_us) s->max_jitter_us = jitter_us;
        if (exec_time_us > s->max_exec_us) s->max_exec_us = exec_time_us;
        s->sum_jitter_us += jitter_us;
        s->sum_exec_us += exec_time_us;

        if ((i + 1) % 20 == 0) {
            printf("[%s] iter=%d jitter=%lld us exec=%lld us miss=%d\n",
                   s->cfg.name, i + 1, jitter_us, exec_time_us, s->deadline_miss);
            fflush(stdout);
        }

        next_release_us += period_us;
    }
    
    DEBUG_PRINT("Task [%s] completed - Total misses: %d", s->cfg.name, s->deadline_miss);

    return NULL;
}

int main(void) {
    DEBUG_PRINT("Real-Time Scheduler starting...");
    
    TaskStats tasks[] = {
    {.cfg = {"T1_10ms", 10, 2, 200}, .max_jitter_us = 0, .sum_jitter_us = 0, 
     .max_exec_us = 0, .sum_exec_us = 0, .deadline_miss = 0},
    {.cfg = {"T2_50ms", 50, 8, 120}, .max_jitter_us = 0, .sum_jitter_us = 0, 
     .max_exec_us = 0, .sum_exec_us = 0, .deadline_miss = 0},
    {.cfg = {"T3_100ms", 100, 15, 80}, .max_jitter_us = 0, .sum_jitter_us = 0, 
     .max_exec_us = 0, .sum_exec_us = 0, .deadline_miss = 0},
    };

    int n = (int)(sizeof(tasks) / sizeof(tasks[0]));
    pthread_t tids[3];
    
    DEBUG_PRINT("Creating %d tasks...", n);
    

    for (int i = 0; i < n; i++) {
        int rc = pthread_create(&tids[i], NULL, task_thread, &tasks[i]);
        if (rc != 0) {
            ERROR_PRINT("pthread_create error (%s): %s",
                    tasks[i].cfg.name, strerror(rc));
            DEBUG_BREAK();
            return 1;
        }
        DEBUG_PRINT("Task %d [%s] created successfully", i, tasks[i].cfg.name);
    }

    for (int i = 0; i < n; i++) {
        int rc = pthread_join(tids[i], NULL);
        if (rc != 0) {
            ERROR_PRINT("pthread_join error for task %d: %s", i, strerror(rc));
            DEBUG_BREAK();
        }
        DEBUG_PRINT("Task %d joined", i);
    }

    printf("\n===== SUMMARY =====\n");
    for (int i = 0; i < n; i++) {
        double avg_jitter = (double)tasks[i].sum_jitter_us / tasks[i].cfg.iterations;
        double avg_exec = (double)tasks[i].sum_exec_us / tasks[i].cfg.iterations;

        printf("%s -> avg_jitter=%.2f us, max_jitter=%lld us, avg_exec=%.2f us, max_exec=%lld us, miss=%d\n",
               tasks[i].cfg.name,
               avg_jitter, tasks[i].max_jitter_us,
               avg_exec, tasks[i].max_exec_us,
               tasks[i].deadline_miss);
        
        if (tasks[i].deadline_miss > 0) {
            ERROR_PRINT("Task %s had %d deadline misses!", 
                       tasks[i].cfg.name, tasks[i].deadline_miss);
        }
    }
    
    DEBUG_PRINT("Real-Time Scheduler finished!");
    return 0;
}