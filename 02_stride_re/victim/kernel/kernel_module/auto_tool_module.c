#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#include "auto_tool_module.h"

MODULE_AUTHOR("Redacted");
MODULE_DESCRIPTION("StrideRE Kernel Module");
MODULE_LICENSE("GPL");

static uint8_t* kernel_buffer;

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

#if defined(__x86_64__)
#define flush(x) asm volatile("clflush (%0)" :: "r" (x))
#define maccess(x) asm volatile("mov (%0), %%rax" :: "r" (x) : "rax")
#define mfence() asm volatile("mfence")
#ifdef APERF 
    static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
      uint64_t a, d;
      asm volatile("mfence");
      asm volatile("rdpru" : "=a" (a), "=d" (d) : "c" (1));
      a = (d << 32) | a;
      asm volatile("mfence");
      return a;
    }
#else // use rdtsc
static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
    uint64_t a, d;
    mfence();
    asm volatile("rdtsc" : "=a"(a), "=d"(d));
    a = (d << 32) | a;
    mfence();
    return a;
}
#endif /* APERF */
#elif defined (__aarch64__)
#define flush(x) asm volatile("DC CIVAC, %0" :: "r" (x))
#define maccess(x) asm volatile("ldr x0, [%0]" :: "r" (x) : "x0")
#define mfence() asm volatile("DSB SY\nISB")
static inline __attribute__((always_inline)) uint64_t _rdtsc(void) {
    uint64_t a;
    mfence();
    asm volatile("mrs %0, PMCCNTR_EL0" : "=r" (a));
    mfence();
    return a;
}
#endif /* ARCHITECTURE */

// measure time of memory load
static inline __attribute__((always_inline)) uint64_t probe(void* addr){
    uint64_t start, end;
    start = _rdtsc();
    maccess(addr);
    end = _rdtsc();
    return end - start;
}

void __gadget(uint64_t);

static long device_ioctl(struct file *file, unsigned int ioctl_num,
                         unsigned long ioctl_param) {
  size_t i;
  
  switch(ioctl_num) {
  
  
  case CMD_GADGET: {
      #ifdef __x86_64__
      asm volatile(".global __gadget\n__gadget:\n mov (%0), %%rax" :: "r" (&kernel_buffer[ioctl_param]) : "rax");
      #else // aarch64
      asm volatile(".global __gadget\n__gadget:\n ldr x0, [%0]" :: "r" (&kernel_buffer[ioctl_param]) : "x0");
      #endif /* ARCHITECTURE */
      break;
  }
  
  case CMD_FLUSH_SINGLE: {
      flush(&kernel_buffer[ioctl_param]);
      break;
  }
  
  case CMD_FLUSH: {
      // flush buffer from cache
      for(i = 0; i < BUFFER_SIZE; i += 64) {
          flush(&kernel_buffer[i]);
      }
      break;
  }
  
  case CMD_INFO: {
      struct stride_re_kernel_info info;
      info.kernel_buffer = (uintptr_t)kernel_buffer;
      info.kernel_access = (uintptr_t)__gadget;
      copy_to_user((void*)ioctl_param, &info, sizeof(info));
      break;
  }
  
  case CMD_PROBE: {
      copy_from_user(&i, (void*)ioctl_param, sizeof(i));
      i = probe(&kernel_buffer[i]);
      copy_to_user((void*)ioctl_param, &i, sizeof(i));
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
    .name = STRIDE_RE_MODULE_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

#ifdef __aarch64__

// https://github.com/jerinjacobk/armv8_pmu_cycle_counter_el0/blob/master/armv8_pmu_el0_cycle_counter.c
static void
enable_cycle_counter_el0(void* data)
{
	u64 val;
	/* Disable cycle counter overflow interrupt */
	asm volatile("msr pmintenclr_el1, %0" : : "r" ((u64)(1 << 31)));
	/* Enable cycle counter */
	asm volatile("msr pmcntenset_el0, %0" :: "r" BIT(31));
	/* Enable user-mode access to cycle counters. */
	asm volatile("msr pmuserenr_el0, %0" : : "r"(BIT(0) | BIT(2)));
	/* Clear cycle counter and start */
	asm volatile("mrs %0, pmcr_el0" : "=r" (val));
	val |= (BIT(0) | BIT(2));
	isb();
	asm volatile("msr pmcr_el0, %0" : : "r" (val));
	val = BIT(27);
	asm volatile("msr pmccfiltr_el0, %0" : : "r" (val));
}

static void
disable_cycle_counter_el0(void* data)
{
	/* Disable cycle counter */
	asm volatile("msr pmcntenset_el0, %0" :: "r" (0 << 31));
	/* Disable user-mode access to counters. */
	asm volatile("msr pmuserenr_el0, %0" : : "r"((u64)0));

}

#endif /* __aarch64__ */

int init_module(void) {
  int r;
  
  #ifdef __aarch64__
  on_each_cpu(enable_cycle_counter_el0, NULL, 1);
  #endif /* __aarch64__ */
  
  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[stride-re] Failed registering device with %d\n", r);
    return 1;
  }

  kernel_buffer = kmalloc(BUFFER_SIZE, GFP_KERNEL);
  
  printk(KERN_INFO "[stride-re] buffer address: 0x%016llx\n", (uint64_t)kernel_buffer);
  printk(KERN_INFO "[stride-re] gadget address: 0x%016llx\n", (uint64_t)__gadget);

  return 0;
}

void cleanup_module(void) {
  
  #ifdef __aarch64__
  on_each_cpu(disable_cycle_counter_el0, NULL, 1);
  #endif /* __aarch64__ */

  misc_deregister(&misc_dev);
  
  kfree(kernel_buffer);
  
  printk(KERN_INFO "[stride-re] Removed.\n");
}

