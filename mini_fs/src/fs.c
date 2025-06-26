#include "fs.h"
#include "auth.h"
#include <stdio.h>
#include <string.h>

/*  simples contador de inodes (único) ------------------------------- */
static uint32_t next_inode = 1;

/*  cria FCB inicializado consoante máscara-padrão do usuário -------- */
static FCB *_new_fcb(const char *name)
{
    User *me  = auth_get_user_by_uid(auth_uid());
    uint16_t dflt = me ? me->dflt_perms : 0666;   /* guest → 0666       */

    FCB *f    = g_new0(FCB, 1);
    f->name   = g_strdup(name);
    f->inode  = next_inode++;
    f->owner  = auth_uid();
    f->group  = auth_gid();
    f->perms  = dflt & 0777;
    f->type   = F_DATA;
    f->created = f->modified = f->accessed = time(NULL);
    f->blocks  = g_ptr_array_new_with_free_func(NULL);
    return f;
}

/*───────────────────────────────────────────────────────────*/
void fs_init(void)
{
    block_init();
    dir_init();
}

/*  helpers internos --------------------------------------- */
static FCB *_lookup(const char *name)
{
    Dir *cwd = dir_get_cwd();
    return cwd ? g_hash_table_lookup(cwd->files, name) : NULL;
}

static int _ensure_capacity(FCB *f, size_t new_sz)
{
    size_t need = (new_sz + BLOCK_SIZE - 1) / BLOCK_SIZE;
    while (f->blocks->len < need) {
        int idx = block_alloc();
        if (idx < 0) return -1;
        g_ptr_array_add(f->blocks, GINT_TO_POINTER(idx));
    }
    return 0;
}

/*──────────────────── criação vazia ───────────────────────*/
int fs_touch(const char *name)
{
    Dir *cwd = dir_get_cwd();
    if (!cwd || !name || !*name || strchr(name,'/')) return -1;
    if (!auth_has_perm_mode(cwd->owner,cwd->group,cwd->perms,P_WRITE)){
        puts("Permissão negada");
        return -1;
    }
    if (g_hash_table_contains(cwd->files, name))     return -1;

    g_hash_table_insert(cwd->files, g_strdup(name), _new_fcb(name));
    return 0;
}

/*──────────────────── escrita / append ─────────────────────*/
int fs_echo(const char *name, const char *txt, int append)
{
    if (!name || !txt) return -1;
    size_t len = strlen(txt);

    FCB *f = _lookup(name);
    if (!f) {                       /* cria se não existe          */
        if (fs_touch(name)) return -1;
        f = _lookup(name);
    }

    if (!auth_has_perm(f, P_WRITE)) {
        puts("Permissão negada");
        return -1;
    }

    size_t offset = append ? f->size : 0;
    size_t new_sz = offset + len;
    if (_ensure_capacity(f, new_sz)) return -1;

    size_t rem = len, pos = 0;
    while (rem) {
        size_t bi = (offset + pos) / BLOCK_SIZE;
        size_t bo = (offset + pos) % BLOCK_SIZE;
        size_t chunk = BLOCK_SIZE - bo;
        if (chunk > rem) chunk = rem;

        int phys = GPOINTER_TO_INT(g_ptr_array_index(f->blocks, bi));
        block_write(phys, txt + pos, chunk, bo);

        pos += chunk; rem -= chunk;
    }
    f->size = new_sz;
    f->modified = time(NULL);
    return (int)len;
}

/*──────────────────── leitura (cat) ───────────────────────*/
int fs_cat(const char *name)
{
    Dir *cwd = dir_get_cwd();
    if (!cwd || !auth_has_perm_mode(cwd->owner,cwd->group,cwd->perms,
                                    P_READ|P_EXEC)){
        puts("Permissão negada");
        return -1;
    }
    FCB *f = _lookup(name);
    if (!f) return -1;
    if (!auth_has_perm(f, P_READ)) {
        puts("Permissão negada");
        return -1;
    }

    size_t rem=f->size,pos=0; char buf[BLOCK_SIZE];
    while (rem) {
        size_t bi = pos / BLOCK_SIZE, bo = pos % BLOCK_SIZE;
        size_t chunk = BLOCK_SIZE - bo; if (chunk > rem) chunk = rem;
        int phys = GPOINTER_TO_INT(g_ptr_array_index(f->blocks,bi));
        block_read(phys, buf, chunk, bo);
        fwrite(buf,1,chunk,stdout);
        pos += chunk; rem -= chunk;
    }
    if (f->size) putchar('\n');
    f->accessed = time(NULL);
    return 0;
}

/*──────────────────── remoção ─────────────────────────────*/
int fs_rm(const char *name)
{
    Dir *cwd = dir_get_cwd();
    if (!cwd ||
        !auth_has_perm_mode(cwd->owner,cwd->group,cwd->perms,P_WRITE)){
        puts("Permissão negada");
        return -1;
    }
    return g_hash_table_remove(cwd->files,name) ? 0 : -1;
}

/*──────────────────── cópia --------------------------------*/
int fs_cp(const char *src, const char *dst)
{
    Dir *cwd = dir_get_cwd();
    if (!cwd ||
        !auth_has_perm_mode(cwd->owner,cwd->group,cwd->perms,P_WRITE)){
        puts("Permissão negada");
        return -1;
    }
    if (_lookup(dst)) return -1;
    FCB *orig = _lookup(src); if (!orig) return -1;
    if (!auth_has_perm(orig,P_READ)) { puts("Permissão negada"); return -1; }

    if (fs_touch(dst)) return -1;
    FCB *copy = _lookup(dst);

    copy->size  = orig->size;
    copy->type  = orig->type;
    copy->perms = orig->perms;
    copy->owner = auth_uid();
    copy->group = auth_gid();

    for (guint i=0;i<orig->blocks->len;++i) {
        int nb = block_alloc(); if (nb < 0) return -1;
        int ob = GPOINTER_TO_INT(g_ptr_array_index(orig->blocks,i));
        char buf[BLOCK_SIZE];
        block_read(ob,buf,BLOCK_SIZE,0);
        block_write(nb,buf,BLOCK_SIZE,0);
        g_ptr_array_add(copy->blocks,GINT_TO_POINTER(nb));
    }
    copy->created = copy->modified = time(NULL);
    return 0;
}

/*──────────────────── rename / move ───────────────────────*/
int fs_mv(const char *src, const char *dst)
{
    Dir *cwd = dir_get_cwd();
    if (!cwd || _lookup(dst)) return -1;
    if (!auth_has_perm_mode(cwd->owner,cwd->group,cwd->perms,P_WRITE)){
        puts("Permissão negada");
        return -1;
    }
    gpointer v = g_hash_table_lookup(cwd->files, src);
    if (!v) return -1;

    g_hash_table_steal(cwd->files, src);
    ((FCB*)v)->name = g_strdup(dst);
    g_hash_table_insert(cwd->files, g_strdup(dst), v);
    return 0;
}

/*──────────────────── chmod (restrito) ────────────────────*/
int fs_chmod(const char *name, uint16_t mode)
{
    FCB *f = _lookup(name); if (!f) return -1;
    uint16_t desired = mode & 0777;
    uint16_t old     = f->perms;

    /* root pode tudo ------------------------------------------------*/
    if (auth_uid() == 0) { f->perms = desired; return 0; }

    /* somente o dono pode (além do root) ---------------------------*/
    if (auth_uid() != f->owner) {
        puts("chmod: apenas o dono ou root");
        return -1;
    }

    /* o dono nunca altera bits owner de outros; pode mexer livremente
       nos 3 bits owner, mas *não pode adicionar* privil. para
       group/public acima dos já existentes.                         */
    uint16_t new_owner = desired & 0700;
    uint16_t new_group = desired & 0070;
    uint16_t new_pub   = desired & 0007;

    /* group/public só podem PERDER bits (ou manter) */
    new_group &= ((old >> 3) & 7);
    new_pub   &=  (old       & 7);

    f->perms = new_owner | (new_group << 3) | new_pub;
    return 0;
}
