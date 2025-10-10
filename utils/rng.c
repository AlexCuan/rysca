#include <stdlib.h>
#include <time.h>
#include "rng.h"

void rng_init() {
    srand(time(NULL));
}

int rng_get_rand_in_range(int min, int max) {
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}
