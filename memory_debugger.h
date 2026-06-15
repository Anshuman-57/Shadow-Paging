/* ============================================================================
   memory_debugger.h - Shadow Paging Memory Debugger
   
   Production-grade memory write tracking library with:
   - Correct write detection: allow write, detect completion, restore protection
   - Per-write protection cycles to catch all writes
   - Atomic write completion detection
   - No race conditions in protection state
   - Deferred logging with batch processing
   ============================================================================ */

#ifndef MEMORY_DEBUGGER_H
#define MEMORY_DEBUGGER_H

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
   Public API Types
   ============================================================================ */

typedef struct {
    time_t unix_time;
    uint32_t milliseconds;
    int index;
    uintptr_t address;
    int old_value;
    int new_value;
    uint64_t write_number;
} WriteEvent;

typedef struct {
    int total_writes;
    int total_sigsegv;
    int protection_errors;
    float sigsegv_per_write;
} DebuggerStats;

/* ============================================================================
   Public API Functions
   ============================================================================ */

/**
 * Initialize the memory debugger
 * @param element_count: number of elements to track
 * @param element_size: size of each element in bytes
 * @param log_file: path to log file (e.g., "logs/memory.log")
 * @return 0 on success, -1 on failure
 */
int debugger_init(int element_count, size_t element_size, const char *log_file);

/**
 * Cleanup debugger resources
 * Must be called before program exit
 */
void debugger_cleanup(void);

/**
 * Get pointer to tracked memory region
 * @return pointer to allocated memory
 */
void* debugger_get_memory(void);

/**
 * Get pointer to shadow memory region
 * @return pointer to shadow copy
 */
void* debugger_get_shadow(void);

/**
 * Ensure memory is read-only before write attempt
 * @return 0 on success, -1 on failure
 */
int debugger_prepare_for_write(void);

/**
 * Mark write operation complete and restore protection
 * Triggers logging thread to process changes
 * @return 0 on success, -1 on failure
 */
int debugger_write_complete(void);

/**
 * Get current debugger statistics
 * @param stats: pointer to output stats structure
 */
void debugger_get_stats(DebuggerStats *stats);

/**
 * Wait for pending writes to be logged
 * @param timeout_ms: maximum wait time in milliseconds
 */
void debugger_flush_logs(int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* MEMORY_DEBUGGER_H */