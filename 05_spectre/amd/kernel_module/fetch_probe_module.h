#ifndef _FETCH_PROBE_MODULE_H
#define _FETCH_PROBE_MODULE_H

#define FETCH_PROBE_MODULE_DEVICE_NAME "fetch_probe_module"
#define FETCH_PROBE_MODULE_DEVICE_PATH "/dev/" FETCH_PROBE_MODULE_DEVICE_NAME

#define FETCH_PROBE_MODULE_IOCTL_MAGIC_NUMBER (long)0x18560

#define BUFFER_SIZE 4096

static uint64_t seed = 42;

static uint64_t rand64(void){
    return (seed = (164603309694725029ull * seed) % 14738995463583502973ull);
}

struct prefetchjection_kernel_info {
    uintptr_t kernel_access_off;
    uintptr_t kernel_buffer;
};

// branch based on secret bit with given index
#define CMD_GADGET _IOR(FETCH_PROBE_MODULE_IOCTL_MAGIC_NUMBER, 0, uint64_t)

// get information about kernel stuff
#define CMD_INFO   _IOR(FETCH_PROBE_MODULE_IOCTL_MAGIC_NUMBER, 1, struct prefetchjection_kernel_info*)

// reset secret buffer
#define CMD_RESET  _IOR(FETCH_PROBE_MODULE_IOCTL_MAGIC_NUMBER, 2, uint64_t)

#endif /* _FETCH_PROBE_H */
