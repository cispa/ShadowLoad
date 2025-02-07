#pragma once

#include "../module/interface.h"

#include <array>
#include <asm-generic/mman-common.h>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <sched.h>
#include <span>
#include <sstream>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

extern int fd;

inline void measure_us(measurement_t &data, uint32_t us) {
    data.cycles = us;
    if ( ioctl(fd, MEASURE_US, &data) < 0 ) {
        perror("ioctl error\n");
        exit(-1);
    }
}
