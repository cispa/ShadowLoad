#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "fetch_probe_module.h"

MODULE_AUTHOR("Redacted");
MODULE_DESCRIPTION("FetchProbe Kernel Module (AMD)");
MODULE_LICENSE("GPL");

uint8_t* secret;
uint8_t* kernel_buffer;

// see https://github.com/IAIK/ZombieLoad/blob/53114eac8e36f3d3a4f69ca6ee40a17e07299ef3/attacker/variant3_windows/cacheutils_win.h#L172
#define speculation_start(label) asm goto ("call %l0" : : : "memory" : label##_retp); 
#define speculation_end(label) asm goto("jmp %l0" : : : "memory" : label); label##_retp: asm goto("lea %l0(%%rip), %%rax\nmovq %%rax, (%%rsp)\nret\n" : : : "memory","rax" : label); label: asm volatile("nop");


static int device_open(struct inode *inode, struct file *file) {
  /* Lock module */
  try_module_get(THIS_MODULE);
  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  /* Unlock module */
  module_put(THIS_MODULE);
  return 0;
}

// gadget used as spectre gadget
void __gadget(uint64_t);

static long device_ioctl(struct file *file, unsigned int ioctl_num,
                         unsigned long ioctl_param) {
  u64 i;
  
  switch(ioctl_num) {
  
  case CMD_GADGET: {
      i = (uintptr_t)kernel_buffer + secret[ioctl_param] * 4;
      asm volatile("mfence");
      if(secret[ioctl_param / 8] | secret[ioctl_param / 4] | secret[ioctl_param / 2]) {
          speculation_start(abc)
              asm volatile(".global __gadget\n__gadget:\n mov (%0), %%al\n" :: "r" (i) : "rax");
          speculation_end(abc)
      }
      
      break;
  }
  
  case CMD_INFO: {
      // provides information about required addresses to userspace
      struct prefetchjection_kernel_info info;
      info.kernel_buffer = (uintptr_t)kernel_buffer;
      info.kernel_access_off = (uintptr_t)__gadget;
      copy_to_user((void*)ioctl_param, &info, sizeof(info));
      break;
  }
  
  case CMD_RESET: {
      // fill secret page with pseudo-random data.
      // We use a seed provided by the userspace attacker such that the userspace attacker can calculate the same random bytes to judge how much data was leaked correctly.
      seed = ioctl_param;
      for(i = 0; i < BUFFER_SIZE; i++) {
          secret[i] = (uint8_t)rand64();
      }
      break;
  }
  
  default:
    return -1;
  }

  return 0;
}


static struct file_operations f_ops = {.unlocked_ioctl = device_ioctl,
                                       .open = device_open,
                                       .release = device_release};

static struct miscdevice misc_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = FETCH_PROBE_MODULE_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

int init_module(void) {
  int r;
  
  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[fetch-probe-amd] Failed registering device with %d\n", r);
    return 1;
  }

  kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
  secret = kmalloc(BUFFER_SIZE, GFP_KERNEL);
  
  printk(KERN_INFO "[fetch-probe-amd] buffer address: 0x%016llx\n", (uint64_t)kernel_buffer);
  printk(KERN_INFO "[fetch-probe-amd] spectre gadget address: 0x%016llx\n", (uint64_t)__gadget);

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);
  
  kfree(kernel_buffer);
  kfree(secret);
  
  printk(KERN_INFO "[fetch-probe-amd] Removed.\n");
}

