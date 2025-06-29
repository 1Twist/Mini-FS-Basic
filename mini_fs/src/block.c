#include "block.h"
#include <string.h>     /* memset, memcpy */
#include <assert.h>

/*  Área de dados: bloco físico × bytes  ---------------------------------- */
static uint8_t _data[BLOCK_COUNT][BLOCK_SIZE];

/*  Bitmap: 1 bit por bloco  ---------------------------------------------- */
static uint8_t _bitmap[(BLOCK_COUNT + 7) / 8];

/*  Helpers para operar na bitmap  ---------------------------------------- */
static inline void _set_bit(int idx)   { _bitmap[idx >> 3] |=  (1U << (idx & 7)); }
static inline void _clr_bit(int idx)   { _bitmap[idx >> 3] &= ~(1U << (idx & 7)); }
static inline int  _tst_bit(int idx)   { return _bitmap[idx >> 3] &   (1U << (idx & 7)); }

/* ------------------------------------------------------------------------ */
void block_init(void)
{
    memset(_bitmap, 0, sizeof(_bitmap));     /* tudo livre                 */
    /* opcional: limpar dados para zero ‒ não é estritamente necessário */
}

/* ------------------------------------------------------------------------ */
int block_alloc(void)
{
    for (int i = 0; i < BLOCK_COUNT; ++i) {
        if (!_tst_bit(i)) {           /* livre? */
            _set_bit(i);
            memset(_data[i], 0, BLOCK_SIZE); /* zera conteúdo */
            return i;
        }
    }
    return -1;                        /* sem espaço */
}

/* ------------------------------------------------------------------------ */
void block_free(int index)
{
    if (index < 0 || index >= BLOCK_COUNT) return;
    _clr_bit(index);
    /* opcional: zerar dados para evitar “lixo” residual           */
    memset(_data[index], 0, BLOCK_SIZE);
}

/* ------------------------------------------------------------------------ */
size_t block_write(int index,
                   const void *buf,
                   size_t len,
                   size_t offset)
{
    if (index < 0 || index >= BLOCK_COUNT || !_tst_bit(index))
        return 0;                                   /* bloco inválido ou livre */

    if (offset >= BLOCK_SIZE) return 0;

    size_t max = BLOCK_SIZE - offset;
    if (len > max) len = max;

    memcpy(_data[index] + offset, buf, len);
    return len;
}

/* ------------------------------------------------------------------------ */
size_t block_read(int index,
                  void *buf,
                  size_t len,
                  size_t offset)
{
    if (index < 0 || index >= BLOCK_COUNT || !_tst_bit(index))
        return 0;

    if (offset >= BLOCK_SIZE) return 0;

    size_t max = BLOCK_SIZE - offset;
    if (len > max) len = max;

    memcpy(buf, _data[index] + offset, len);
    return len;
}

/* ------------------------------------------------------------------------ */
bool block_is_free(int index)
{
    return (index < 0 || index >= BLOCK_COUNT) ? true : !_tst_bit(index);
}
