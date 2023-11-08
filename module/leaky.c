#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include "leaky.h"

MODULE_AUTHOR("Ruiyi Zhang");
MODULE_DESCRIPTION("Device to call kernel functions directly from user space");
MODULE_LICENSE("GPL");

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)

static bool device_busy = false;

static void maccess(void *p) { asm volatile("movq (%0), %%rax\n" : : "c"(p) : "rax"); }

static void myflush(void *p) { asm volatile("clflush 0(%0)\n" : : "c"(p) : "rax"); }

static int device_open(struct inode *inode, struct file *file) {
#if 0
  /* Check if device is busy */
  if (device_busy == true) {
    return -EBUSY;
  }
#endif
  /* Lock module */
  try_module_get(THIS_MODULE);

  device_busy = true;

  return 0;
}

static int device_release(struct inode *inode, struct file *file) {
  /* Unlock module */
  device_busy = false;

  module_put(THIS_MODULE);

  return 0;
}

volatile static char __attribute__((aligned(4096))) test_buffer[4096*64];

static size_t rdpru_a(void) {
    size_t a, d;
    asm volatile("mfence");
    asm volatile(".byte 0x0f,0x01,0xfd" : "=a"(a), "=d"(d) : "c"(1) : );
    a = (d << 32) | a;
    asm volatile("mfence");
    return a;
}

static size_t bench(void) {
    size_t start, end;
    start = rdpru_a(); 
    myflush(&start);
    asm volatile("mfence");
    asm volatile("invd");
    asm volatile("mfence");
    end = rdpru_a(); 
    return end-start;
}

static long device_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) {
  switch (ioctl_num) {
    case LEAKY_IOCTL_CMD_INVD:
    {
        asm volatile("invd");
        return 0;
    }
    case LEAKY_IOCTL_CMD_WBNOINVD:
    {
        asm volatile("wbnoinvd");
        return 0;
    }
    case LEAKY_IOCTL_CMD_WBINVD:
    {
        asm volatile("wbinvd");
        return 0;
    }
    case LEAKY_IOCTL_CMD_CACHE_TEST:
    {
        printk(KERN_ALERT "[leaky-module] Testing\n");
        size_t target = 1;
    	//local_irq_disable();
        asm volatile("movq $2, (%0)\n" :: "r"(&target) : "memory");
        asm volatile("wbinvd\n");

        asm volatile("movq $3, (%0)\n"
            "mfence\n"
            "invd\n"
            "mov $1000000, %%rcx\n"
            "2:\n"
            "loop 2b\n"
            :: "r"(&target) : "rcx","memory");
        if(target == 2) 
	    printk(KERN_ALERT "[leaky-module] FAULT! %zd\n", target);
	    //local_irq_enable();

    }
    case LEAKY_IOCTL_CMD_INVD_TIMING:
    {
        printk(KERN_ALERT "[leaky-module] INVD Timing\n");
        
        uint64_t avg_measure[512] = {0};
        asm volatile("cli\n");
        asm volatile("wbinvd\n");
        for (size_t r = 0; r < 100; r++)
        {
            for (size_t i = 0; i < 512; i++)
            {
                asm volatile("mfence\n");
                asm volatile("wbinvd\n");
                asm volatile("mfence\n");
                for (size_t j = 0; j < i*5; j+=5)
                {
                    test_buffer[j*64] ^= 0xce;
                    test_buffer[(j+1)*64] ^= 0xce;
                    test_buffer[(j+2)*64] ^= 0xce;
                    test_buffer[(j+3)*64] ^= 0xce;
                    test_buffer[(j+4)*64] ^= 0xce;
                }
                asm volatile("mfence\n");    
                // Time invd instruction
                avg_measure[i] += bench();
                asm volatile("mfence\n");
                asm volatile("wbinvd\n");
                asm volatile("mfence\n");
            }
        }
        asm volatile("sti\n");

        for (size_t i = 0; i < 512; i++ )
        {
            printk("%ld,%ld\n", i, avg_measure[i]/100);
        }

        return 0;
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
    .name = LEAKY_DEVICE_NAME,
    .fops = &f_ops,
    .mode = S_IRWXUGO,
};

int init_module(void) {
  int r;

  /* Register device */
  r = misc_register(&misc_dev);
  if (r != 0) {
    printk(KERN_ALERT "[leaky-module] Failed registering device with %d\n", r);
    return 1;
  }

  printk(KERN_INFO "[leaky-module] Loaded.\n");

  return 0;
}

void cleanup_module(void) {
  misc_deregister(&misc_dev);

  printk(KERN_INFO "[leaky-module] Removed.\n");
}
