#ifndef PS1_INPUT_H
#define PS1_INPUT_H

#include <stdint.h>

/* Zelfde bitwaarden als de JOYPAD_* defines in peanut_gb.h (1 = ingedrukt). */
#define GBBTN_A       0x01
#define GBBTN_B       0x02
#define GBBTN_SELECT  0x04
#define GBBTN_START   0x08
#define GBBTN_RIGHT   0x10
#define GBBTN_LEFT    0x20
#define GBBTN_UP      0x40
#define GBBTN_DOWN    0x80

/* Start de BIOS-paddriver (InitPAD/StartPAD). Na ResetGraph() aanroepen. */
void ps1_input_init(void);

/* PS1-knoppen van pad 1, 1 = ingedrukt (bits uit PadButton in psxpad.h).
 * Geeft 0 als er geen pad aangesloten is of er nog geen data is. */
uint16_t ps1_input_buttons(void);

/* Dezelfde pad vertaald naar Game Boy-knoppen (GBBTN_*, 1 = ingedrukt). */
uint8_t ps1_input_gb_pressed(void);

#endif
