/* ============================================================================
   demo.c - Memory Debugger Demo Application
   
   Demonstrates the usage of the memory debugger library
   with controlled write sequences and analysis
   ============================================================================ */

#include "memory_debugger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define ARRAY_SIZE 10

/* ============================================================================
   Demo Helper Functions
   ============================================================================ */

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
        printf("│%s %d │ %8d │ %8d │   %s   │\n",
               (i < 10) ? " " : "", i, target[i], shadow[i], changed);
    }
    printf("└──────┴──────────┴──────────┴─────────┘\n");
}

void print_stats(void) {
    DebuggerStats stats;
    debugger_get_stats(&stats);
    
    printf("\n📈 Debugger Statistics:\n");
    printf("   Total Writes:           %d\n", stats.total_writes);
    printf("   Total SIGSEGV Signals:  %d\n", stats.total_sigsegv);
    printf("   Protection Errors:      %d\n", stats.protection_errors);
    printf("   Avg SIGSEGV per Write:  %.2f\n", stats.sigsegv_per_write);
}

/* ============================================================================
   Demo Sequence Functions
   ============================================================================ */

void demo_write(int index, int new_value) {
    printf("   → Writing to index %d: %d\n", index, new_value);
    
    debugger_prepare_for_write();
    
    int *target = (int *)debugger_get_memory();
    target[index] = new_value;
    
    debugger_write_complete();
    
    /* Small delay to allow logging thread to process */
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 50000000;  /* 50ms */
    nanosleep(&ts, NULL);
}

void demo_sequence_1_sequential(void) {
    print_section("Sequence 1: Sequential Writes");
    printf("   Writing to indices 2, 5, 7 with different values\n");
    
    demo_write(2, 5000);
    demo_write(5, 6000);
    demo_write(7, 7000);
    
    print_memory_table();
}

void demo_sequence_2_repeated(void) {
    print_section("Sequence 2: Repeated Index (Multiple Writes to Same Location)");
    printf("   Writing multiple times to index 2 to test write count tracking\n");
    
    demo_write(2, 5001);
    demo_write(2, 5002);
    demo_write(2, 5003);
    
    print_memory_table();
}

void demo_sequence_3_random(void) {
    print_section("Sequence 3: Random Pattern");
    printf("   Various writes across different indices\n");
    
    demo_write(9, 9999);
    demo_write(1, 1111);
    demo_write(0, 100);
    demo_write(4, 4444);
    
    print_memory_table();
}

void demo_sequence_4_burst(void) {
    print_section("Sequence 4: Write Burst");
    printf("   Rapid consecutive writes to test batching\n");
    
    for (int i = 0; i < ARRAY_SIZE; i += 2) {
        demo_write(i, 2000 + i * 100);
    }
    
    print_memory_table();
}

void demo_sequence_5_same_value(void) {
    print_section("Sequence 5: Write Same Value (No Change Detection)");
    printf("   Writing same value multiple times\n");
    
    int *target = (int *)debugger_get_memory();
    int current_val = target[3];
    
    printf("   → Current value at index 3: %d\n", current_val);
    demo_write(3, current_val);
    
    printf("   → Writing same value again (should not log as change)\n");
    demo_write(3, current_val);
    
    print_memory_table();
}

/* ============================================================================
   Main Demo Application
   ============================================================================ */

int main(void) {
    print_header("Memory Debugger Demo Application v1.0");
    
    printf("\nInitializing debugger...\n");
    
    /* Initialize the debugger */
    if (debugger_init(ARRAY_SIZE, sizeof(int), "logs/memory_diff.log") == -1) {
        fprintf(stderr, "✗ Failed to initialize debugger\n");
        return 1;
    }
    
    printf("✓ Debugger initialized successfully\n");
    
    /* Initialize target memory with starting values */
    int *target = (int *)debugger_get_memory();
    for (int i = 0; i < ARRAY_SIZE; i++) {
        target[i] = 1000 + i;
    }
    
    /* Sync shadow memory */
    int *shadow = (int *)debugger_get_shadow();
    memcpy(shadow, target, ARRAY_SIZE * sizeof(int));
    
    printf("✓ Memory initialized: %d elements × %zu bytes\n", 
           ARRAY_SIZE, sizeof(int));
    printf("✓ Protection: READ-ONLY (will catch writes via SIGSEGV)\n");
    
    print_memory_table();
    
    /* ================================================================
       Run all demo sequences
       ================================================================ */
    
    print_header("Running Demo Sequences");
    
    demo_sequence_1_sequential();
    debugger_flush_logs(100);
    
    demo_sequence_2_repeated();
    debugger_flush_logs(100);
    
    demo_sequence_3_random();
    debugger_flush_logs(100);
    
    demo_sequence_4_burst();
    debugger_flush_logs(100);
    
    demo_sequence_5_same_value();
    debugger_flush_logs(100);
    
    /* ================================================================
       Final Report
       ================================================================ */
    
    print_header("Demo Complete - Final Report");
    
    printf("\n✓ All write sequences executed successfully\n");
    
    print_memory_table();
    print_stats();
    
    printf("\n📁 Log file: logs/memory_diff.log\n");
    printf("   (Use this file to analyze memory write patterns)\n");
    
    /* Cleanup */
    printf("\nCleaning up...\n");
    debugger_cleanup();
    printf("✓ Cleanup complete\n\n");
    
    return 0;
}