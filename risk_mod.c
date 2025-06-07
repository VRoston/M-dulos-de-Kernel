#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Seu Nome");
MODULE_DESCRIPTION("Módulo de avaliação de risco de processos");

static struct proc_dir_entry *proc_entry;

// Risk levels
#define RISK_LOW    0
#define RISK_MEDIUM 1
#define RISK_HIGH   2

static const char *risk_level_names[] = {
    "Low",
    "Medium", 
    "High"
};

struct process_metrics {
    unsigned long cpu_usage;    // CPU usage
    unsigned long memory_usage; // Memory usage in KB
    unsigned long io_usage;     // IO operations
    int priority;               // Process priority
    unsigned long runtime;      // Runtime in jiffies
};

static void get_process_metrics(struct task_struct *task, struct process_metrics *metrics) {
    unsigned long memory = 0;
    
    // Get CPU usage approximation
    metrics->cpu_usage = task->utime + task->stime;
    
    // Get memory usage
    if (task->mm) {
        memory = get_mm_rss(task->mm) << PAGE_SHIFT;
        metrics->memory_usage = memory / 1024; // Convert to KB
    } else {
        metrics->memory_usage = 0;
    }
    
    // Get IO usage approximation (simplified)
    metrics->io_usage = 0;
    if (task->ioac.syscr + task->ioac.syscw > 0) {
        metrics->io_usage = task->ioac.syscr + task->ioac.syscw;
    }
    
    // Get priority
    metrics->priority = task->prio;
    
    // Get runtime
    metrics->runtime = (jiffies - task->start_time) / HZ; // In seconds
}

static int calculate_risk_level(struct task_struct *task) {
    struct process_metrics metrics;
    int risk_score = 0;
    
    get_process_metrics(task, &metrics);
    
    // CPU usage assessment
    if (metrics.cpu_usage > 1000000) {
        risk_score += 30;
    } else if (metrics.cpu_usage > 100000) {
        risk_score += 15;
    }
    
    // Memory usage assessment
    if (metrics.memory_usage > 500000) { // 500MB
        risk_score += 25;
    } else if (metrics.memory_usage > 100000) { // 100MB
        risk_score += 10;
    }
    
    // Priority assessment (lower is higher priority)
    if (metrics.priority < 100) {
        risk_score += 15;
    }
    
    // Process state assessment
    if (task_is_running(task)) {
        risk_score += 10;
    }
    
    // Process flags assessment
    if (task->flags & PF_EXITING) {
        risk_score += 20;
    }
    
    // IO assessment
    if (metrics.io_usage > 1000) {
        risk_score += 20;
    }
    
    // Runtime assessment
    if (metrics.runtime > 3600) { // 1 hour
        risk_score += 10;
    }
    
    // Determine risk level
    if (risk_score >= 60) {
        return RISK_HIGH;
    } else if (risk_score >= 30) {
        return RISK_MEDIUM;
    } else {
        return RISK_LOW;
    }
}

static int proc_show(struct seq_file *m, void *v) {
    struct task_struct *task;
    
    seq_printf(m, "%-8s %-20s %-10s %-15s %-15s %-10s\n", 
              "PID", "COMM", "RISK", "MEM (KB)", "CPU", "RUNTIME(s)");
    seq_puts(m, "------------------------------------------------------------------\n");
    
    rcu_read_lock();
    for_each_process(task) {
        struct process_metrics metrics;
        int risk_level = calculate_risk_level(task);
        
        get_process_metrics(task, &metrics);
        
        seq_printf(m, "%-8d %-20s %-10s %-15lu %-15lu %-10lu\n",
                  task->pid,
                  task->comm,
                  risk_level_names[risk_level],
                  metrics.memory_usage,
                  metrics.cpu_usage,
                  metrics.runtime);
    }
    rcu_read_unlock();
    
    return 0;
}

static int proc_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_show, NULL);
}

static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init risk_init(void) {
    proc_entry = proc_create("kfetch_risk", 0444, NULL, &proc_fops);
    if (!proc_entry)
        return -ENOMEM;
    
    pr_info("Risk assessment module loaded\n");
    return 0;
}

static void __exit risk_exit(void) {
    proc_remove(proc_entry);
    pr_info("Risk assessment module unloaded\n");
}

module_init(risk_init);
module_exit(risk_exit);