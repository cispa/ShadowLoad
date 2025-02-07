#ifndef _FETCHPROBE_MODULE_H
#define _FETCHPROBE_MODULE_H

#define FETCHPROBE_MODULE_DEVICE_NAME "prefetchjection_module"
#define FETCHPROBE_MODULE_DEVICE_PATH "/dev/" FETCHPROBE_MODULE_DEVICE_NAME

#define FETCHPROBE_MODULE_IOCTL_MAGIC_NUMBER (long)0x225f63

#define BUFFER_SIZE 4096

static uint64_t seed = 42;

static uint64_t rand64(void){
    return (seed = (164603309694725029ull * seed) % 14738995463583502973ull);
}

struct fetchprobe_kernel_info {
    uintptr_t kernel_access_cf;
    uintptr_t kernel_access_off;
    uintptr_t kernel_buffer;
};

// branch based on secret bit with given index
#define CMD_GADGET_CF  _IOR(FETCHPROBE_MODULE_IOCTL_MAGIC_NUMBER, 0, uint64_t)

// branch based on secret bit with given index
#define CMD_GADGET_OFF _IOR(FETCHPROBE_MODULE_IOCTL_MAGIC_NUMBER, 1, uint64_t)

// get information about kernel stuff
#define CMD_INFO       _IOR(FETCHPROBE_MODULE_IOCTL_MAGIC_NUMBER, 2, struct fetchprobe_kernel_info*)

// reset secret buffer
#define CMD_RESET      _IOR(FETCHPROBE_MODULE_IOCTL_MAGIC_NUMBER, 3, uint64_t)

#endif /* _FETCHPROBE_MODULE_H */
