#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include "../module/leaky.h"

void pin_to_core(int core) {
    cpu_set_t cpuset;
    pthread_t thread;

    thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
}

int main(void) {
    pin_to_core(7);

    int leaky_fd = open(LEAKY_DEVICE_PATH, O_RDONLY);
    if (leaky_fd < 0) {
        fprintf(stderr, "Error: Could not open userkernel device: %s\n", LEAKY_DEVICE_PATH);
        return 0;
    }
    
    ioctl(leaky_fd, LEAKY_IOCTL_CMD_INVD_TIMING, (size_t)0);
    
    exit(EXIT_SUCCESS);
}
