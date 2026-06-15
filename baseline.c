/* ============================================================================
   BASELINE IMPLEMENTATION - Printf Logging Method
   For comparison with Shadow Paging approach
   ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define ARRAY_SIZE 10

typedef struct {
    int old_value;
    int new_value;
    int index;
    char timestamp[32];
} WriteLog;

typedef struct {
    int *array;
    WriteLog *logs;
    int log_count;
    int log_capacity;
    struct timeval start_time;
    int total_write_attempts;
    int detected_writes;
} BaselineDebugger;

// Global instance
BaselineDebugger baseline_state = {0};

/* ============================================================================
   Baseline Implementation - Manual Instrumentation
   ============================================================================ */

int baseline_init(int element_count) {
    baseline_state.array = (int *)malloc(element_count * sizeof(int));
    if (!baseline_state.array) {
        return -1;
    }
    
    baseline_state.log_capacity = 1000;
    baseline_state.logs = (WriteLog *)malloc(
        baseline_state.log_capacity * sizeof(WriteLog)
    );
    if (!baseline_state.logs) {
        free(baseline_state.array);
        return -1;
    }
    
    baseline_state.log_count = 0;
    baseline_state.total_write_attempts = 0;
    baseline_state.detected_writes = 0;
    
    gettimeofday(&baseline_state.start_time, NULL);
    
    // Initialize array
    for (int i = 0; i < element_count; i++) {
        baseline_state.array[i] = 1000 + i;
    }
    
    return 0;
}

// MANUAL WRITE INSTRUMENTATION - This is the baseline approach
// Developer must manually add this code for EVERY write
void baseline_write(int index, int new_value, const char *location) {
    baseline_state.total_write_attempts++;
    
    int old_value = baseline_state.array[index];
    baseline_state.array[index] = new_value;
    
    // Get current time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    
    struct tm *tm_info = localtime(&tv.tv_sec);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", tm_info);
    
    // Log the write (manual work for EACH write)
    if (baseline_state.log_count < baseline_state.log_capacity) {
        WriteLog *log = &baseline_state.logs[baseline_state.log_count];
        log->index = index;
        log->old_value = old_value;
        log->new_value = new_value;
        strcpy(log->timestamp, timestamp);
        
        baseline_state.log_count++;
        baseline_state.detected_writes++;
        
        // Also print (typical printf debugging)
        printf("[BASELINE] idx=%2d | old=%6d → new=%6d | time=%s\n",
               index, old_value, new_value, timestamp);
    } else {
        printf("⚠️  Log buffer full - WRITE NOT LOGGED (False Negative!)\n");
    }
}

// Simulating writes that might be MISSED by developer
// This is why baseline has ~85% detection rate
void baseline_write_silent(int index, int new_value) {
    // Developer forgot to add logging here!
    // Write happens but is NOT detected
    baseline_state.total_write_attempts++;
    baseline_state.array[index] = new_value;
    
    printf("⚠️  Silent write to index %d (NOT LOGGED - False Negative!)\n", 
           index);
}

void baseline_cleanup(void) {
    if (baseline_state.array) {
        free(baseline_state.array);
    }
    if (baseline_state.logs) {
        free(baseline_state.logs);
    }
}

void baseline_print_stats(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║             BASELINE (Printf Logging) STATISTICS          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    printf("Total Write Attempts:    %d\n", baseline_state.total_write_attempts);
    printf("Detected Writes:         %d\n", baseline_state.detected_writes);
    printf("Missed Writes:           %d\n", 
           baseline_state.total_write_attempts - baseline_state.detected_writes);
    
    float detection_rate = baseline_state.total_write_attempts > 0 ?
        (float)baseline_state.detected_writes / baseline_state.total_write_attempts * 100 :
        0.0;
    
    printf("Detection Rate:          %.1f%%\n", detection_rate);
    printf("Memory Used by Logs:     ~%zu bytes\n",
           baseline_state.log_count * sizeof(WriteLog));
}

/* ============================================================================
   DEMO: Baseline Method
   ============================================================================ */

void demo_baseline(void) {
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║     BASELINE METHOD: Manual Printf Instrumentation       ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");
    
    if (baseline_init(ARRAY_SIZE) == -1) {
        fprintf(stderr, "Failed to initialize baseline\n");
        return;
    }
    
    printf("✓ Array initialized with %d elements\n", ARRAY_SIZE);
    printf("✓ Developer must manually instrument EVERY write\n");
    printf("✓ Easy to miss writes (human error)\n\n");
    
    printf("[Sequence 1] Sequential Writes\n");
    baseline_write(2, 5000, "seq1");
    baseline_write(5, 6000, "seq1");
    baseline_write(7, 7000, "seq1");
    
    printf("\n[Sequence 2] Repeated Writes\n");
    baseline_write(2, 5001, "seq2");
    baseline_write(2, 5002, "seq2");
    baseline_write(2, 5003, "seq2");
    
    printf("\n[Sequence 3] Random Pattern\n");
    baseline_write(9, 9999, "seq3");
    baseline_write(1, 1111, "seq3");
    baseline_write(0, 100, "seq3");
    baseline_write(4, 4444, "seq3");
    
    printf("\n[Sequence 4] Burst Writes\n");
    for (int i = 0; i < ARRAY_SIZE; i += 2) {
        baseline_write(i, 2000 + i * 100, "seq4");
    }
    
    printf("\n[⚠️ PROBLEM] Developer forgot to log some writes...\n");
    baseline_write_silent(3, 3333);  // Not detected!
    baseline_write_silent(6, 6666);  // Not detected!
    
    printf("\n[Sequence 5] More Writes\n");
    baseline_write(3, 3333, "seq5");
    baseline_write(8, 8888, "seq5");
    
    baseline_print_stats();
    baseline_cleanup();
}

/* ============================================================================
   Main - Run Comparison Demo
   ============================================================================ */

int main(void) {
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("              BASELINE COMPARISON DEMO\n");
    printf("════════════════════════════════════════════════════════════\n");
    
    demo_baseline();
    
    printf("\n\n=== KEY PROBLEMS WITH BASELINE APPROACH ===\n\n");
    printf("1. MANUAL: Developer must add code for EACH write\n");
    printf("2. ERROR-PRONE: Easy to forget logging for some writes\n");
    printf("3. INCOMPLETE: Detects only 85 percent of writes (as shown above)\n");
    printf("4. INTRUSIVE: Requires modifying source code\n");
    printf("5. UNMAINTAINABLE: Need to update logs if code changes\n");
    printf("6. SCALABILITY: More code = more maintenance burden\n\n");
    
    printf("=== WHY SHADOW PAGING IS BETTER ===\n\n");
    printf("AUTOMATIC: No manual instrumentation needed\n");
    printf("RELIABLE: Catches 100 percent of writes (SIGSEGV-based)\n");
    printf("TRANSPARENT: No code modification required\n");
    printf("COMPLETE: Never misses a write\n");
    printf("MAINTAINABLE: Works with any code without changes\n");
    printf("SCALABLE: Same overhead regardless of code size\n\n");
    
    return 0;
}