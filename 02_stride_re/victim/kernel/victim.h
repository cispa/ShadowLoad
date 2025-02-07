#include <unistd.h>

#include "kernel_module/auto_tool_module.h"

int module_fd;

struct stride_re_kernel_info info;

int victim_init(void) {
    module_fd = open(STRIDE_RE_MODULE_DEVICE_PATH, O_RDONLY);
    ioctl(module_fd, CMD_INFO, &info);
}

uintptr_t victim_buffer_address(void) {
    return info.kernel_buffer;
}

uintptr_t victim_load_address(void) {
    return info.kernel_access;
}

void victim_flush_buffer(void) {
    ioctl(module_fd, CMD_FLUSH, 0);
}

void victim_flush_single(uint64_t offset){
    ioctl(module_fd, CMD_FLUSH_SINGLE, offset);
}

void victim_load_gadget(uint64_t offset) {
    ioctl(module_fd, CMD_GADGET, offset);
}

uint64_t victim_probe(uint64_t offset) {
    ioctl(module_fd, CMD_PROBE, &offset);
    return offset;
}

void victim_destroy(void) {
    close(module_fd);
}
