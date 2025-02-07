#pragma once

#include <linux/ioctl.h>
#include <linux/types.h>

#define MAGIC 'P'

#define PF_CONTROL _IOWR(MAGIC, 17, struct pf_control)

#define MEASURE_US _IOWR(MAGIC, 24, struct measurement_t)

struct measurement_t {
    __u32 energy_pkg;
    __u32 energy_pp0;
    __u32 energy_dram;
    __u64 cycles;
    __u64 voltage;
    __u64 pstate;
    __u64 temperature;
    __u64 aperf;
    __u64 mperf;
    __u64 reg_diff;
    __u64 inst_a;
    __u64 inst_b;
    //__u64 c1_residency;
    //__u64 c3_residency;
    //__u64 c6_residency;
    //__u64 c7_residency;
    __u64 temp_a;
    __u64 temp_b;
} __attribute__((__packed__));
