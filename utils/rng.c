#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include "rng.h"

static int rng_initialized = 0;

void rng_init()
{
    /* Idempotente: udp_open y los main() llaman aqui, y volver a sembrar con
       time(NULL) dentro del mismo segundo reinicia la secuencia. El PID separa
       ademas a dos procesos arrancados a la vez. */
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
