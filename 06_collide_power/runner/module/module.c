#include "interface.h"

#include <asm/io.h>
#include <asm/msr-index.h>
#include <asm/mwait.h>
#include <asm/smap.h>
#include <asm/special_insns.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/irqnr.h>
#include <linux/irqreturn.h>
#include <linux/kallsyms.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ktime.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/wait.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Andreas Kogler");
MODULE_DESCRIPTION("");
MODULE_VERSION("");

#if !defined(IS_AMD)
#pragma message "### Building INTEL"

#define ENERGY_UNIT MSR_RAPL_POWER_UNIT
#define ENERGY_PKG  MSR_PKG_ENERGY_STATUS
#define ENERGY_PP0  MSR_PP0_ENERGY_STATUS
#define ENERGY_DRAM MSR_DRAM_ENERGY_STATUS

#define PLACE_INTEL(_x) _x

#else
#pragma message "### Building AMD"

#define ENERGY_UNIT MSR_AMD_RAPL_POWER_UNIT
#define ENERGY_PKG  MSR_AMD_PKG_ENERGY_STATUS
#define ENERGY_PP0  MSR_AMD_CORE_ENERGY_STATUS

#define PLACE_INTEL(_x) ""

#endif

#define ASM_RDMSR(_nr, _dest)                                                \
    "    movabsq $" str(_nr) ", %%rcx\n"                                     \
                             "    rdmsr\n"                                   \
                             "    mov %%eax, %[" str(_dest) "]           \n" \
                                                            "    mov %%edx, -4+%H[" str(_dest) "]       \n"

#define ASM_RDMSR_LOW(_nr, _dest)                                        \
    "    movabsq $" str(_nr) ", %%rcx           \n"                      \
                             "    rdmsr                              \n" \
                             "    mov %%eax, %[" str(_dest) "]           \n"

#define ASM_RDTSCP(_dest)            \
    "rdtscp\n"                       \
    "mov %%eax, %[" str(_dest) "]\n" \
                               "mov %%edx, -4+%H[" str(_dest) "]\n"

/********************************************************************************
 * logging
 ********************************************************************************/
#define VA_ARGS(...) , ##__VA_ARGS__

#define xstr(s) str(s)
#define str(s)  #s

#define TAG xstr(NAME)

#define INFO(_x, ...)  printk(KERN_INFO "[" TAG "] " _x "\n" VA_ARGS(__VA_ARGS__))
#define ERROR(_x, ...) printk(KERN_ERR "[" TAG "] " _x "\n" VA_ARGS(__VA_ARGS__))

/********************************************************************************
 * device functions
 ********************************************************************************/
static long module_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
static int  module_open(struct inode *, struct file *);
static int  module_release(struct inode *, struct file *);

typedef unsigned long (*kallsyms_lookup_name_t)(const char *);

static struct file_operations file_ops = { .open = module_open, .release = module_release, .unlocked_ioctl = module_ioctl /*, .mmap = module_mmap*/ };

static struct miscdevice dev = { .minor = MISC_DYNAMIC_MINOR, .name = TAG, .fops = &file_ops, .mode = S_IRUGO | S_IWUGO };

static uint32_t device_open_count = 0;

/********************************************************************************
 * access macros
 ********************************************************************************/

#define WRMSR(_nr, _val) wrmsrl(_nr, _val)
#define RDMSR(_nr)       __rdmsr(_nr)

/********************************************************************************
 * IOCTL
 ********************************************************************************/

long measure_begin(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct measurement_t *begin = (struct measurement_t *)arg;

    // sync
    // uint64_t energy_start = __rdmsr(ENERGY_PP0);
    // while ( energy_start == __rdmsr(ENERGY_PP0) )
    //    ;

    asm volatile(

        /* read c-state residencies*/
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C7_RESIDENCY, c7)) //
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C6_RESIDENCY, c6)) //
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C3_RESIDENCY, c3)) //
        //  ASM_RDMSR(MSR_CORE_C1_RESIDENCY, c1) //

        /* mperf aperf end */
        ASM_RDMSR(MSR_IA32_MPERF, mp) //
        ASM_RDMSR(MSR_IA32_APERF, ap) //

        /* read tsc */
        ASM_RDTSCP(tsc) //

        /* read pkg and pp0 */
        PLACE_INTEL(ASM_RDMSR_LOW(ENERGY_DRAM, dram)) //
        ASM_RDMSR_LOW(ENERGY_PKG, pkg)                //
        ASM_RDMSR_LOW(ENERGY_PP0, pp0)                //

        : [pkg] "=m"(begin->energy_pkg),   //
          [dram] "=m"(begin->energy_dram), //
          [pp0] "=m"(begin->energy_pp0),   //
          [tsc] "=m"(begin->cycles),       //
          [ap] "=m"(begin->aperf),         //
          [mp] "=m"(begin->mperf)          //,         //
                                           //[c1] "=m"(begin->c1_residency),  //
                                           //[c3] "=m"(begin->c3_residency),  //
                                           //[c6] "=m"(begin->c6_residency),  //
                                           //[c7] "=m"(begin->c7_residency)   //
        :
        : "rax", "rcx", "rdx");

    return 0;
}

long measure_end(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct measurement_t *begin = (struct measurement_t *)arg;

    uint64_t dro = 0;
    uint64_t tcc = 0;
    uint64_t perf_status;
    uint64_t therm_status;
    uint64_t therm_target;

    struct measurement_t end;

    asm volatile(

        /* read tsc */
        ASM_RDTSCP(tsc) //

        /* read pkg and pp0 */
        ASM_RDMSR_LOW(ENERGY_PP0, pp0)                //
        ASM_RDMSR_LOW(ENERGY_PKG, pkg)                //
        PLACE_INTEL(ASM_RDMSR_LOW(ENERGY_DRAM, dram)) //

        /* read pstate and voltage */
        PLACE_INTEL(ASM_RDMSR(MSR_IA32_PERF_STATUS, ps)) //

        /* mperf aperf end */
        ASM_RDMSR(MSR_IA32_APERF, ap) //
        ASM_RDMSR(MSR_IA32_MPERF, mp) //

        /* read thermal status */
        PLACE_INTEL(ASM_RDMSR_LOW(MSR_IA32_THERM_STATUS, ts)) //

        /* read temperature target */
        PLACE_INTEL(ASM_RDMSR_LOW(MSR_IA32_TEMPERATURE_TARGET, tt)) //

        /* read c-state residencies*/
        // ASM_RDMSR(MSR_CORE_C1_RESIDENCY, c1) //
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C3_RESIDENCY, c3)) //
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C6_RESIDENCY, c6)) //
        // PLACE_INTEL(ASM_RDMSR(MSR_CORE_C7_RESIDENCY, c7)) //

        : [pkg] "=m"(end.energy_pkg),   //
          [dram] "=m"(end.energy_dram), //
          [pp0] "=m"(end.energy_pp0),   //
          [tsc] "=m"(end.cycles),       //
          [ps] "=m"(perf_status),       //
          [ap] "=m"(end.aperf),         //
          [mp] "=m"(end.mperf),         //
          [tt] "=m"(therm_target),      //
          [ts] "=m"(therm_status)       //,      //
                                        //[c1] "=m"(end.c1_residency),  //
                                        //[c3] "=m"(end.c3_residency),  //
                                        //[c6] "=m"(end.c6_residency),  //
                                        //[c7] "=m"(end.c7_residency)   //
        :
        : "rax", "rcx", "rdx");

    dro = (therm_status >> 16) & 0x7F;
    tcc = (therm_target >> 16) & 0xFF;

    begin->energy_pkg  = end.energy_pkg - begin->energy_pkg;
    begin->energy_pp0  = end.energy_pp0 - begin->energy_pp0;
    begin->energy_dram = end.energy_dram - begin->energy_dram;

    begin->cycles = end.cycles - begin->cycles;

    begin->voltage = ((perf_status & 0xFFFF00000000llu) >> 32);
    begin->pstate  = ((perf_status & 0xFFFFllu) >> 8);

    begin->temperature = tcc - dro;

    begin->aperf = end.aperf - begin->aperf;
    begin->mperf = end.mperf - begin->mperf;

    // begin->c1_residency = 0; // end.c1_residency - begin->c1_residency;
    // begin->c3_residency = end.c3_residency - begin->c3_residency;
    // begin->c6_residency = end.c6_residency - begin->c6_residency;
    // begin->c7_residency = end.c7_residency - begin->c7_residency;

#if defined(IS_AMD)
    begin->voltage     = 0;
    begin->pstate      = 0;
    begin->energy_dram = 0;
    begin->temperature = 0;

    // begin->c1_residency = 0;
    // begin->c3_residency = 0;
    // begin->c6_residency = 0;
    // begin->c7_residency = 0;
#endif

    return 0;
}

long measure_us(struct file *filep, unsigned int cmd, unsigned long arg) {
    struct measurement_t *data = (struct measurement_t *)arg;

    uint64_t delay = data->cycles;

    measure_begin(filep, cmd, arg);
    udelay(delay);
    measure_end(filep, cmd, arg);
    return 0;
}

#define TIMEOUT_NSEC (1000000L) // 1 ms
#define TIMEOUT_SEC  (0)        //

typedef long (*module_ioc_t)(struct file *filep, unsigned int cmd, unsigned long arg);

//                                                         %rdi, %rsi, %rdx, %r10, %r8 and %r9.
static long module_ioctl(struct file *filep, unsigned int cmd, unsigned long arg) {

    char         data[256];
    module_ioc_t handler = NULL;
    long         ret;

    if ( _IOC_SIZE(cmd) > 256 ) {
        return -EFAULT;
    }

    switch ( cmd ) {

        case MEASURE_US:
            handler = measure_us;
            break;

        default:
            return -ENOIOCTLCMD;
    }

    if ( copy_from_user(data, (void __user *)arg, _IOC_SIZE(cmd)) )
        return -EFAULT;

    ret = handler(filep, cmd, (unsigned long)((void *)data));

    if ( !ret && (cmd & IOC_OUT) ) {
        if ( copy_to_user((void __user *)arg, data, _IOC_SIZE(cmd)) )
            return -EFAULT;
    }

    return ret;
}

/********************************************************************************
 * OPEN RELEASE
 ********************************************************************************/
static int module_open(struct inode *inode, struct file *file) {
    if ( device_open_count ) {
        return -EBUSY;
    }
    INFO("opened!");

    device_open_count++;
    try_module_get(THIS_MODULE);

    return 0;
}

static int module_release(struct inode *inode, struct file *file) {
    device_open_count--;
    module_put(THIS_MODULE);
    INFO("released!");

    return 0;
}

/********************************************************************************
 * INIT EXIT
 ********************************************************************************/
int module_init_function(void) {

    INFO("Module loaded");
    if ( misc_register(&dev) ) {
        ERROR("could not register device!");
        dev.this_device = NULL;
        return -EINVAL;
    }

    return 0;
}

void module_exit_function(void) {
    if ( dev.this_device ) {
        misc_deregister(&dev);
    }
    INFO("Module unloaded");
}

module_init(module_init_function);
module_exit(module_exit_function);
