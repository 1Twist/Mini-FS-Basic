#ifndef BLOCK_H
#define BLOCK_H
/*---------------------------------------------------------------------------
 *  Block Manager — mini-filesystem em memória
 *---------------------------------------------------------------------------*/

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Tamanho de cada bloco (bytes) e quantidade total de blocos ------------- */
#define BLOCK_SIZE   4096          /* 4 KiB              */
#define BLOCK_COUNT  1024          /* 4 MiB (1024×4 KiB) */

/* API pública ------------------------------------------------------------- */
void     block_init(void);

int      block_alloc(void);                       /* retorna índice (0+) ou −1  */
void     block_free(int index);

size_t   block_write(int index,
                     const void *buf,
                     size_t len,
                     size_t offset);              /* bytes realmente escritos   */

size_t   block_read(int index,
                    void *buf,
                    size_t len,
                    size_t offset);               /* bytes realmente lidos      */

bool     block_is_free(int index);

#endif /* BLOCK_H */
