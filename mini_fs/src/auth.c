#include "auth.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ─── ficheiros de persistência ──────────────────────────── */
#define USERS_DB "users.db"
#define GROUP_DB "groups.db"

/* ─── tabelas residentes em memória ──────────────────────── */
static GHashTable *users;      /* <name ,User *> */
static GHashTable *groups;     /* <name ,Group *> */

/* ─── sessão ─────────────────────────────────────────────── */
static uint32_t cur_uid = 1000;   /* guest */
static uint32_t cur_gid = 1000;
static uint32_t next_uid = 2001;

/* ─── protótipos internos que o linker cobrava ───────────── */
static void     _load_users(void);
static void     _load_groups(void);
static gboolean _save_users(void);
static gboolean _save_groups(void);

/* ─── setters / getters ──────────────────────────────────── */
void auth_set_uid(uint32_t id){ cur_uid = id; }
void auth_set_gid(uint32_t id){ cur_gid = id; }
uint32_t auth_uid(void)       { return cur_uid; }
uint32_t auth_gid(void)       { return cur_gid; }
bool auth_is_admin(void)      { return cur_uid == 0; }

/* ─── liberação ──────────────────────────────────────────── */
static void _free_user(gpointer p){
    User *u=p;  g_free(u->name); g_free(u->passwd); g_free(u);
}
static void _free_group(gpointer p){
    Group *g=p; g_free(g->name);
    g_list_free_full(g->members,g_free);
    g_free(g);
}

/* ─── helpers ------------------------------------------------*/
User *auth_get_user(const char *n){ return g_hash_table_lookup(users,n); }
Group*auth_get_group(const char *n){return g_hash_table_lookup(groups,n); }

User *auth_get_user_by_uid(uint32_t uid){
    GHashTableIter it; gpointer k,v;
    g_hash_table_iter_init(&it,users);
    while(g_hash_table_iter_next(&it,&k,&v))
        if(((User*)v)->uid==uid) return v;
    return NULL;
}

static bool _gid_exists(uint32_t gid){
    GHashTableIter it; gpointer k,v;
    g_hash_table_iter_init(&it,groups);
    while(g_hash_table_iter_next(&it,&k,&v))
        if(((Group*)v)->gid==gid) return true;
    return false;
}

/* ─── criação de grupo / usuário ─────────────────────────── */
int auth_groupadd(const char *n,uint32_t gid,uint16_t perm)
{
    if(_gid_exists(gid)||g_hash_table_contains(groups,n)) return -1;
    Group*g=g_new0(Group,1);
    g->name=g_strdup(n); g->gid=gid; g->perm=perm&0x1FF;
    g->members=NULL;
    g_hash_table_insert(groups,g_strdup(n),g);
    return 0;
}

static void _bump_uid(uint32_t uid){ if(uid>=next_uid) next_uid=uid+1; }

int auth_useradd(const char *n,uint32_t uid,uint32_t gid,
                 const char *pwd,uint16_t dp)
{
    if(g_hash_table_contains(users,n)||!_gid_exists(gid)) return -1;
    User*u=g_new0(User,1);
    u->name=g_strdup(n); u->uid=uid; u->gid=gid;
    u->passwd=g_strdup(pwd); u->dflt_perms=dp&0x1FF;
    g_hash_table_insert(users,g_strdup(n),u);
    _bump_uid(uid);
    return 0;
}

/* ─── alteração de permissões padrão do usuário ──────────── */
int auth_set_user_perms(const char *n,uint16_t p)
{
    User*u=auth_get_user(n); if(!u) return -1;
    u->dflt_perms=p&0x1FF; return 0;
}

/* ─── grupos suplementares ----------------------------------*/
int auth_add_user_to_group(const char *user,const char *grp)
{
    Group *g=auth_get_group(grp);
    if(!g||!auth_get_user(user)) return -1;
    if(g_list_find_custom(g->members,user,(GCompareFunc)g_strcmp0)) return 0;
    g->members=g_list_append(g->members,g_strdup(user));
    return 0;
}

/* ─── remoção de usuário ------------------------------------*/
static void _remove_from_all(const char *u){
    GHashTableIter it; gpointer k,v;
    g_hash_table_iter_init(&it,groups);
    while(g_hash_table_iter_next(&it,&k,&v)){
        Group*g=v;
        GList*pos=g_list_find_custom(g->members,u,(GCompareFunc)g_strcmp0);
        if(pos){ g_free(pos->data); g->members=g_list_delete_link(g->members,pos);}
    }
}
int auth_delete_user(const char *n)
{
    if(strcmp(n,"root")==0) return -1;
    if(!auth_get_user(n))   return -1;
    _remove_from_all(n);
    g_hash_table_remove(users,n);
    return 0;
}

/* ─── permissões genéricas / arquivo ───────────────────────*/
bool auth_user_in_group(uint32_t uid,uint32_t gid)
{
    Group *g=NULL; GHashTableIter it;gpointer k,v;
    g_hash_table_iter_init(&it,groups);
    while(g_hash_table_iter_next(&it,&k,&v)){
        g=v; if(g->gid==gid) break; else g=NULL;
    }
    if(!g) return false;
    return g_list_find_custom(g->members,
                              auth_get_user_by_uid(uid)->name,
                              (GCompareFunc)g_strcmp0)!=NULL;
}
bool auth_has_perm_mode(uint32_t owner,uint32_t group,
                        uint16_t perms,uint16_t bit)
{
    if(auth_uid()==0) return true;            /* root */
    uint16_t cls = (auth_uid()==owner) ? (perms>>6)&7 :
                   (auth_gid()==group||
                    auth_user_in_group(auth_uid(),group))
                                 ? (perms>>3)&7 : perms&7;
    return (cls & bit)!=0;
}
bool auth_has_perm(const FCB *f,uint16_t bit)
{ return auth_has_perm_mode(f->owner,f->group,f->perms,bit); }

/* ─── persistência (users & groups) ───────────────────────── */
static gboolean _save_users(void)
{
    FILE *fp=fopen(USERS_DB,"w");
    if(!fp) return FALSE;
    GHashTableIter it;gpointer k,v;
    g_hash_table_iter_init(&it,users);
    while(g_hash_table_iter_next(&it,&k,&v)){
        User*u=v;
        fprintf(fp,"%s %u %u %s %hu\n",
                u->name,u->uid,u->gid,u->passwd,u->dflt_perms);
    }
    fclose(fp); return TRUE;
}
static void _serialize_members(GString*s,GList*m){
    for(GList*l=m;l;l=l->next){
        g_string_append(s,(char*)l->data);
        if(l->next) g_string_append_c(s,',');
    }
}
static gboolean _save_groups(void)
{
    GString*out=g_string_new(NULL);
    GHashTableIter it;gpointer k,v;
    g_hash_table_iter_init(&it,groups);
    while(g_hash_table_iter_next(&it,&k,&v)){
        Group*g=v;
        g_string_append_printf(out,"%s %u %hu ",g->name,g->gid,g->perm);
        _serialize_members(out,g->members);
        g_string_append_c(out,'\n');
    }
    gboolean ok=g_file_set_contents(GROUP_DB,out->str,out->len,NULL);
    g_string_free(out,TRUE);
    return ok;
}
int auth_save(void){ return _save_users()&&_save_groups()?0:-1; }

/* ---- _load helpers --------------------------------------- */
static void _load_users(void)
{
    gchar*txt=NULL; gsize len=0;
    if(!g_file_get_contents(USERS_DB,&txt,&len,NULL)||!txt) return;
    gchar **ln=g_strsplit(txt,"\n",-1);
    for(gchar **l=ln;*l;++l){
        if(!**l) continue;
        char n[64],p[128];uint32_t uid,gid;uint16_t dp;
        if(sscanf(*l,"%63s %u %u %127s %hu",n,&uid,&gid,p,&dp)==5)
            auth_useradd(n,uid,gid,p,dp);
    }
    g_strfreev(ln); g_free(txt);
}
static void _load_groups(void)
{
    gchar*txt=NULL;gsize len=0;
    if(!g_file_get_contents(GROUP_DB,&txt,&len,NULL)||!txt) return;
    gchar **ln=g_strsplit(txt,"\n",-1);
    for(gchar **l=ln;*l;++l){
        if(!**l) continue;
        char n[64],mb[512]="";uint32_t gid;uint16_t perm;
        sscanf(*l,"%63s %u %hu %[^\n]",n,&gid,&perm,mb);
        if(auth_groupadd(n,gid,perm)!=0) continue;
        Group*g=auth_get_group(n);
        if(*mb){
            gchar **m=g_strsplit(mb,",",-1);
            for(gchar **x=m;*x;++x) if(**x)
                g->members=g_list_append(g->members,g_strdup(*x));
            g_strfreev(m);
        }
    }
    g_strfreev(ln); g_free(txt);
}

/* ─── criação de conta interativa --------------------------- */
static bool _create_account(void)
{
    char n[32],p[32];
    printf("Novo usuário ‒ nome: "); fgets(n,sizeof n,stdin);
    n[strcspn(n,"\n")]='\0';
    if(!*n||auth_get_user(n)){ puts("Nome inválido/existente"); return false;}

    printf("Senha: "); fgets(p,sizeof p,stdin);
    p[strcspn(p,"\n")]='\0';

    uint32_t uid=next_uid++;
    auth_useradd(n,uid,1000,p,0664);
    auth_set_uid(uid); auth_set_gid(1000);
    printf("Conta criada (UID=%u)\n",uid);
    auth_save();
    return true;
}

/* ─── init / login / logout --------------------------------- */
void auth_init(void)
{
    users  = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,_free_user);
    groups = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,_free_group);

    auth_groupadd("root",  0,0);
    auth_groupadd("guest",1000,0);
    auth_useradd ("admin",0,0,"admin",0666);
    auth_useradd ("guest",1000,1000,"",0664);

    _load_users();
    _load_groups();
}

bool auth_login(void)
{
    char op[8];
    while(1){
        printf("Login (l)  Novo (n)  Sair (q) ? "); fgets(op,8,stdin);
        if(tolower(op[0])=='q') return false;
        if(tolower(op[0])=='n') return _create_account();
        if(tolower(op[0])!='l') continue;

        char n[32],p[32];
        printf("Usuário: "); fgets(n,32,stdin); n[strcspn(n,"\n")]='\0';
        User*u=auth_get_user(n); if(!u){ puts("Inexistente"); continue; }

        printf("Senha: "); fgets(p,32,stdin); p[strcspn(p,"\n")]='\0';
        if(strcmp(p,u->passwd)!=0){ puts("Senha incorreta"); continue; }

        auth_set_uid(u->uid); auth_set_gid(u->gid);
        printf("Bem-vindo, %s!\n",u->name);
        return true;
    }
}
void auth_logout(void)
{
    auth_set_uid(1000); auth_set_gid(1000);
    puts("logout → guest");
}
