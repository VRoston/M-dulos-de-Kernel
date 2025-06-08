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
sudo apt update
```

```bash
sudo apt install build-essential linux-headers-$(uname -r)
```

## Compilação dos módulos

1. Clone este repositório ou extraia os arquivos em um diretório
2. Navegue até o diretório dos módulos
3. Execute o comando make para compilar:

```bash
cd <Seu_caminho_até_o_diretório>/Modulos-de-Kernel
make
```

Após a compilação bem-sucedida, você verá os arquivos `.ko` gerados no diretório.

## Carregando os módulos

### Módulo kfetch

```bash
# Carregue o módulo kfetch
sudo insmod kfetch_mod.ko
```
```bash
# Verifique se o módulo foi carregado corretamente
lsmod | grep kfetch
```
```bash
# Verifique o dispositivo criado
ls -l /dev/kfetch
```

### Módulo risk

```bash
# Carregue o módulo risk
sudo insmod risk_mod.ko
```
```bash
# Verifique se o módulo foi carregado corretamente
lsmod | grep risk
```
```bash
# Verifique a entrada criada em /proc
ls -l /proc/kfetch_risk
```

## Utilizando os módulos

### kfetch_mod

O módulo kfetch cria um dispositivo de caractere `/dev/kfetch` que pode ser lido pelo kfetch.c, primeiro ele deve ser copilado.

```bash
gcc -g -o kfetch kfetch.c
```
Para ler as infomação utilize abaixo:

```bash
sudo ./kfetch
```

Para filtrar as informações que serão exibidas passe um parametro que determinará a sua mascara:  

```bash
# A máscara determina o que será exibido transformando o número
# passado em binário e mostrando a informação equivalente que tem o valor 1
sudo ./kfetch 1
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
```
```bash
# Remover o módulo risk
sudo rmmod risk_mod
```
```bash
# Verificar se os módulos foram removidos
lsmod | grep kfetch
```
```bash
lsmod | grep risk
```

## Limpeza

Para limpar os arquivos compilados:

```bash
make clean
```
## Known issues

- Na versão 22 do Ubuntu uma função ```class_create``` quebra por falta de um parametro, para solucionar va na linha(214) desta função e mude a funlção para ```class_create(THIS_MODULE, CLASS_NAME);```.
- Nós testamos somente em ```UbuntuServer 22, 24``` não damos serteza que irá fucionar em outro sistemas.
