#ifndef _STRIDE_RE_MODULE_H
#define _STRIDE_RE_MODULE_H

#define BUFFER_SIZE (PAGE_SIZE * 4)
#define VICTIM_BUFFER_SIZE BUFFER_SIZE

#define STRIDE_RE_MODULE_DEVICE_NAME "stride_re_module"
#define STRIDE_RE_MODULE_DEVICE_PATH "/dev/" STRIDE_RE_MODULE_DEVICE_NAME

#define STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER (long)0x158165


struct stride_re_kernel_info {
    uintptr_t kernel_buffer;
    uintptr_t kernel_access;
};

// execute the load gadget
#define CMD_GADGET _IOR(STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER,  1, uint64_t)

// flush the whole buffer from the cache
#define CMD_FLUSH  _IOR(STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER,  2, uint64_t)

// flush a single cache line from the cache
#define CMD_FLUSH_SINGLE  _IOR(STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER,  5, uint64_t)

// get address of load and buffer
#define CMD_INFO   _IOR(STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER,  3, void*)

// probe provided offset of buffer
#define CMD_PROBE  _IOR(STRIDE_RE_MODULE_IOCTL_MAGIC_NUMBER,  4, void*)


#endif /* _STRIDE_RE_MODULE_H */
