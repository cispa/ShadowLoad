#ifndef _SHADOWLOAD_MODULE_H
#define _SHADOWLOAD_MODULE_H

#define SHADOWLOAD_MODULE_DEVICE_NAME "shadowload_module"
#define SHADOWLOAD_MODULE_DEVICE_PATH "/dev/" SHADOWLOAD_MODULE_DEVICE_NAME

#define SHADOWLOAD_MODULE_IOCTL_MAGIC_NUMBER (long)0x158165

struct shadowload_kernel_info {
    uintptr_t kernel_buffer;
    uintptr_t kernel_access;
};

// execute the load gadget
#define CMD_GADGET _IOR(SHADOWLOAD_MODULE_IOCTL_MAGIC_NUMBER,  1, uint64_t)

// flush the whole buffer from the cache
#define CMD_FLUSH  _IOR(SHADOWLOAD_MODULE_IOCTL_MAGIC_NUMBER,  2, uint64_t)

// get address of load and buffer
#define CMD_INFO   _IOR(SHADOWLOAD_MODULE_IOCTL_MAGIC_NUMBER,  3, void*)

// probe provided offset of buffer
#define CMD_PROBE  _IOR(SHADOWLOAD_MODULE_IOCTL_MAGIC_NUMBER,  4, void*)


#endif /* _SHADOWLOAD_MODULE_H */
