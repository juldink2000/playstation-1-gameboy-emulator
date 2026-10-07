/*
 * Game Boy PS1 - PS1-wrapper rond Peanut-GB.
 *
 * peanut_gb.h wordt hier (en ALLEEN hier) ingevoegd: de header bevat de
 * volledige implementatie, dus in een tweede .c-bestand zou je dubbele
 * symbolen krijgen.
 */
#include <stdint.h>
#include <stdio.h>

#include <psxgpu.h>
#include <psxpad.h>

/* Opties voor Peanut-GB, vóór de include. */
#define ENABLE_SOUND                0  /* geen audio in deze versie       */
#define PEANUT_GB_12_COLOUR         0  /* alleen 4 schaduwen, minder werk */
#define PEANUT_GB_HIGH_LCD_ACCURACY 0  /* sneller; sprites zonder 10-limiet/sortering */
#include "peanut_gb.h"

#include "ps1_video.h"
#include "ps1_input.h"
#include "ps1_rom.h"

#ifndef JOYPAD_START
#define JOYPAD_START 0x08
#endif

/* ---- Games op de CD --------------------------------------------------------
 * Nieuwe game toevoegen: zet het .gb-bestand in roms\ en voer
 * cmake -S . -B build uit. De lijst (naam + pad op de CD) wordt door
 * CMakeLists.txt gemaakt in build/generated/games_gen.h. */
struct game { const char *title; const char *path; };

#include "games_gen.h"   /* maakt CMake van roms/*.gb */
#define GAME_COUNT ((int) (sizeof(games) / sizeof(games[0])))

/* Pokemon Red heeft 32 KiB cart-RAM (battery). Nog niet opgeslagen. */
#define CART_RAM_SIZE (32 * 1024)

static struct gb_s gb;
static uint8_t cart_ram[CART_RAM_SIZE];
static const uint8_t *rom;

/* Index van de game die nu in het RAM staat (en dus kan doorlopen), of -1. */
static int running = -1;

/* ---- Verplichte callbacks voor gb_init() ---------------------------------- */

static uint8_t cb_rom_read(struct gb_s *g, const uint_fast32_t addr)
{
	(void) g;
	return rom[addr];
}

static uint8_t cb_cart_ram_read(struct gb_s *g, const uint_fast32_t addr)
{
	(void) g;
	return (addr < CART_RAM_SIZE) ? cart_ram[addr] : 0xFF;
}

static void cb_cart_ram_write(struct gb_s *g, const uint_fast32_t addr,
			      const uint8_t val)
{
	(void) g;
	if (addr < CART_RAM_SIZE)
		cart_ram[addr] = val;
}

/* ---- Fout- en LCD-callbacks ----------------------------------------------- */

static void fatal(const char *msg)
{
	for (;;) {
		ps1_video_print(msg);
		ps1_video_flip();
	}
}

static void cb_error(struct gb_s *g, const enum gb_error_e err,
		     const uint16_t addr)
{
	static char msg[96];
	(void) g;

	/* Peanut-GB: terugkeren uit gb_error is ongedefinieerd -> hier blijven. */
	snprintf(msg, sizeof(msg), "\n GAME BOY ERROR %d\n AT ADDRESS %04X\n",
		 (int) err, (unsigned) addr);
	fatal(msg);
}

static void cb_lcd_draw_line(struct gb_s *g, const uint8_t *pixels,
			     const uint_fast8_t line)
{
	(void) g;
	ps1_video_put_line(pixels, line);
}

/* ---- Menu ----------------------------------------------------------------- */

/* Geeft de index van de gekozen game terug. Kiest de game die nog draait
 * (PLAYING), dan gaat die door waar hij was; een andere game begint opnieuw.
 * L1 hervat de lopende game meteen. De lijst scrolt als er meer games zijn
 * dan MENU_ROWS. */
#define MENU_ROWS 12

static int game_menu(void)
{
	static char text[768];
	static int  sel = 0, top = 0;
	uint16_t    prev = 0xFFFF;   /* knoppen uit de game niet meteen laten tellen */

	if (running >= 0)
		sel = running;       /* cursor begint op de lopende game */

	for (;;) {
		const uint16_t btn  = ps1_input_buttons();
		const uint16_t edge = btn & (uint16_t) ~prev;
		prev = btn;

		if ((edge & PAD_UP)   && sel > 0)              sel--;
		if ((edge & PAD_DOWN) && sel < GAME_COUNT - 1) sel++;
		if (edge & PAD_RIGHT) ps1_video_set_palette(ps1_video_get_palette() + 1);
		if (edge & PAD_LEFT)  ps1_video_set_palette(ps1_video_get_palette() - 1);
		if ((edge & PAD_L1) && running >= 0)
			return running;
		if (edge & (PAD_START | PAD_CROSS))
			return sel;

		/* Venster van MENU_ROWS games met de cursor erin. */
		if (sel < top)              top = sel;
		if (sel >= top + MENU_ROWS) top = sel - MENU_ROWS + 1;

		int n = snprintf(text, sizeof(text), "\n   GAME BOY PS1\n\n");
		n += snprintf(text + n, sizeof(text) - n, "      %s\n", (top > 0) ? "^" : " ");
		for (int i = top; i < GAME_COUNT && i < top + MENU_ROWS && n < (int) sizeof(text) - 60; i++)
			n += snprintf(text + n, sizeof(text) - n, "   %s %s%s\n",
				      (i == sel) ? ">" : " ", games[i].title,
				      (i == running) ? " *" : "");
		n += snprintf(text + n, sizeof(text) - n, "      %s\n",
			      (top + MENU_ROWS < GAME_COUNT) ? "v" : " ");
		snprintf(text + n, sizeof(text) - n,
			 "\n   < COLOR: %s >\n   UP/DOWN + START%s",
			 ps1_video_palette_name(ps1_video_get_palette()),
			 (running >= 0) ? "\n   * = PLAYING, L1 = RESUME" : "");

		ps1_video_print(text);
		ps1_video_flip();
	}
}

/* ---- Hoofdprogramma ------------------------------------------------------- */

int main(void)
{
	ps1_video_init();
	ps1_input_init();

	int level = 0;   /* snelheidsstand blijft bewaard tussen games */

	for (;;) {
		const int choice = game_menu();

		/* Zelfde game als die al draait: niets laden of resetten, gewoon
		 * verder waar je was. Alleen een andere game start opnieuw. */
		if (choice != running) {
			running = -1;   /* tijdens het laden is er geen geldige game */

			/* Game van de CD in het RAM zetten (pas nu, niet bij het opstarten). */
			ps1_video_print("\n\n        LOADING...");
			ps1_video_flip();

			int err = ps1_rom_load(games[choice].path);
			if (err != PS1_ROM_OK) {
				static char msg[96];
				snprintf(msg, sizeof(msg), "\n ROM LOAD FAILED (%d)\n %s\n",
					 err, games[choice].path);
				fatal(msg);
			}
			rom = ps1_rom_data();

			/* Bescherm onze 1 MiB ROM-buffer: header-byte 0x148 > 5 = groter dan 1 MiB. */
			if (rom[0x0148] > 5)
				fatal("\n ROM TOO BIG FOR\n 1 MIB BUFFER\n");

			/* Schone Game Boy: geen restanten van de vorige game. */
			memset(&gb, 0, sizeof(gb));
			memset(cart_ram, 0, sizeof(cart_ram));

			const enum gb_init_error_e ie = gb_init(&gb, cb_rom_read, cb_cart_ram_read,
								cb_cart_ram_write, cb_error, NULL);
			if (ie != GB_INIT_NO_ERROR) {
				static char msg[64];
				snprintf(msg, sizeof(msg), "\n GB INIT FAILED (%d)\n", (int) ie);
				fatal(msg);
			}

			gb_init_lcd(&gb, cb_lcd_draw_line);

			/* Snelheidspatch: ROM direct uit RAM lezen i.p.v. via callback. */
			gb.rom_direct = rom;

			running = choice;
		}

		/* Snelheidsstanden 0-10 (R2 = hoger/sneller, L2 = lager/mooier):
		 *   0     = niets veranderd, elk frame volledig getekend
		 *   1     = elk 2e frame niet tekenen (frame_skip)
		 *   2     = + interlace (per getekend frame maar de helft van de lijnen)
		 *   3-10  = + beeld maar 1x per 2/3/4 geemuleerde frames tonen
		 * R1 = volledig scherm aan/uit (verbergt ook de tekst)
		 * Vierkant = volgend kleurenpalet (vorig palet kan in het menu met links)
		 * Start of Driehoek = Game Boy-START
		 * L1 = terug naar het gamemenu; de game blijft in het RAM staan en
		 *      gaat door als je dezelfde game weer kiest (of L1 in het menu)
		 */
		static const uint8_t lv_skip[11]  = { 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
		static const uint8_t lv_inter[11] = { 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
		static const uint8_t lv_per[11]   = { 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4 };
		#define MAX_LEVEL 10

		const unsigned hz    = (GetVideoMode() == MODE_PAL) ? 50 : 60;
		const unsigned lines = (GetVideoMode() == MODE_PAL) ? 313 : 263;
		uint16_t prev_btn   = 0xFFFF;   /* knoppen van het menu negeren */
		unsigned frames = 0, emu_sum = 0;
		int      last_vb = VSync(-1);
		char     stats[80];
		stats[0] = '\0';
		int      back = 0;

		while (!back) {
			const uint16_t btn  = ps1_input_buttons();
			const uint16_t edge = btn & (uint16_t) ~prev_btn;
			prev_btn = btn;

			if (edge & PAD_L1) { back = 1; break; }

			if ((edge & PAD_R2) && level < MAX_LEVEL) level++;
			if ((edge & PAD_L2) && level > 0)         level--;
			gb.direct.frame_skip = lv_skip[level];
			gb.direct.interlace  = lv_inter[level];
			const int per = lv_per[level];   /* geemuleerde frames per getoond beeld */
			if (edge & PAD_R1)
				ps1_video_set_fullscreen(!ps1_video_get_fullscreen());
			if (edge & PAD_SQUARE)   ps1_video_set_palette(ps1_video_get_palette() + 1);

			/* VSync(1) = hblanks sinds de laatste VSync-wachtactie (16 bit teller). */
			const uint16_t t0 = (uint16_t) VSync(1);
			for (int i = 0; i < per; i++) {
				/* Peanut-GB: joypad-bit 0 = ingedrukt, dus inverteren.
				 * Elk geemuleerd frame opnieuw lezen, zodat de besturing
				 * ook bij hogere standen direct reageert. */
				uint8_t pressed = (uint8_t) ps1_input_gb_pressed();
				/* Start en Driehoek geven allebei Game Boy-START. */
				if (ps1_input_buttons() & (PAD_START | PAD_TRIANGLE))
					pressed |= JOYPAD_START;
				gb.direct.joypad = (uint8_t) ~pressed;
				gb_run_frame(&gb);
			}
			const uint16_t t1 = (uint16_t) VSync(1);

			frames += per;
			emu_sum += (uint16_t) (t1 - t0);

			const int vb = VSync(-1);
			if ((unsigned) (vb - last_vb) >= hz) {
				const unsigned dv  = (unsigned) (vb - last_vb);
				const unsigned fps = frames * hz / dv;           /* geëmuleerde frames/s */
				const unsigned ms  = (emu_sum / frames) * 1000 / (hz * lines);

				snprintf(stats, sizeof(stats),
					 " SPEED %d%% FPS %d\n EMU %d MS\n LEVEL:%d I:%d",
					 (int) (fps * 100 / 60), (int) fps, (int) ms,
					 level, (int) gb.direct.interlace);

				frames = 0;
				emu_sum = 0;
				last_vb = vb;
			}

			ps1_video_blit_gb();
			if (!ps1_video_get_fullscreen())
				ps1_video_print(stats);
			ps1_video_flip();
		}
	}

	return 0;
}
