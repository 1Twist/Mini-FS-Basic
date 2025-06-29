
 #include "directory.h"
#include "auth.h"       /* para UID/GID e permissões */
#include "fs.h"         /* para _destroy_fcb e blocos */
#include <glib.h>
#include <stdio.h>
#include <string.h>

/*──────────────── globais ─────────────*/
static Dir *root = NULL;
static Dir *cwd  = NULL;

/* comparação alfabética para GTree */
static gint _cmp(gconstpointer a,gconstpointer b,gpointer u){ (void)u;
    return g_strcmp0(a,b);
}

/*──────────────── helpers de perm ─────*/
/* verifica acesso a um diretório seguindo mesma lógica do auth */
bool dir_has_perm(const Dir *d,uint16_t bit)
{
    return auth_has_perm_mode(d->owner,d->group,d->perms,bit);
}

/*──────────────── destrutor de arquivos dentro da pasta ─────────────*/
static void _destroy_fcb(gpointer data)
{
    FCB *f = data; if(!f) return;
    for(guint i=0;i<f->blocks->len;++i)
        block_free(GPOINTER_TO_INT(g_ptr_array_index(f->blocks,i)));
    g_ptr_array_free(f->blocks,TRUE);
    g_free(f->name); g_free(f);
}

/*──────────────── criar novo nó Dir ───*/
static uint16_t _dir_default_perms(void)
{
    if (auth_is_admin())      return 0700;
    if (auth_gid() != 1000)   return 0770;
    return 0777;
}

static Dir *_dir_new(const char *name,Dir *parent)
{
    Dir *d = g_new0(Dir,1);
    d->name   = g_strdup(name);
    d->parent = parent;

    d->perms = _dir_default_perms();

    d->owner = auth_uid();
    d->group = auth_gid();

    d->subdirs = g_tree_new_full(_cmp,NULL,g_free,NULL);
    d->files   = g_hash_table_new_full(
                    g_str_hash,g_str_equal,g_free,_destroy_fcb);
    return d;
}

/*──────────────── init raiz ───────────*/
void dir_init(void)
{
    if(root) return;
    root = _dir_new("/",NULL);
    root->perms = 0755;              /* raiz sempre pública leitura/x   */
    root->owner = 0;
    root->group = 0;
    cwd  = root;
}

Dir *dir_get_cwd(void){ return cwd; }

/*──────────────── pwd ─────────────────*/
const char *dir_pwd(char *buf,size_t n)
{
    if(!buf||!n) return NULL;
    GPtrArray *stk=g_ptr_array_new();
    for(Dir*d=cwd;d;d=d->parent) g_ptr_array_add(stk,d);

    buf[0]='\0';
    for(gint i=stk->len-1;i>=0;--i){
        Dir*d=g_ptr_array_index(stk,i);
        g_strlcat(buf,(d==root)?"/":d->name,n);
        if(i>0 && d!=root) g_strlcat(buf,"/",n);
    }
    g_ptr_array_free(stk,TRUE);
    return buf;
}

/*──────────────── mkdir ───────────────*/
int dir_mkdir(const char *name)
{
    if(!cwd||!name||!*name||strchr(name,'/')) return -1;
    if(!dir_has_perm(cwd,P_WRITE|P_EXEC)) { puts("Permissão negada"); return -1; }
    if(g_tree_lookup(cwd->subdirs,name))  return -1;

    Dir*nd=_dir_new(name,cwd);
    g_tree_insert(cwd->subdirs,g_strdup(name),nd);
    return 0;
}

/*──────────────── resolver componente ---*/
static Dir *_step(Dir *cur,const char *comp)
{
    if(strcmp(comp,".")==0)  return cur;
    if(strcmp(comp,"..")==0) return cur->parent?cur->parent:cur;
    return g_tree_lookup(cur->subdirs,comp);
}

/*──────────────── resolver caminho (checa X a cada passo) ───────────*/
static Dir *_resolve(const char *path)
{
    Dir *cur = (*path=='/')? root : cwd;
    if(!dir_has_perm(cur,P_EXEC)) return NULL;

    gchar **parts = g_strsplit(path,"/",-1);
    for(gchar **p=parts;*p;++p){
        if(**p=='\0') continue;
        cur = _step(cur,*p);
        if(!cur || !dir_has_perm(cur,P_EXEC)){ cur=NULL; break; }
    }
    g_strfreev(parts);
    return cur;
}

/*──────────────── cd ─────────────────*/
int dir_cd(const char *path)
{
    if(!path||!*path) return -1;
    Dir *d=_resolve(path);
    if(!d) { puts("Permissão negada ou diretório inex."); return -1; }
    cwd=d; return 0;
}

/*──────────────── impressão ls helper ─*/
static gboolean _print_one(gpointer k,gpointer v,gpointer d)
{
    (void)v;
    gboolean longf = GPOINTER_TO_INT(d);
    if(longf) g_print("<DIR>\t%s/\n",(char*)k);
    else      g_print("%s/\t",(char*)k);
    return FALSE;
}

/*──────────────── ls ─────────────────*/
void dir_ls(gboolean longf)
{
    if(!cwd) return;
    if(!dir_has_perm(cwd,P_READ)) { puts("Permissão negada"); return; }

    g_tree_foreach(cwd->subdirs,_print_one,GINT_TO_POINTER(longf));

    GHashTableIter it; gpointer k,v;
    g_hash_table_iter_init(&it,cwd->files);
    while(g_hash_table_iter_next(&it,&k,&v)){
        if(longf) g_print("     \t%s\n",(char*)k);
        else      g_print("%s\t",(char*)k);
    }
    if(!longf) putchar('\n');
}
