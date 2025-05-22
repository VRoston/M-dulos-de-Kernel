# Modulos-de-Kernel

Este projeto contém dois módulos de kernel Linux:
1. `kfetch_mod.c`: Fornece informações do sistema similar ao comando neofetch
2. `risk_mod.c`: Avalia o risco de processos em execução no sistema

## Requisitos

- Sistema operacional Linux
- Cabeçalhos de kernel instalados para sua versão do kernel
- Ferramentas de compilação (gcc, make)

## Preparação do ambiente

Instale as dependências necessárias:

```bash
# Para distribuições baseadas em Debian/Ubuntu
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)

# Para distribuições baseadas em RHEL/Fedora
sudo dnf install kernel-devel kernel-headers gcc make
```

## Compilação dos módulos

1. Clone este repositório ou extraia os arquivos em um diretório
2. Navegue até o diretório dos módulos
3. Execute o comando make para compilar:

```bash
cd Modulos-de-Kernel
make
```

Após a compilação bem-sucedida, você verá os arquivos `.ko` gerados no diretório.

## Carregando os módulos

### Módulo kfetch

```bash
# Carregue o módulo kfetch
sudo insmod kfetch_mod.ko

# Verifique se o módulo foi carregado corretamente
lsmod | grep kfetch

# Verifique o dispositivo criado
ls -l /dev/kfetch
```

### Módulo risk

```bash
# Carregue o módulo risk
sudo insmod risk_mod.ko

# Verifique se o módulo foi carregado corretamente
lsmod | grep risk

# Verifique a entrada criada em /proc
ls -l /proc/kfetch_risk
```

## Utilizando os módulos

### kfetch_mod

O módulo kfetch cria um dispositivo de caractere `/dev/kfetch` que pode ser lido para obter informações do sistema.

```bash
# Ler informações padrão do sistema
cat /dev/kfetch

# Definir máscara personalizada (bitwise) para filtrar informações específicas:
# Bit 0: Kernel
# Bit 1: Número de CPUs
# Bit 2: Modelo da CPU
# Bit 3: Memória
# Bit 4: Uptime
# Bit 5: Processos
# Exemplo para mostrar apenas kernel e memória (bits 0 e 3):
echo -ne "\x09\x00\x00\x00" > /dev/kfetch  # Valor 9 (1001 em binário)
cat /dev/kfetch
```

### risk_mod

O módulo risk cria uma entrada de procfs que pode ser lida para visualizar informações de risco dos processos.

```bash
# Visualizar avaliação de risco de todos os processos
cat /proc/kfetch_risk
```

## Removendo os módulos

```bash
# Remover o módulo kfetch
sudo rmmod kfetch_mod

# Remover o módulo risk
sudo rmmod risk_mod

# Verificar se os módulos foram removidos
lsmod | grep kfetch
lsmod | grep risk
```

## Limpeza

Para limpar os arquivos compilados:

```bash
make clean
```

## Observações

- Este módulo foi desenvolvido para fins educacionais
- A avaliação de risco é apenas um exemplo simples e não representa uma análise real de segurança
- Certifique-se de ter as permissões adequadas para acessar os dispositivos criados
