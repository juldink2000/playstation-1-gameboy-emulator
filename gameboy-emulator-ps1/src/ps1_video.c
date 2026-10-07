#include <stdint.h>
#include <psxgpu.h>

#include "ps1_video.h"

#define SCREEN_W 320
#define SCREEN_H 240

/* Game Boy-beeld 1:1 gecentreerd in 320x240. */
#define GB_X ((SCREEN_W - GB_W) / 2)
#define GB_Y ((SCREEN_H - GB_H) / 2)

/* Volledig scherm met behoud van de beeldverhouding, 1,5x vergroot
 * (240x216). De bovenste/onderste regels van het 240-regelige scherm vallen
 * bij overscan (emulator-crop, echte tv) buiten beeld, dus we houden 12
 * regels marge. Een andere grootte kan met een andere factor in FS_W en
 * FS_H (bijv. 5/3 = 267x240, volle hoogte, kan bovenaan afgeknipt worden). */
#define FS_W (GB_W * 3 / 2)
#define FS_H (GB_H * 3 / 2)
#define FS_X ((SCREEN_W - FS_W) / 2)
#define FS_Y ((SCREEN_H - FS_H) / 2)

/* PS1 15-bit kleur: bit 0-4 = R, 5-9 = G, 10-14 = B (5 bits per kanaal). */
#define RGB15(r, g, b) ((uint16_t) ((r) | ((g) << 5) | ((b) << 10)))

/* 24-bit hex (0xRRGGBB) -> PS1 15-bit. */
#define HEXC(h) RGB15(((h) >> 19) & 31, ((h) >> 11) & 31, ((h) >> 3) & 31)

/* Een palet: naam + 4 kleuren. Peanut-GB schaduw 0 = lichtst ... 3 = donkerst. */
struct pal { const char *name; uint16_t c[4]; };

static const struct pal palettes[] = {
	/* Zwart-wit: dezelfde kleuren als de eerste versie (31/20/10/0). */
	{ "BLACK WHITE",  { RGB15(31, 31, 31), RGB15(20, 20, 20), RGB15(10, 10, 10), RGB15(0, 0, 0) } },
	{ "GREEN",        { HEXC(0x9BBC0F), HEXC(0x8BAC0F), HEXC(0x306230), HEXC(0x0F380F) } },
	{ "GREEN BGB",    { HEXC(0xE0F8D0), HEXC(0x88C070), HEXC(0x346856), HEXC(0x081820) } },
	{ "POCKET GRAY",  { HEXC(0xE8E8E0), HEXC(0xA8A8A0), HEXC(0x585850), HEXC(0x181810) } },
	{ "RED",          { HEXC(0xFFE8E0), HEXC(0xE87868), HEXC(0x902828), HEXC(0x280808) } },
	{ "BLUE",         { HEXC(0xE0F0FF), HEXC(0x78A8E8), HEXC(0x284890), HEXC(0x081028) } },
	{ "AMBER",        { HEXC(0xFFF0C0), HEXC(0xE8B040), HEXC(0x906010), HEXC(0x281800) } },
	{ "CYAN",         { HEXC(0xD8FFF8), HEXC(0x70D0C8), HEXC(0x287870), HEXC(0x082828) } },
	{ "SEPIA",        { HEXC(0xF8ECD0), HEXC(0xC8A878), HEXC(0x786040), HEXC(0x201808) } },
	/* Extra paletten uit tools/make_palettes.js (optioneel bestand). */
#if defined(__has_include)
#if __has_include("gb_palettes_extra.h")
#include "gb_palettes_extra.h"
#endif
#endif
};
#define PALETTE_COUNT ((int) (sizeof(palettes) / sizeof(palettes[0])))

static int cur_palette;

/* 160*144*2 = 46080 bytes in main RAM; LoadImage wil 32-bit uitlijning. */
static uint16_t gb_fb[GB_W * GB_H] __attribute__((aligned(4)));

/* Twee pixels tegelijk: index = (pix0 & 3) | ((pix1 & 3) << 2) -> 2 pixels
 * van 16 bit in één 32-bit woord. Halveert het aantal loads/stores per lijn. */
static uint32_t pal2[16];

/* Vblank-teller bij de laatste flip, om niet nodeloos te wachten als we te laat zijn. */
static int last_flip_vb;

/* Volledig scherm: het GB-beeld gaat als 15-bit texture naar dit VRAM-gebied
 * (onder de twee framebuffers, die lopen tot y = 240) en wordt als 1 polygoon
 * met de juiste beeldverhouding in het midden van het scherm getekend. */
#define FS_TEX_X 0
#define FS_TEX_Y 256

static int fullscreen;
static uint32_t fs_ot[2];
static POLY_FT4 fs_poly;

static DISPENV disp[2];
static DRAWENV draw[2];
static int active;

void ps1_video_init(void)
{
	ResetGraph(0);

	/* Zelfde dubbelbuffer-patroon als de PSn00bSDK-voorbeelden:
	 * draw[n] tekent in de buffer die disp[n ^ 1] laat zien. */
	SetDefDispEnv(&disp[0], 0,        0, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&draw[0], SCREEN_W, 0, SCREEN_W, SCREEN_H);
	SetDefDispEnv(&disp[1], SCREEN_W, 0, SCREEN_W, SCREEN_H);
	SetDefDrawEnv(&draw[1], 0,        0, SCREEN_W, SCREEN_H);

	for (int i = 0; i < 2; i++) {
		draw[i].isbg = 1;          /* achterbuffer elk frame zwart wissen */
		draw[i].r0 = 0;
		draw[i].g0 = 0;
		draw[i].b0 = 0;
	}

	ps1_video_set_palette(0);

	active = 0;
	PutDrawEnv(&draw[0]);

	/* Debugfont-texture in VRAM rechts van de twee framebuffers. */
	FntLoad(960, 0);
	FntOpen(16, 16, 288, 208, 0, 512);
}

int ps1_video_palette_count(void)
{
	return PALETTE_COUNT;
}

const char *ps1_video_palette_name(int index)
{
	return palettes[((index % PALETTE_COUNT) + PALETTE_COUNT) % PALETTE_COUNT].name;
}

int ps1_video_get_palette(void)
{
	return cur_palette;
}

void ps1_video_set_palette(int index)
{
	cur_palette = ((index % PALETTE_COUNT) + PALETTE_COUNT) % PALETTE_COUNT;
	const uint16_t *c = palettes[cur_palette].c;

	for (int i = 0; i < 16; i++)
		pal2[i] = (uint32_t) c[i & 3] |
			  ((uint32_t) c[(i >> 2) & 3] << 16);
}

void ps1_video_put_line(const uint8_t *pixels, unsigned line)
{
	if (line >= GB_H)
		return;

	uint32_t *dst = (uint32_t *) &gb_fb[line * GB_W];
	for (int x = 0; x < GB_W / 2; x++) {
		dst[x] = pal2[(pixels[0] & 3) | ((pixels[1] & 3) << 2)];
		pixels += 2;
	}
}

void ps1_video_set_fullscreen(int on)
{
	fullscreen = on ? 1 : 0;
}

int ps1_video_get_fullscreen(void)
{
	return fullscreen;
}

void ps1_video_blit_gb(void)
{
	RECT r;

	if (!fullscreen) {
		setRECT(&r, draw[active].clip.x + GB_X, draw[active].clip.y + GB_Y,
			GB_W, GB_H);
		LoadImage(&r, (const uint32_t *) gb_fb);
		return;
	}

	/* Beeld als texture naar VRAM, daarna 1 polygoon over de volle hoogte.
	 * De GPU schaalt zonder filter: grotere, scherpe pixels. De breedte
	 * volgt uit de hoogte, zodat de Game Boy-beeldverhouding (10:9) klopt;
	 * links en rechts blijft zwart (de achterbuffer wordt elk frame gewist). */
	setRECT(&r, FS_TEX_X, FS_TEX_Y, GB_W, GB_H);
	LoadImage(&r, (const uint32_t *) gb_fb);
	DrawSync(0);

	setPolyFT4(&fs_poly);
	setRGB0(&fs_poly, 128, 128, 128);
	setXY4(&fs_poly,
	       FS_X,        FS_Y,
	       FS_X + FS_W, FS_Y,
	       FS_X,        FS_Y + FS_H,
	       FS_X + FS_W, FS_Y + FS_H);
	/* Texturecoordinaten t/m GB_W / GB_H (net buiten het beeld): de GPU
	 * pakt per pixel de linker/bovenste texel, dus geen rand-artefacten. */
	setUV4(&fs_poly,
	       0,    0,
	       GB_W, 0,
	       0,    GB_H,
	       GB_W, GB_H);
	fs_poly.tpage = getTPage(2, 0, FS_TEX_X, FS_TEX_Y);   /* 2 = 15-bit kleur */
	fs_poly.clut  = 0;

	ClearOTagR(fs_ot, 2);
	addPrim(fs_ot + 1, &fs_poly);
	DrawOTag(fs_ot + 1);
}

void ps1_video_print(const char *text)
{
	FntPrint(-1, "%s", text);
	FntFlush(-1);
}

void ps1_video_flip(void)
{
	DrawSync(0);

	/* Alleen op de vblank wachten als er sinds de vorige flip nog geen
	 * vblank was (we waren sneller dan 60 Hz). Zijn we te laat, dan direct
	 * wisselen i.p.v. nog tot de volgende vblank te blijven hangen: bij
	 * trage emulatie scheelt dat gemiddeld een halve vblank per frame, ten
	 * koste van wat tearing. */
	if (VSync(-1) == last_flip_vb)
		VSync(0);
	last_flip_vb = VSync(-1);

	active ^= 1;
	PutDrawEnv(&draw[active]);
	PutDispEnv(&disp[active]);
	SetDispMask(1);
}
