#include <stdio.h>
#include <stdlib.h>

//array filled with characters
const char *blocks[] = {"square", "Lleft", "Lright", "zigzagleft", "zigzagright", "straight"};

// unbiased dice roll 1..6 using rejection sampling
static int roll_d6(void) {
    const int n = 6;
    int limit = RAND_MAX - (RAND_MAX % n);
    int r;
    do { r = rand(); } while (r >= limit);
    return (r % n);
}

// reads a number from the user
static unsigned int get_seed(void) {
    unsigned int s;
    printf("Enter a seed: ");
    if (scanf("%u", &s) != 1) { fprintf(stderr, "Invalid input\n"); exit(1); }
    return s;
}


int main(void) {
    unsigned int s = get_seed();  // ONLY the user input
    srand(s ? s : 1);             // avoid the (rare) zero edge case
    for (int i = 0; i < 10; ++i) {
        const char *x = blocks[roll_d6()];
        printf("%s\n", x);
    }
    return 0;
}
