#ifndef TCP_IP_STACK_RNG_H
#define TCP_IP_STACK_RNG_H

void rng_init();
int rng_get_rand_in_range(int min, int max);

#endif //TCP_IP_STACK_RNG_H
