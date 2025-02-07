#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "meltdown_module.h"

MODULE_AUTHOR("Redacted");
MODULE_DESCRIPTION("Meltdown PoC Kernel Module");
MODULE_LICENSE("GPL");

#define BUFFER_SIZE PAGE_SIZE

static uint8_t* kernel_buffer;
static uint8_t* kernel_buffer2; // used for reference load

static uint64_t seed = 42;

static uint64_t rand64(void){
    return (seed = (164603309694725029ull * seed) % 14738995463583502973ull);
}

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

void __gadget(uint64_t);

static long device_ioctl(struct file *file, unsigned int ioctl_num,
                         unsigned long ioctl_param) {
  size_t i;
  switch(ioctl_num) {
  case CMD_RESET: {
      // use seed from userspace so both can calculate the same secret
      seed = ioctl_param;
      
      // initialize buffer with random bytes (no nullbytes)
      for(i = 0; i < BUFFER_SIZE; i++) {
          kernel_buffer[i] = (uint8_t)rand64();
          while(!kernel_buffer[i]) {
              kernel_buffer[i] = (uint8_t)rand64();
          }
      }
      
      break;
  }
  
  case CMD_GADGET: {
      // access to kernel_buffer[GADGET_OFFSET]
      asm volatile(".global __gadget\n__gadget:\n mov (%0), %%rax" :: "r" (&kernel_buffer[GADGET_OFFSET]) : "rax");
      break;
  }
  
  case CMD_FLUSH: {
      // flush buffer from cache
      for(i = 0; i < BUFFER_SIZE; i += 64) {
          asm volatile("clflush (%0)" :: "r" (&kernel_buffer[i]));
      }
      break;
  }
  
  case CMD_CACHE: {
      // bring buffer into cache
      for(i = 0; i < 100000000; i++)
          asm volatile("mov (%0), %%rax\n":: "r" (&kernel_buffer[ioctl_param]) : "rax");
      break;
  }
  
  case CMD_INFO: {
      struct meltdown_kernel_info info;
      info.kernel_buffer = (uintptr_t)kernel_buffer;
      info.kernel_access = (uintptr_t)__gadget;
      info.kernel_buffer_size = BUFFER_SIZE;
      copy_to_user((void*)ioctl_param, &info, sizeof(info));
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
    .name = MELTDOWN_MODULE_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};


// prefetcher disabling / enabling for testing (making sure leakage is caused by exactly the prefetcher we think)
static void intel_disable_prefetch(void* blah) {
  (void) blah;
  asm volatile(
    "rdmsr\n"
    "or $15, %%eax\n"
    "mfence\n"
    "wrmsr\n"
    :: "c" (0x1a4) : "rax"
  );
}

static void intel_enable_prefetch(void* blah) {
  (void) blah;
  asm volatile(
    "rdmsr\n"
    "and $0xfffffff0, %%eax\n"
    "mfence\n"
    "wrmsr\n"
    :: "c" (0x1a4)
  );
}


int init_module(void) {
  int r;
  
  // on_each_cpu(intel_disable_prefetch, NULL, 1);
  
  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[meltdown-poc] Failed registering device with %d\n", r);
    return 1;
  }

  kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
  kernel_buffer2 = kmalloc(BUFFER_SIZE, GFP_KERNEL);
  
  printk(KERN_INFO "[meltdown-poc] buffer address: 0x%016llx\n", (uint64_t)kernel_buffer);
  printk(KERN_INFO "[meltdown-poc] gadget address: 0x%016llx\n", (uint64_t)__gadget);
  printk(KERN_INFO "[meltdown-poc] Loaded to %s.\n", MELTDOWN_MODULE_DEVICE_PATH);

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);
  
  kfree(kernel_buffer);
  kfree(kernel_buffer2);
  
  // on_each_cpu(intel_enable_prefetch, NULL, 1);
  printk(KERN_INFO "[meltdown-poc] Removed.\n");
}

