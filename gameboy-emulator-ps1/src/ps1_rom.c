#include <stdint.h>
#include <psxcd.h>

#include "ps1_rom.h"

/* CdRead schrijft per sector 2048 bytes en wil een 32-bit uitgelijnde buffer.
 * Statisch (.bss): kost niets in de .EXE zelf. */
static uint32_t rom_buf[PS1_ROM_MAX_SIZE / 4];
static size_t rom_size;

int ps1_rom_load(const char *path)
{
	CdlFILE file;
	uint8_t result[8];

	rom_size = 0;

	if (!CdInit())
		return PS1_ROM_ERR_CDINIT;

	if (!CdSearchFile(&file, path))
		return PS1_ROM_ERR_NOTFOUND;

	if (file.size <= 0 || file.size > PS1_ROM_MAX_SIZE)
		return PS1_ROM_ERR_SIZE;

	const int sectors = (file.size + 2047) / 2048;

	/* CdRead begint op de laatst met CdlSetloc ingestelde positie. */
	if (CdControl(CdlSetloc, &file.pos, result) != 1)
		return PS1_ROM_ERR_SETLOC;
	CdSync(0, result);

	/* Dubbele snelheid, tot 3 pogingen (nuttig op echte hardware/CD-R). */
	if (!CdReadRetry(sectors, rom_buf, CdlModeSpeed, 3))
		return PS1_ROM_ERR_READ;
	if (CdReadSync(0, result) < 0)
		return PS1_ROM_ERR_READ;

	rom_size = (size_t) file.size;
	return PS1_ROM_OK;
}

const uint8_t *ps1_rom_data(void)
{
	return (const uint8_t *) rom_buf;
}

size_t ps1_rom_size(void)
{
	return rom_size;
}
