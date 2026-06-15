#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main() {
    int *arr = (int*)malloc(10 * sizeof(int));
    if (!arr) {
        perror("malloc failed");
        return 1;
    }
    memset(arr, 0, 10 * sizeof(int));
    
    printf("PID: %d, Heap address: %p\n", getpid(), arr);
    fflush(stdout);
    
    sleep(30);
    
    printf("DEBUG: Writing arr[0] = 10\n");
    fflush(stdout);
    arr[0] = 10;
    sleep(3);
    printf("DEBUG: Writing arr[1] = 20\n");
    fflush(stdout);
    arr[1] = 20;
    sleep(3);
    printf("DEBUG: Writing arr[2] = 30\n");
    fflush(stdout);
    arr[2] = 30;
    sleep(20);
    free(arr);
    printf("DEBUG: Freed memory and exiting\n");
    fflush(stdout);
    return 0;
}
