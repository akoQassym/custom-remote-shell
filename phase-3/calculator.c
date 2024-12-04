#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <operator> <num1> <num2>\n", argv[0]);
        return 1;
    }

    char operator = argv[1][0];
    double num1 = atof(argv[2]);
    double num2 = atof(argv[3]);
    double result;

    if (operator == '+') {
        result = num1 + num2;
    } else if (operator == '-') {
        result = num1 - num2;
    } else {
        fprintf(stderr, "Error: Invalid operator. Use + or -\n");
        return 1;
    }

    printf("Result: %.2f\n", result);
    return 0;
}