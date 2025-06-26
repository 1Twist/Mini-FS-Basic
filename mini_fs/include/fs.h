#ifndef FS_H
#define FS_H
/*───────────────────────────────────────────────────────────*/
/*  Mini–filesystem – camada de arquivos/blocos              */
/*───────────────────────────────────────────────────────────*/
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <glib.h>
#include "directory.h"
#include "block.h"

/* ───── bits de permissão ─────
 *            rwx rwx rwx
 *             o   g   p   */
#define P_READ   4            /* r-bit */
#define P_WRITE  2            /* w-bit */
#define P_EXEC   1            /* x-bit */
#define PERM(o,g,u)  (((o) << 6) | ((g) << 3) | (u))

typedef enum { F_DATA = 0, F_PROGRAM = 1 } ftype_t;

typedef struct fcb {
    char      *name;                        /* cópia alocada            */
    uint32_t   inode;
    uint32_t   owner;                       /* UID do criador           */
    uint32_t   group;                       /* GID primário do criador  */
    size_t     size;                        /* bytes válidos            */
    ftype_t    type;
    time_t     created, modified, accessed;
    uint16_t   perms;                       /* 9 bits rwxrwxrwx          */
    GPtrArray *blocks;                      /* vetor de índices físicos */
} FCB;

/* ───── API ───── */
void fs_init(void);

int  fs_touch (const char *name);
int  fs_echo  (const char *name, const char *txt, int append);
int  fs_cat   (const char *name);
int  fs_rm    (const char *name);
int  fs_cp    (const char *src,  const char *dst);
int  fs_mv    (const char *src,  const char *dst);
int  fs_chmod (const char *name, uint16_t new_mode);

#endif /* FS_H */
