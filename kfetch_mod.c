#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/utsname.h>
#include <linux/cpu.h>
#include <linux/sched/task.h>
#include <linux/jiffies.h>
#include <asm/processor.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <linux/sysinfo.h>
#include <linux/ktime.h>
#include <linux/sched/signal.h>
#include <linux/cpumask.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>

#define DEVICE_NAME "kfetch"
#define CLASS_NAME "kfetch"
#define KFETCH_NUM_INFO 6

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seu Nome");
MODULE_DESCRIPTION("Módulo para informações do sistema");

struct kfetch_data {
    int mask;
    struct mutex lock;
};

static int major_num;
static struct class *kfetch_class = NULL;
static struct device *kfetch_device = NULL;

static int kfetch_open(struct inode *inodep, struct file *filep) {
    struct kfetch_data *data = kmalloc(sizeof(struct kfetch_data), GFP_KERNEL);
    if (!data) return -ENOMEM;
    
    mutex_init(&data->lock);
    data->mask = (1 << KFETCH_NUM_INFO) - 1; // Máscara padrão
    filep->private_data = data;
    return 0;
}

static int kfetch_release(struct inode *inodep, struct file *filep) {
    struct kfetch_data *data = filep->private_data;
    mutex_destroy(&data->lock);
    kfree(data);
    return 0;
}

static ssize_t kfetch_write(struct file *filep, const char __user *buffer,
                          size_t len, loff_t *offset) {
    struct kfetch_data *data = filep->private_data;
    int mask;
    
    if (copy_from_user(&mask, buffer, sizeof(int)))
        return -EFAULT;
    mutex_lock(&data->lock);
    data->mask = mask;
    mutex_unlock(&data->lock);
    return sizeof(int);
}

static void get_system_info(struct kfetch_data *data, char *buf, size_t size) {
    struct new_utsname *uts = init_utsname();
    struct sysinfo mem_info;
    struct timespec64 uptime;
    int pos = 0;
    int logo_idx = 0;
    const char *logo[] = {
        "   ____ _____ ",
        "  / ___|___  |",
        " | |  _   / / ",
        " | |_| | / /  ",
        "  \\____|/_/   ",
        "              ",
        "              ",
        "              ",
        "              ",
        "              "
    };
    const int logo_lines = 6;  // Número real de linhas do logo
    
    si_meminfo(&mem_info);
    ktime_get_boottime_ts64(&uptime);

    // Primeira linha com nome do host
    pos += snprintf(buf + pos, size - pos, "%-32s %s\n", logo[logo_idx++], uts->nodename);
    pos += snprintf(buf + pos, size - pos, "%-32s ----------------------------------\n", logo[logo_idx++]);

    // Informação do kernel
    if (data->mask & (1 << 0)) {
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : logo[logo_lines - 1];
        pos += snprintf(buf + pos, size - pos, "%-32s Kernel: %s\n", line, uts->release);
    }

    // Informação do CPU modelo
    if (data->mask & (1 << 2)) {
        struct cpuinfo_x86 *c = &cpu_data(0);
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : "                   ";
        pos += snprintf(buf + pos, size - pos, "%-32s CPU: %s\n", line, c->x86_model_id);
    }

    // Informação do número de CPUs
    if (data->mask & (1 << 1)) {
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : "                   ";
        pos += snprintf(buf + pos, size - pos, "%-32s CPUs: %u/%u\n", line,
                      num_online_cpus(), num_possible_cpus());
    }

    // Informação da memória
    if (data->mask & (1 << 3)) {
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : "                   ";
        pos += snprintf(buf + pos, size - pos, "%-32s Mem: %luMB/%luMB\n", line,
                      (mem_info.freeram << (PAGE_SHIFT - 10)) / 1024,
                      (mem_info.totalram << (PAGE_SHIFT - 10)) / 1024);
    }

    // Informação de processos
    if (data->mask & (1 << 5)) {
        struct task_struct *task;
        int process_count = 0;
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : "                   ";
        
        rcu_read_lock();
        for_each_process(task) {
            process_count++;
        }
        rcu_read_unlock();
        
        pos += snprintf(buf + pos, size - pos, "%-32s Proc: %d\n", line, process_count);
    }

    // Informação de uptime
    if (data->mask & (1 << 4)) {
        unsigned long minutes = uptime.tv_sec / 60;
        const char *line = (logo_idx < logo_lines) ? logo[logo_idx++] : "                   ";
        pos += snprintf(buf + pos, size - pos, "%-32s Uptime: %lu min\n", line, minutes);
    }
}

static ssize_t kfetch_read(struct file *filep, char __user *buffer,
                          size_t len, loff_t *offset) {
    struct kfetch_data *data = filep->private_data;
    char *kfetch_buf;
    size_t pos;
    ssize_t ret = 0;

    if (*offset > 0) return 0;

    kfetch_buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!kfetch_buf) return -ENOMEM;

    mutex_lock(&data->lock);
    get_system_info(data, kfetch_buf, PAGE_SIZE);
    mutex_unlock(&data->lock);

    pos = strlen(kfetch_buf);
    
    if (copy_to_user(buffer, kfetch_buf, min(pos, len))) {
        ret = -EFAULT;
        goto out;
    }
    
    *offset = pos;
    ret = min(pos, len);

out:
    kfree(kfetch_buf);
    return ret;
}

static const struct file_operations kfetch_ops = {
    .owner = THIS_MODULE,
    .open = kfetch_open,
    .release = kfetch_release,
    .read = kfetch_read,
    .write = kfetch_write,
};

static int __init kfetch_init(void) {
    major_num = register_chrdev(0, DEVICE_NAME, &kfetch_ops);
    if (major_num < 0) {
        pr_err("Failed to register device\n");
        return major_num;
    }

    kfetch_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(kfetch_class)) {
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(kfetch_class);
    }

    kfetch_device = device_create(kfetch_class, NULL, MKDEV(major_num, 0),
                                 NULL, DEVICE_NAME);
    if (IS_ERR(kfetch_device)) {
        class_destroy(kfetch_class);
        unregister_chrdev(major_num, DEVICE_NAME);
        return PTR_ERR(kfetch_device);
    }

    pr_info("kfetch module loaded\n");
    return 0;
}

static void __exit kfetch_exit(void) {
    device_destroy(kfetch_class, MKDEV(major_num, 0));
    class_destroy(kfetch_class);
    unregister_chrdev(major_num, DEVICE_NAME);
    pr_info("kfetch module unloaded\n");
}

module_init(kfetch_init);
module_exit(kfetch_exit);
