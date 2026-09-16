#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "rng.h"

static int rng_initialized = 0;

void rng_init()
{
    /* Idempotent: udp_open and the main() functions both call here, and
       reseeding with time(NULL) within the same second restarts the sequence.
       The PID also separates two processes started at the same time. */
    if (!rng_initialized)
    {
        srand((unsigned int)time(NULL) ^ (unsigned int)getpid());
        rng_initialized = 1;
    }
}

int rng_get_rand_in_range(int min, int max)
{
    return min + rand() / (RAND_MAX / (max - min + 1) + 1);
}
