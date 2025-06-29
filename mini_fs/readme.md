# Mini-FS Basic

Este projeto implementa um simulador de sistema de arquivos em memória,
escrito em C e utilizando a biblioteca GLib. O programa fornece uma
pequena shell (`mfs`) capaz de criar usuários, gerenciar permissões e
manipular arquivos e diretórios em uma estrutura hierárquica.

## Compilação e Execução

Requisitos:

- `gcc` e `make`;
- `pkg-config` com a biblioteca **glib-2.0** instalada.

Para compilar o simulador:

```bash
cd mini_fs
make
```

O binário resultante fica em `mini_fs/mfs`. Para iniciar o sistema:

```bash
./mfs
```

Ao executar, o programa apresenta um prompt de login. Há contas
`admin` e `guest` disponíveis por padrão, mas novas contas podem ser
criadas.

## Estruturas de Dados e Design

### File Control Block (FCB)

A estrutura `FCB`, declarada em [`include/fs.h`](include/fs.h), atua como
um inode simplificado:

```c
typedef struct fcb {
    char      *name;
    uint32_t   inode;
    uint32_t   owner;
    uint32_t   group;
    size_t     size;
    ftype_t    type;
    time_t     created, modified, accessed;
    uint16_t   perms;
    GPtrArray *blocks;
} FCB;
```

O campo `blocks` guarda índices de blocos de dados alocados, permitindo
uma forma de **alocação indexada**.

### Diretórios em Árvore

Diretórios são representados pela estrutura `Dir`, definida em
[`include/directory.h`](include/directory.h):

```c
typedef struct dir_node {
    uint32_t            owner;
    uint32_t            group;
    uint16_t            perms;
    char               *name;
    struct dir_node    *parent;
    GTree              *subdirs;
    GHashTable         *files;
} Dir;
```

Cada nó mantém ponteiros para seu pai, subdiretórios e arquivos,
formando uma árvore eficiente para busca de caminhos.

### Gerenciador de Blocos

O arquivo [`block.c`](src/block.c) controla um conjunto de 1024 blocos de
4&nbsp;KiB definidos em [`block.h`](include/block.h):

```c
#define BLOCK_SIZE   4096
#define BLOCK_COUNT  1024
```

Um bitmap indica quais blocos estão livres. Cada `FCB` referencia os
blocos que possui por meio do vetor `blocks`.

## Operações Implementadas

A mini-shell oferece comandos como:

- `pwd`, `ls [-l]`, `mkdir <dir>`, `cd <dir>`
- `touch <arq>`, `echo "txt" > arq`, `echo "txt" >> arq`
- `cat <arq>`, `rm <arq>`, `cp <orig> <dest>`, `mv <orig> <dest>`
- `chmod <octal> <arq>` (restrito ao dono ou ao administrador)
- Gerenciamento de usuários e grupos: `useradd`, `userdel`, `groupadd`,
  `joingroup`, `sg`, `su` e outros

Essas funções demonstram as principais operações de sistemas de
arquivos: criação, leitura, escrita, exclusão e controle de acesso.

## Conceitos Demonstrados

- **Atributos de Arquivo** – cada `FCB` registra nome, tamanho, datas e
  permissões;
- **Operações de Arquivo** – criação, gravação, leitura e remoção;
- **File Control Block / inode** – o vetor de índices e o contador
  interno `next_inode` simulam um inode simplificado;
- **Árvore de Diretórios** – diretórios ligados via ponteiros para
  eficiência de busca e agrupamento;
- **Proteção e Permissões** – bits `rwx` para dono, grupo e público,
  verificados em cada operação, com suporte ao comando `chmod`;
- **Alocação de Blocos** – os índices armazenados no `FCB` exemplificam a
  técnica de alocação indexada.

## Execução do Simulador

Após compilar, execute `./mfs`, faça login e utilize os comandos
acima (digite `help` para listá-los). O estado do sistema de arquivos é
euclidicamente mantido apenas nos arquivos `users.db` e `groups.db` para
preservar contas e grupos entre execucoes.


## Organizacao do Codigo

O projeto esta organizado da seguinte forma:

- `mini_fs/src/` contem os arquivos de implementacao em C.
- `mini_fs/include/` traz os cabecalhos compartilhados.
- `mini_fs/build/` guarda os objetos gerados pelo `make`.
- Os bancos `users.db` e `groups.db` registram os perfis existentes.

O sistema de arquivos propriamente dito é volátil e reiniciado a cada
execução do programa; somente as contas são preservadas.

## Sessao de Exemplo

```bash
$ ./mfs
login: guest
password: 
$/ mkdir docs
$/ cd docs
/docs$ touch notas
/docs$ echo "Ola" > notas
/docs$ cat notas
Ola
```