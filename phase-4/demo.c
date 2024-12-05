#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    // Check if an argument is provided
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <iterations>\n", argv[0]);
        return 1;
    }

    // Parse the number of iterations
    int iterations = atoi(argv[1]);
    if (iterations <= 0) {
        fprintf(stderr, "Error: iterations must be a positive integer.\n");
        return 1;
    }

    // Simulate workload
    for (int i = 0; i <= iterations; i++) {
        printf("Demo %d/%d\n", i, iterations);
        fflush(stdout); // Ensure the output is flushed immediately
        sleep(1);       // Simulate work by sleeping for 1 second
    }

    fflush(stdout);
    return 0;
}
