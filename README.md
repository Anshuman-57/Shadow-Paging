# Shadow Paging Memory Debugger

A production-grade memory write tracking and debugging system implemented in C using page protection, shadow memory, and SIGSEGV-based write detection.

This project demonstrates how operating systems and debuggers can monitor memory modifications by leveraging virtual memory protection mechanisms.

---

## Features

- Memory write detection using page protection (`mprotect`)
- Shadow memory for change comparison
- SIGSEGV-based write interception
- Automatic write logging
- Timestamped memory modification history
- Deferred logging with worker thread
- Statistics collection
- Memory visualization support
- Change heatmaps and timeline analysis
- Thread-safe implementation

---

## Project Structure

```text
OS_Project/
│
├── memory_debugger.h        # Public API
├── memory_debugger.c        # Core implementation
├── demo.c                  # Demo application
├── Makefile                # Build configuration
│
├── logs/
│   └── memory_diff.log     # Generated write logs
│
├── visualizations/
│   ├── 00_dashboard.png
│   ├── 01_timeline.png
│   ├── 02_heatmap.png
│   ├── 03_evolution.png
│   ├── 04_shadow_diff.png
│   └── index.html
│
├── visualize.py            # Visualization script
│
├── timeline.png
├── heatmap.png
├── evolution.png
└── shadow_diff.png
```

---

## How It Works

### 1. Protected Memory Allocation

The debugger allocates a memory region and marks it as read-only using:

```c
mprotect(memory, size, PROT_READ);
```

---

### 2. Write Attempt

When a program tries to write:

```c
target[index] = value;
```

the CPU raises a:

```text
SIGSEGV
```

because the page is protected.

---

### 3. Signal Handler

The debugger intercepts the fault:

```c
SIGSEGV Handler
```

and temporarily makes the page writable.

---

### 4. Change Detection

After the write completes:

- Target memory is compared with shadow memory.
- Modified locations are identified.
- Old and new values are recorded.

---

### 5. Logging

Each write event is stored with:

- Timestamp
- Address
- Index
- Old value
- New value
- Write count

Example:

```text
Timestamp            Address      Index  Old Value  New Value
----------------------------------------------------------------
2025-01-01 12:00:00  0x12345678      2       100        5000
```

---

## Build Instructions

### Requirements

- GCC
- POSIX Threads
- Linux Environment

### Build

```bash
make
```

### Run

```bash
make run
```

or

```bash
./demo
```

---

## Demo Scenarios

The demo application includes:

### Sequential Writes

```text
Index 2 -> 5000
Index 5 -> 6000
Index 7 -> 7000
```

### Repeated Writes

```text
Index 2 -> 5001
Index 2 -> 5002
Index 2 -> 5003
```

### Random Writes

Random memory updates across different locations.

### Burst Writes

Rapid consecutive writes for stress testing.

### Same Value Writes

Tests detection when values do not actually change.

---

## Generated Statistics

The debugger reports:

```text
Total Writes
Total SIGSEGV Signals
Protection Errors
Average SIGSEGV per Write
```

---

## Visualizations

The project generates visual analysis of memory activity.

### Timeline

Tracks writes over time.

### Heatmap

Shows frequently modified memory locations.

### Evolution

Displays memory state transitions.

### Shadow Difference View

Highlights differences between target and shadow memory.

---

## Example API Usage

```c
debugger_init(10, sizeof(int), "logs/memory.log");

int *target = debugger_get_memory();

debugger_prepare_for_write();

target[2] = 5000;

debugger_write_complete();

debugger_cleanup();
```

---

## Learning Objectives

This project demonstrates concepts from:

- Operating Systems
- Virtual Memory
- Memory Protection
- Page Fault Handling
- Signal Processing
- Shadow Paging
- Debugging Systems
- Concurrent Programming
- Thread Synchronization

---

## Technologies Used

- C11
- POSIX Threads
- Linux Signals
- mprotect()
- mmap()
- GCC
- Python (Visualization)

---

## Future Improvements

- Multi-page memory tracking
- Real-time dashboard
- Web-based visualization
- Memory access profiling
- Per-thread write statistics
- Large-scale memory monitoring

---

## Author

Developed as an Operating Systems project demonstrating shadow paging and memory write-tracking techniques by Anshuman and Ayush Chaurasia.
