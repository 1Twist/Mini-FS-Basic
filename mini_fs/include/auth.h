#ifndef AUTH_H
#define AUTH_H
/*-----------------------------------------------------------
 *  Autenticação + Controle de Acesso
 *-----------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include <glib.h>
#include "fs.h"

/* ───── estruturas ───────────────────────────────────────── */
typedef struct {
    char    *name;
    uint32_t uid, gid;          /* primário                      */
    char    *passwd;            /* texto-plano (demo)            */
    uint16_t dflt_perms;        /* rwx rwx rwx p/ novos arquivos */
} User;

typedef struct {
    char    *name;
    uint32_t gid;
    uint16_t perm;              /* (unused – reservado)          */
    GList   *members;           /* lista de char*                */
} Group;

/* ───── sessão atual ─────────────────────────────────────── */
bool      auth_login(void);           /* prompt    */
void      auth_logout(void);
bool      auth_is_admin(void);        /* uid==0    */
uint32_t  auth_uid(void);
uint32_t  auth_gid(void);
void      auth_set_uid(uint32_t uid); /* su        */
void      auth_set_gid(uint32_t gid); /* sg        */

/* ───── CRUD usuários / grupos ───────────────────────────── */
int  auth_groupadd (const char *name,uint32_t gid,uint16_t perm);
int  auth_useradd  (const char *name,uint32_t uid,uint32_t gid,
                    const char *pwd,uint16_t def_perms);
int  auth_delete_user(const char *name);
int  auth_add_user_to_group(const char *user,const char *group);
int  auth_set_user_perms(const char *name,uint16_t perms);

/* ───── pesquisa ─────────────────────────────────────────── */
User  *auth_get_user(const char *name);
User  *auth_get_user_by_uid(uint32_t uid);
Group *auth_get_group(const char *name);
int    auth_get_gid_by_name(const char *name,uint32_t *out_gid);

/* ───── permissões ───────────────────────────────────────── */
bool auth_has_perm     (const FCB *f,uint16_t bit);        /* arquivo */
bool auth_has_perm_mode(uint32_t owner,uint32_t group,
                        uint16_t perms,uint16_t bit);      /* genérico*/

/* ───── inicialização / persistência ─────────────────────── */
void auth_init(void);          /* lê users.db / groups.db ou cria padrão */
int  auth_save(void);          /* grava nos mesmos arquivos               */

#endif /* AUTH_H */
