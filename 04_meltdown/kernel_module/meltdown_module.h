#ifndef _MELTDWON_MODULE_H
#define _MELTDWON_MODULE_H

#define MELTDOWN_MODULE_DEVICE_NAME "meltdown_module"
#define MELTDOWN_MODULE_DEVICE_PATH "/dev/" MELTDOWN_MODULE_DEVICE_NAME

#define MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER (long)0x635823

#define GADGET_OFFSET 2048

struct meltdown_kernel_info {
    uintptr_t kernel_buffer;
    uintptr_t kernel_access;
    uint64_t kernel_buffer_size;
};

// reset the PoC
#define CMD_RESET  _IOR(MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER,  1, uint64_t)

// execute the load gadget
#define CMD_GADGET _IOR(MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER,  2, uint64_t)

// flush the whole buffer from the cache
#define CMD_FLUSH  _IOR(MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER,  3, uint64_t)

// bring entry of buffer into cache
#define CMD_CACHE  _IOR(MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER,  4, uint64_t)

// get address of load and buffer
#define CMD_INFO   _IOR(MELTDOWN_MODULE_IOCTL_MAGIC_NUMBER,  5, void*)


#endif /* _UARCH_MODULE_H */
