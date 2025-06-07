#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h> // Para task->signal, rlim
#include <linux/sched/task.h>   // Para get_task_struct, put_task_struct (task_is_running está em linux/sched.h)
#include <linux/cred.h>         // Para task_euid, current_user_ns(), kuid_t
#include <linux/uidgid.h>       // Para KUIDT_INIT, from_kuid_munged
#include <linux/kernel.h>       // Para KERN_INFO, RLIM_INFINITY, pr_info
#include <linux/rcupdate.h>     // Para rcu_read_lock/unlock
#include <linux/user_namespace.h> // Para init_user_ns (contexto para KUIDT_INIT)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EduardoSilvaS via Copilot");
MODULE_DESCRIPTION("Módulo de avaliação de risco de processos aprimorado");

static struct proc_dir_entry *proc_entry;

// Define os níveis de risco
enum risk_level {
    RISK_LOW,
    RISK_MEDIUM,
    RISK_HIGH
};

// Strings correspondentes aos níveis de risco
static const char *risk_level_str[] = {
    "Baixo", "Médio", "Alto"
};

// Função para calcular o score numérico de risco
static int calculate_numerical_risk(struct task_struct *task) {
    int risk_score = 0;

    // Tratar threads do kernel
    if (task->flags & PF_KTHREAD) {
        // Verifica se a thread do kernel está na runqueue (ativa)
        if (task_is_running(task)) { // Removido task_curr(task)
             risk_score += 1;
        }
        if (task->flags & PF_EXITING) {
            risk_score += 2;
        }
        return risk_score;
    }

    // Para processos de usuário:
    if (task->signal) {
        if (task->signal->rlim[RLIMIT_CPU].rlim_cur != RLIM_INFINITY &&
            task->signal->rlim[RLIMIT_CPU].rlim_cur < 10) {
            risk_score += 1;
        }
    }

    // Verifica se o processo está na runqueue (ativamente usando CPU ou pronto para)
    if (task_is_running(task)) { // Removido task_curr(task)
         risk_score += 1;
    }

    if (task->ioac.read_bytes > (100UL * 1024 * 1024)) {
        risk_score += 1;
    }
    if (task->ioac.write_bytes > (100UL * 1024 * 1024)) {
        risk_score += 1;
    }

    if (uid_eq(task_euid(task), KUIDT_INIT(0))) {
        risk_score += 2;
    }

    if (task->flags & PF_EXITING) {
        risk_score += 2;
    }
    
    return risk_score;
}

// Função para converter score numérico em categoria de risco
static enum risk_level get_risk_category(int numerical_risk) {
    if (numerical_risk <= 1) {
        return RISK_LOW;
    } else if (numerical_risk <= 3) {
        return RISK_MEDIUM;
    } else {
        return RISK_HIGH;
    }
}

// Função chamada para mostrar o conteúdo do arquivo no procfs
static int proc_show(struct seq_file *m, void *v) {
    struct task_struct *task;
    int numerical_risk;
    enum risk_level category;
    unsigned long rlim_cpu_cur_val;
    unsigned long read_mb;
    unsigned long write_mb;
    kuid_t task_euid_val;
    int display_uid;

    seq_printf(m, "%-8s %-20s %-5s %-10s %-10s %-10s %-8s (Score)\n",
               "PID", "COMM", "UID", "CPU_RLIM", "IO_R_MB", "IO_W_MB", "RISK");
    seq_puts(m, "-----------------------------------------------------------------------------------\n");

    rcu_read_lock();
    for_each_process(task) {
        get_task_struct(task);

        if (task->flags & PF_KTHREAD) {
            rlim_cpu_cur_val = (unsigned long)-2;
            display_uid = -1;
        } else {
            if (task->signal) {
                rlim_cpu_cur_val = task->signal->rlim[RLIMIT_CPU].rlim_cur;
                if (rlim_cpu_cur_val == RLIM_INFINITY) {
                    rlim_cpu_cur_val = (unsigned long)-1;
                }
            } else {
                rlim_cpu_cur_val = (unsigned long)-2;
            }
            task_euid_val = task_euid(task);
            display_uid = from_kuid_munged(current_user_ns(), task_euid_val);
        }

        read_mb = task->ioac.read_bytes / (1024 * 1024);
        write_mb = task->ioac.write_bytes / (1024 * 1024);

        numerical_risk = calculate_numerical_risk(task);
        category = get_risk_category(numerical_risk);

        seq_printf(m, "%-8d %-20s %-5d %-10lu %-10lu %-10lu %-8s (%d)\n",
                   task->pid,
                   task->comm,
                   display_uid,
                   rlim_cpu_cur_val,
                   read_mb,
                   write_mb,
                   risk_level_str[category],
                   numerical_risk);
        
        put_task_struct(task);
    }
    rcu_read_unlock();

    seq_puts(m, "\nLegenda CPU_RLIM: Segundos. (-1 = Infinito, -2 = N/A para kthreads ou s/sinal)\n");
    seq_puts(m, "Notas:\n");
    seq_puts(m, "- Este módulo fornece uma análise de risco sob demanda.\n");
    seq_puts(m, "- Métricas de E/S (IO_R_MB, IO_W_MB) são cumulativas.\n");
    seq_puts(m, "- Rastreamento detalhado de chamadas de sistema e tráfego de rede por processo não está incluído.\n");

    return 0;
}

// Função chamada quando o arquivo no procfs é aberto
static int proc_open(struct inode *inode, struct file *file) {
    return single_open(file, proc_show, NULL);
}

// Define as operações do arquivo no procfs
static const struct proc_ops proc_fops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Função de inicialização do módulo
static int __init risk_init(void) {
    proc_entry = proc_create("kfetch_risk", 0, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Falha ao criar a entrada /proc/kfetch_risk\n");
        return -ENOMEM;
    }
    pr_info("Módulo de risco aprimorado carregado: /proc/kfetch_risk\n");
    return 0;
}

// Função de finalização do módulo
static void __exit risk_exit(void) {
    if (proc_entry) {
        proc_remove(proc_entry);
    }
    pr_info("Módulo de risco aprimorado descarregado\n");
}

module_init(risk_init);
module_exit(risk_exit);
