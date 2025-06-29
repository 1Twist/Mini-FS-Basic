#ifndef DIRECTORY_H
#define DIRECTORY_H
/*───────────────────────────────────────────────────────────*/
/*  Directory Manager – árvore de diretórios em memória      */
/*───────────────────────────────────────────────────────────*/
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <glib.h>

/* bits herdados de fs.h –  copiados aqui para evitar include-ciclo */
#define P_READ   4
#define P_WRITE  2
#define P_EXEC   1

typedef struct dir_node {
    /* ─── segurança ───────────────────────────────────────── */
    uint32_t            owner;          /* UID do criador            */
    uint32_t            group;          /* GID primário do criador   */
    uint16_t            perms;          /* rwx rwx rwx (9 bits)      */

    /* ─── hierarquia ──────────────────────────────────────── */
    char               *name;           /* "foo"                     */
    struct dir_node    *parent;         /* NULL na raiz “/”          */
    GTree              *subdirs;        /* <nome,Dir*> ordenado      */
    GHashTable         *files;          /* <nome,FCB*> (fs.c)        */
} Dir;

/* ─── API ────────────────────────────────────────────────── */
void        dir_init   (void);                    /* cria raiz              */
int         dir_mkdir  (const char *name);        /* mkdir                  */
int         dir_cd     (const char *path);        /* cd / a/../x            */
void        dir_ls     (gboolean long_fmt);       /* ls / ls -l             */
const char *dir_pwd    (char *buf,size_t n);      /* caminho textual        */

Dir        *dir_get_cwd(void);                    /* CWD p/ fs.c            */
bool        dir_has_perm(const Dir *d,uint16_t bit);/* checagem de permissão */

#endif /* DIRECTORY_H */