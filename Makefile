# ============================================================================
# Makefile - Memory Debugger Project Build Configuration
# ============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -g -O2 -pthread
LDFLAGS = -pthread

TARGET = demo
OBJS = memory_debugger.o demo.o
HEADERS = memory_debugger.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)
	@echo "✓ Build complete: ./$(TARGET)"

memory_debugger.o: memory_debugger.c $(HEADERS)
	$(CC) $(CFLAGS) -c memory_debugger.c

demo.o: demo.c $(HEADERS)
	$(CC) $(CFLAGS) -c demo.c

clean:
	rm -f $(OBJS) $(TARGET)
	@echo "✓ Clean complete"

clean-logs:
	rm -rf logs/
	@echo "✓ Logs cleaned"

distclean: clean clean-logs
	@echo "✓ Distribution clean complete"

run: $(TARGET)
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  make              - Build the project"
	@echo "  make run          - Build and run the demo"
	@echo "  make clean        - Remove object files and executable"
	@echo "  make clean-logs   - Remove log files"
	@echo "  make distclean    - Remove all generated files"
	@echo "  make help         - Show this help message"

.PHONY: all clean clean-logs distclean run help