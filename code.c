/* ============================================================================
 * MEMORY DEBUGGER - Complete C Implementation
 * Shadow Paging Analysis with Write Detection and Logging
 * ============================================================================ */

#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <ucontext.h>
#include <fcntl.h>
#include <pthread.h>
#include <limits.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>

#ifndef SIGSTKSZ
#define SIGSTKSZ 8192
#endif

#define SIGSTKSZ_CUSTOM (16 * SIGSTKSZ)
#define ARRAY_SIZE 10

typedef struct {
    volatile sig_atomic_t write_detected;
    volatile sig_atomic_t write_count;
} ElementState;

typedef struct {
    void *mem_ptr;
    void *shadow_ptr;
    size_t alloc_size;
    size_t aligned_size;
    size_t element_size;
    int num_elements;
    volatile sig_atomic_t protected;
    volatile sig_atomic_t mem_error;
} MemoryBlock;

typedef struct {
    MemoryBlock block;
    ElementState *elements;
    
    FILE *log_fp;
    int log_fd;
    const char *log_filename;
    volatile sig_atomic_t write_count;
    volatile sig_atomic_t sigsegv_count;
    volatile sig_atomic_t in_handler;
    volatile sig_atomic_t shutdown_requested;
    volatile sig_atomic_t handler_made_writable;
    
    pthread_mutex_t log_mutex;
    pthread_cond_t log_cond;
    pthread_t logger_thread;
    int logger_running;
    
    stack_t sig_stack;
} DebuggerState;

typedef struct {
    int total_writes;
    int total_sigsegv;
    int protection_errors;
    float sigsegv_per_write;
} DebuggerStats;

static DebuggerState g_state = {0};

/* ============================================================================
 * Logging Utilities
 * ============================================================================ */

static void log_header(void) {
    fprintf(g_state.log_fp, 
        "╔════════════════════════════════════════════════════════════════════════════════════════════╗\n"
        "║                               MEMORY WRITE LOG - Shadow Paging Debugger                                ║\n"
        "╚════════════════════════════════════════════════════════════════════════════════════════════╝\n\n"
    );
    
    fprintf(g_state.log_fp,
        "| %-19s | %-10s | %-14s | %5s | %10s | %10s | %3s |\n",
        "Timestamp", "Time (sec)", "Address", "Index", "Old Value", "New Value", "Cnt");
    
    fprintf(g_state.log_fp,
        "|---------------------|------------|----------------|-------|------------|------------|-----|\n");
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(g_state.log_fp, "\nLog Started: %s\n\n", timestamp);
}

static void log_statistics(int total_writes, int total_sigsegv, int errors) {
    fprintf(g_state.log_fp,
        "\n─────────────────────────────────────────────────────────────────────────\n"
        "  STATISTICS\n"
        "─────────────────────────────────────────────────────────────────────────\n\n"
        "  Total Writes Detected:    %d\n"
        "  Total SIGSEGV Signals:    %d\n"
        "  Protection Errors:        %d\n"
        "  Avg. SIGSEGV per Write:   %.2f\n\n",
        total_writes,
        total_sigsegv,
        errors,
        (total_sigsegv > 0) ? (float)total_sigsegv / total_writes : 0.0
    );
}

static void log_footer(void) {
    fprintf(g_state.log_fp,
        "|---------------------|------------|----------------|-------|------------|------------|-----|\n\n");
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    fprintf(g_state.log_fp,
        "╔════════════════════════════════════════════════════════════════════════════════════════════╗\n"
        "║  Log Complete: %s                                                        ║\n"
        "╚════════════════════════════════════════════════════════════════════════════════════════════╝\n",
        timestamp);
}

/* ============================================================================
 * Signal Handler
 * ============================================================================ */

static void sigsegv_handler(int sig, siginfo_t *info, void *context) {
    (void)context;
    
    if (sig != SIGSEGV) return;
    if (g_state.in_handler) return;
    
    g_state.in_handler = 1;
    
    uintptr_t fault_addr = (uintptr_t)info->si_addr;
    uintptr_t mem_start = (uintptr_t)g_state.block.mem_ptr;
    uintptr_t mem_end = mem_start + g_state.block.aligned_size;
    
    if (fault_addr < mem_start || fault_addr >= mem_end) {
        g_state.in_handler = 0;
        return;
    }
    
    int index = (fault_addr - mem_start) / g_state.block.element_size;
    if (index < 0 || index >= g_state.block.num_elements) {
        g_state.in_handler = 0;
        return;
    }
    
    g_state.sigsegv_count++;
    g_state.elements[index].write_detected = 1;
    g_state.elements[index].write_count++;
    
    if (mprotect(g_state.block.mem_ptr, g_state.block.aligned_size,
                 PROT_READ | PROT_WRITE) == -1) {
        g_state.block.mem_error = 1;
    } else {
        g_state.block.protected = 0;
        g_state.handler_made_writable = 1;
    }
    
    g_state.in_handler = 0;
}

static void install_signal_handler_safe(void) {
    g_state.sig_stack.ss_sp = malloc(SIGSTKSZ_CUSTOM);
    if (!g_state.sig_stack.ss_sp) {
        perror("malloc sig_stack");
        exit(1);
    }
    g_state.sig_stack.ss_size = SIGSTKSZ_CUSTOM;
    g_state.sig_stack.ss_flags = 0;
    
    if (sigaltstack(&g_state.sig_stack, NULL) == -1) {
        perror("sigaltstack");
        exit(1);
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = sigsegv_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

/* ============================================================================
 * Write Detection & Logging
 * ============================================================================ */

static void detect_and_log_writes(void) {
    uint8_t *shadow_bytes = (uint8_t *)g_state.block.shadow_ptr;
    uint8_t *target_bytes = (uint8_t *)g_state.block.mem_ptr;
    
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    int detected_any = 0;
    
    for (int i = 0; i < g_state.block.num_elements; i++) {
        if (g_state.elements[i].write_detected) {
            uint8_t *old_ptr = shadow_bytes + (i * g_state.block.element_size);
            uint8_t *new_ptr = target_bytes + (i * g_state.block.element_size);
            
            int old_val = 0, new_val = 0;
            memcpy(&old_val, old_ptr, sizeof(int));
            memcpy(&new_val, new_ptr, sizeof(int));
            
            if (old_val != new_val) {
                time_t now = tv.tv_sec;
                struct tm tm_info;
                localtime_r(&now, &tm_info);
                char timestamp[64];
                strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_info);
                
                double precise_time = tv.tv_sec + (tv.tv_usec / 1e6);
                
                fprintf(g_state.log_fp,
                    "| %-19s | %-10.3f | %-14p | %5d | %10d | %10d | %3d |\n",
                    timestamp, precise_time,
                    (void*)((uintptr_t)g_state.block.mem_ptr + (i * g_state.block.element_size)),
                    i, old_val, new_val,
                    g_state.elements[i].write_count);
                
                memcpy(old_ptr, new_ptr, g_state.block.element_size);
                g_state.write_count++;
                detected_any = 1;
                
                printf(" [WRITE] idx=%2d | old=%6d → new=%6d | count=%d\n",
                    i, old_val, new_val, g_state.elements[i].write_count);
            }
            
            g_state.elements[i].write_detected = 0;
        }
    }
    
    if (detected_any) {
        fflush(g_state.log_fp);
    }
}

/* ============================================================================
 * Protection Management
 * ============================================================================ */

static int restore_readonly_protection(void) {
    if (!g_state.handler_made_writable) {
        return 0;
    }
    
    g_state.handler_made_writable = 0;
    
    if (mprotect(g_state.block.mem_ptr, g_state.block.aligned_size, PROT_READ) == -1) {
        g_state.block.mem_error = 1;
        return -1;
    }
    
    g_state.block.protected = 1;
    return 0;
}

/* ============================================================================
 * Logging Thread
 * ============================================================================ */

static void* logging_thread(void *arg) {
    (void)arg;
    
    while (!g_state.shutdown_requested) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 1;
        
        pthread_mutex_lock(&g_state.log_mutex);
        pthread_cond_timedwait(&g_state.log_cond, &g_state.log_mutex, &timeout);
        pthread_mutex_unlock(&g_state.log_mutex);
        
        detect_and_log_writes();
    }
    
    detect_and_log_writes();
    log_statistics(g_state.write_count, g_state.sigsegv_count, g_state.block.mem_error);
    log_footer();
    fflush(g_state.log_fp);
    
    return NULL;
}

/* ============================================================================
 * Memory Management
 * ============================================================================ */

static size_t get_page_size(void) {
    long page_size = sysconf(_SC_PAGESIZE);
    return (page_size > 0) ? (size_t)page_size : 4096;
}

static size_t align_to_page(size_t size, size_t page_size) {
    return ((size + page_size - 1) / page_size) * page_size;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

int debugger_init(int element_count, size_t element_size, const char *log_file) {
    size_t page_size = get_page_size();
    
    g_state.log_filename = log_file;
    g_state.block.num_elements = element_count;
    g_state.block.element_size = element_size;
    g_state.block.alloc_size = element_count * element_size;
    g_state.block.aligned_size = align_to_page(g_state.block.alloc_size, page_size);
    
    g_state.block.mem_ptr = mmap(NULL, g_state.block.aligned_size,
                                 PROT_READ | PROT_WRITE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (g_state.block.mem_ptr == MAP_FAILED) {
        perror("mmap target");
        return -1;
    }
    
    if (posix_memalign(&g_state.block.shadow_ptr, page_size,
                       g_state.block.alloc_size) != 0) {
        perror("posix_memalign shadow");
        munmap(g_state.block.mem_ptr, g_state.block.aligned_size);
        return -1;
    }
    
    g_state.elements = (ElementState *)calloc(element_count, sizeof(ElementState));
    if (!g_state.elements) {
        perror("malloc elements");
        free(g_state.block.shadow_ptr);
        munmap(g_state.block.mem_ptr, g_state.block.aligned_size);
        return -1;
    }
    
    memcpy(g_state.block.shadow_ptr, g_state.block.mem_ptr,
           g_state.block.alloc_size);
    
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    
    int mutex_type = PTHREAD_MUTEX_ERRORCHECK;
    #ifdef PTHREAD_MUTEX_ERRORCHECK_NP
    mutex_type = PTHREAD_MUTEX_ERRORCHECK_NP;
    #endif
    
    pthread_mutexattr_settype(&mattr, mutex_type);
    
    if (pthread_mutex_init(&g_state.log_mutex, &mattr) != 0) {
        perror("pthread_mutex_init");
        goto error;
    }
    
    if (pthread_cond_init(&g_state.log_cond, NULL) != 0) {
        perror("pthread_cond_init");
        goto error;
    }
    
    pthread_mutexattr_destroy(&mattr);
    
    if (mkdir("logs", 0755) == -1 && errno != EEXIST) {
        perror("mkdir");
        goto error;
    }
    
    g_state.log_fd = open(log_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_state.log_fd < 0) {
        perror("open log");
        goto error;
    }
    
    g_state.log_fp = fdopen(g_state.log_fd, "w");
    if (!g_state.log_fp) {
        perror("fdopen");
        close(g_state.log_fd);
        goto error;
    }
    
    log_header();
    
    if (pthread_create(&g_state.logger_thread, NULL, logging_thread, NULL) != 0) {
        perror("pthread_create");
        fclose(g_state.log_fp);
        goto error;
    }
    
    g_state.logger_running = 1;
    
    if (mprotect(g_state.block.mem_ptr, g_state.block.aligned_size, PROT_READ) == -1) {
        perror("mprotect readonly");
        goto error;
    }
    
    g_state.block.protected = 1;
    g_state.handler_made_writable = 0;
    
    install_signal_handler_safe();
    
    return 0;

error:
    free(g_state.elements);
    free(g_state.block.shadow_ptr);
    munmap(g_state.block.mem_ptr, g_state.block.aligned_size);
    return -1;
}

void debugger_cleanup(void) {
    g_state.shutdown_requested = 1;
    pthread_cond_signal(&g_state.log_cond);
    
    if (g_state.logger_running) {
        pthread_join(g_state.logger_thread, NULL);
        g_state.logger_running = 0;
    }
    
    if (g_state.block.mem_ptr && g_state.block.protected) {
        mprotect(g_state.block.mem_ptr, g_state.block.aligned_size,
               PROT_READ | PROT_WRITE);
    }
    
    if (g_state.block.mem_ptr) {
        munmap(g_state.block.mem_ptr, g_state.block.aligned_size);
    }
    
    if (g_state.block.shadow_ptr) {
        free(g_state.block.shadow_ptr);
    }
    
    if (g_state.elements) {
        free(g_state.elements);
    }
    
    if (g_state.log_fp) {
        fclose(g_state.log_fp);
    }
    
    pthread_mutex_destroy(&g_state.log_mutex);
    pthread_cond_destroy(&g_state.log_cond);
    
    if (g_state.sig_stack.ss_sp) {
        free(g_state.sig_stack.ss_sp);
    }
}

void* debugger_get_memory(void) {
    return g_state.block.mem_ptr;
}

void* debugger_get_shadow(void) {
    return g_state.block.shadow_ptr;
}

int debugger_prepare_for_write(void) {
    if (!g_state.block.protected) {
        return restore_readonly_protection();
    }
    return 0;
}

int debugger_write_complete(void) {
    if (!g_state.block.protected) {
        if (restore_readonly_protection() == -1) {
            return -1;
        }
    }
    pthread_cond_signal(&g_state.log_cond);
    return 0;
}

void debugger_get_stats(DebuggerStats *stats) {
    stats->total_writes = g_state.write_count;
    stats->total_sigsegv = g_state.sigsegv_count;
    stats->protection_errors = g_state.block.mem_error;
    stats->sigsegv_per_write = (g_state.sigsegv_count > 0) ?
        (float)g_state.sigsegv_count / g_state.write_count : 0.0;
}

void debugger_flush_logs(int timeout_ms) {
    struct timespec ts;
    ts.tv_sec = timeout_ms / 1000;
    ts.tv_nsec = (timeout_ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/* ============================================================================
 * Demo Application
 * ============================================================================ */

void print_header(const char *title) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║  %-58s ║\n", title);
    printf("╚════════════════════════════════════════════════════════════╝\n");
}

void print_section(const char *title) {
    printf("\n[%s]\n", title);
}

void print_memory_table(void) {
    int *target = (int *)debugger_get_memory();
    int *shadow = (int *)debugger_get_shadow();
    
    printf("\n📊 Memory State:\n");
    printf("┌──────┬──────────┬──────────┬─────────┐\n");
    printf("│ Idx  │  Target  │  Shadow  │ Changed │\n");
    printf("├──────┼──────────┼──────────┼─────────┤\n");
    
    for (int i = 0; i < ARRAY_SIZE; i++) {
        const char *changed = (target[i] != shadow[i]) ? "Y" : "N";
        printf("│%s %d │ %8d │ %8d │    %s    │\n",
               (i < 10) ? " " : "", i, target[i], shadow[i], changed);
    }
    printf("└──────┴──────────┴──────────┴─────────┘\n");
}

void print_stats(void) {
    DebuggerStats stats;
    debugger_get_stats(&stats);
    
    printf("\n📈 Debugger Statistics:\n");
    printf("    Total Writes:          %d\n", stats.total_writes);
    printf("    Total SIGSEGV Signals:   %d\n", stats.total_sigsegv);
    printf("    Protection Errors:     %d\n", stats.protection_errors);
    printf("    Avg SIGSEGV per Write: %.2f\n", stats.sigsegv_per_write);
}

void demo_write(int index, int new_value) {
    printf("    → Writing to index %d: %d\n", index, new_value);
    
    debugger_prepare_for_write();
    
    int *target = (int *)debugger_get_memory();
    target[index] = new_value;
    
    debugger_write_complete();
    
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50000000;
    nanosleep(&ts, NULL);
}

void demo_sequence_1_sequential(void) {
    print_section("Sequence 1: Sequential Writes");
    printf("    Writing to indices 2, 5, 7 with different values\n");
    
    demo_write(2, 5000);
    demo_write(5, 6000);
    demo_write(7, 7000);
    
    print_memory_table();
}

void demo_sequence_2_repeated(void) {
    print_section("Sequence 2: Repeated Index");
    printf("    Writing multiple times to index 2\n");
    
    demo_write(2, 5001);
    demo_write(2, 5002);
    demo_write(2, 5003);
    
    print_memory_table();
}

void demo_sequence_3_random(void) {
    print_section("Sequence 3: Random Pattern");
    printf("    Various writes across different indices\n");
    
    demo_write(9, 9999);
    demo_write(1, 1111);
    demo_write(0, 100);
    demo_write(4, 4444);
    
    print_memory_table();
}

void demo_sequence_4_burst(void) {
    print_section("Sequence 4: Write Burst");
    printf("    Rapid consecutive writes\n");
    
    for (int i = 0; i < ARRAY_SIZE; i += 2) {
        demo_write(i, 2000 + i * 100);
    }
    
    print_memory_table();
}

void demo_sequence_5_same_value(void) {
    print_section("Sequence 5: Write Same Value");
    printf("    Writing same value multiple times\n");
    
    int *target = (int *)debugger_get_memory();
    int current_val = target[3];
    
    printf("    → Current value at index 3: %d\n", current_val);
    demo_write(3, current_val);
    demo_write(3, current_val);
    
    print_memory_table();
}

int main(void) {
    print_header("Memory Debugger Demo Application v1.0");
    
    printf("\nInitializing debugger...\n");
    
    if (debugger_init(ARRAY_SIZE, sizeof(int), "logs/memory_diff.log") == -1) {
        fprintf(stderr, "✗ Failed to initialize debugger\n");
        return 1;
    }
    
    printf("✓ Debugger initialized successfully\n");
    
    int *target = (int *)debugger_get_memory();
    for (int i = 0; i < ARRAY_SIZE; i++) {
        target[i] = 1000 + i;
    }
    
    int *shadow = (int *)debugger_get_shadow();
    memcpy(shadow, target, ARRAY_SIZE * sizeof(int));
    
    printf("✓ Memory initialized: %d elements × %zu bytes\n", 
           ARRAY_SIZE, sizeof(int));
    printf("✓ Protection: READ-ONLY (will catch writes via SIGSEGV)\n");
    
    print_memory_table();
    
    print_header("Running Demo Sequences");

    // ===================================================================
    //  TIMING LOGIC STARTS HERE
    // ===================================================================
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    demo_sequence_1_sequential();
    // debugger_flush_logs(100); // <-- REMOVED for accurate timing
    
    demo_sequence_2_repeated();
    // debugger_flush_logs(100); // <-- REMOVED for accurate timing
    
    demo_sequence_3_random();
    // debugger_flush_logs(100); // <-- REMOVED for accurate timing
    
    demo_sequence_4_burst();
    // debugger_flush_logs(100); // <-- REMOVED for accurate timing
    
    demo_sequence_5_same_value();
    // debugger_flush_logs(100); // <-- REMOVED for accurate timing
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    // ===================================================================
    //  TIMING LOGIC ENDS HERE
    // ===================================================================
    
    // This must match the count in baseline.c (17 writes total in your demo)
    #define NUM_WRITES 17 

    double time_taken = (end.tv_sec - start.tv_sec) + 
                        (end.tv_nsec - start.tv_nsec) / 1e9;
    
    double avg_latency_us = (time_taken / NUM_WRITES) * 1e6;
    
    print_header("Demo Complete - Final Report");

    // --- Print the timing results ---
    printf("\n--- Proposed System Results ---\n");
    printf("Total time for %d writes: %f seconds\n", NUM_WRITES, time_taken);
    printf("Average Latency per write: %.2f microseconds (µs)\n", avg_latency_us);
    
    printf("\n✓ All write sequences executed successfully\n");
    
    print_memory_table();
    print_stats();
    
    printf("\n📁 Log file: logs/memory_diff.log\n");
    printf("   (Use this file to analyze memory write patterns)\n");
    
    printf("\nCleaning up...\n");
    debugger_cleanup();
    printf("✓ Cleanup complete\n\n");
    
    return 0;
}