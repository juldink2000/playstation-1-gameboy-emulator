#include <string.h>
#include <psxapi.h>
#include <psxpad.h>

#include "ps1_input.h"

/* BIOS-padbuffers: 34 bytes per poort (zoals PSn00bSDK/officiele SDK). */
static uint8_t pad_buff[2][34];

void ps1_input_init(void)
{
	/* 0xFF = "geen geldige respons" tot de BIOS-driver de eerste poll heeft
	 * gedaan (stat != 0). Zo lijkt het bij het opstarten niet alsof alle
	 * knoppen ingedrukt zijn. */
	memset(pad_buff, 0xFF, sizeof(pad_buff));

	InitPAD(pad_buff[0], 34, pad_buff[1], 34);
	StartPAD();
	ChangeClearPAD(0);
}

uint16_t ps1_input_buttons(void)
{
	const PADTYPE *pad = (const PADTYPE *) pad_buff[0];

	/* stat == 0 betekent: geldige respons. Anders: geen pad/geen data. */
	if (pad->stat != 0)
		return 0;

	/* De BIOS-driver levert knoppen active-low (0 = ingedrukt). */
	return (uint16_t) ~pad->btn;
}

uint8_t ps1_input_gb_pressed(void)
{
	const uint16_t b = ps1_input_buttons();
	uint8_t gb = 0;

	/* Nintendo-indeling: A = rechter knop (Circle), B = onderste (Cross). */
	if (b & PAD_CIRCLE)  gb |= GBBTN_A;
	if (b & PAD_CROSS)   gb |= GBBTN_B;
	if (b & PAD_SELECT)  gb |= GBBTN_SELECT;
	if (b & PAD_START)   gb |= GBBTN_START;
	if (b & PAD_RIGHT)   gb |= GBBTN_RIGHT;
	if (b & PAD_LEFT)    gb |= GBBTN_LEFT;
	if (b & PAD_UP)      gb |= GBBTN_UP;
	if (b & PAD_DOWN)    gb |= GBBTN_DOWN;

	return gb;
}
