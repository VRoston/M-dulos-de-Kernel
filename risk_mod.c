#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h> // Para task->signal, rlim
#include <linux/sched/task.h>   // Para get_task_struct, put_task_struct, task_is_running, task_curr
#include <linux/cred.h>         // Para task_euid, current_user_ns(), kuid_t
#include <linux/uidgid.h>       // Para KUIDT_INIT, from_kuid_munged
#include <linux/kernel.h>       // Para KERN_INFO, RLIM_INFINITY, pr_info
#include <linux/rcupdate.h>     // Para rcu_read_lock/unlock
#include <linux/user_namespace.h> // Para init_user_ns (contexto para KUIDT_INIT)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("EduardoSilvaS via Copilot");
MODULE_DESCRIPTION("Módulo de avaliação de risco de processos aprimorado");

static struct proc_dir_entry *proc_entry_enhanced;

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
        // Threads do kernel são geralmente confiáveis, mas podem ser parte de um problema.
        // Atribuir um score base baixo.
        if (task_is_running(task) || task_curr(task)) { // Verifica se está na runqueue ou executando
             risk_score += 1; // Thread do kernel ativa
        }
        if (task->flags & PF_EXITING) { // Se a thread do kernel estiver saindo
            risk_score += 2;
        }
        return risk_score; // Max para kthread aqui é 3 (ativo + saindo)
    }

    // Para processos de usuário:

    // Métrica 1: Uso/Configuração da CPU
    // task->signal pode ser NULL para alguns processos muito cedo ou se não tiverem manipuladores de sinal.
    if (task->signal) {
        // RLIMIT_CPU é o limite de tempo de CPU em segundos.
        // Um limite muito baixo pode indicar um processo restrito ou mal configurado.
        // RLIM_INFINITY indica sem limite.
        if (task->signal->rlim[RLIMIT_CPU].rlim_cur != RLIM_INFINITY &&
            task->signal->rlim[RLIMIT_CPU].rlim_cur < 10) { // Ex: limite < 10 segundos
            risk_score += 1;
        }
    }

    // Verifica se o processo está ativamente usando CPU (na runqueue ou executando)
    if (task_is_running(task) || task_curr(task)) {
         risk_score += 1;
    }

    // Métrica 2: Atividade de E/S (cumulativa desde o início do processo)
    // task->ioac contém contadores de bytes de E/S.
    // Valores altos podem indicar atividade intensa. Ex: > 100MB.
    if (task->ioac.read_bytes > (100UL * 1024 * 1024)) {
        risk_score += 1;
    }
    if (task->ioac.write_bytes > (100UL * 1024 * 1024)) {
        risk_score += 1;
    }

    // Métrica 3: Privilégios
    // Processos rodando como root (EUID 0) têm maior potencial de impacto.
    // KUIDT_INIT(0) representa o UID root no namespace de usuário inicial.
    if (uid_eq(task_euid(task), KUIDT_INIT(0))) {
        risk_score += 2;
    }

    // Métrica 4: Estado do Processo
    // Processos que estão finalizando (PF_EXITING).
    if (task->flags & PF_EXITING) {
        risk_score += 2; // Peso maior para processos saindo
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
static int proc_show_enhanced(struct seq_file *m, void *v) {
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

    rcu_read_lock(); // Necessário para iterar sobre processos com segurança
    for_each_process(task) {
        // get_task_struct incrementa a contagem de uso do task_struct,
        // impedindo que seja liberado enquanto o usamos.
        get_task_struct(task);

        if (task->flags & PF_KTHREAD) { // Simplificação para threads do kernel
            rlim_cpu_cur_val = (unsigned long)-2; // Indicar N/A para kthreads
            display_uid = -1; // UID não é tão relevante da mesma forma
        } else {
            if (task->signal) {
                rlim_cpu_cur_val = task->signal->rlim[RLIMIT_CPU].rlim_cur;
                if (rlim_cpu_cur_val == RLIM_INFINITY) {
                    rlim_cpu_cur_val = (unsigned long)-1; // Representa infinito
                }
            } else {
                rlim_cpu_cur_val = (unsigned long)-2; // Indicar N/A se task->signal for NULL
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
        
        put_task_struct(task); // Libera a referência ao task_struct
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
static int proc_open_enhanced(struct inode *inode, struct file *file) {
    return single_open(file, proc_show_enhanced, NULL);
}

// Define as operações do arquivo no procfs
static const struct proc_ops proc_fops_enhanced = {
    .proc_open = proc_open_enhanced,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

// Função de inicialização do módulo
static int __init risk_init_enhanced(void) {
    proc_entry_enhanced = proc_create("kfetch_risk_enhanced", 0, NULL, &proc_fops_enhanced);
    if (!proc_entry_enhanced) {
        pr_err("Falha ao criar a entrada /proc/kfetch_risk_enhanced\n");
        return -ENOMEM;
    }
    pr_info("Módulo de risco aprimorado carregado: /proc/kfetch_risk_enhanced\n");
    return 0;
}

// Função de finalização do módulo
static void __exit risk_exit_enhanced(void) {
    if (proc_entry_enhanced) {
        proc_remove(proc_entry_enhanced);
    }
    pr_info("Módulo de risco aprimorado descarregado\n");
}

module_init(risk_init_enhanced);
module_exit(risk_exit_enhanced);
