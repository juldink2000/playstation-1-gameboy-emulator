#ifndef PS1_ROM_H
#define PS1_ROM_H

#include <stdint.h>
#include <stddef.h>

/* Max ROM-grootte in deze eerste versie: 1 MiB (= Pokemon Red, 64 banken). */
#define PS1_ROM_MAX_SIZE (1024 * 1024)

/* Foutcodes van ps1_rom_load(). */
#define PS1_ROM_OK            0
#define PS1_ROM_ERR_CDINIT   -1
#define PS1_ROM_ERR_NOTFOUND -2
#define PS1_ROM_ERR_SIZE     -3
#define PS1_ROM_ERR_SETLOC   -4
#define PS1_ROM_ERR_READ     -5

/* Initialiseert de CD-drive en leest `path` (bv. "\\POKEMONR.GB;1") volledig
 * in een statische buffer. Geeft PS1_ROM_OK of een negatieve foutcode. */
int ps1_rom_load(const char *path);

const uint8_t *ps1_rom_data(void);
size_t ps1_rom_size(void);

#endif
