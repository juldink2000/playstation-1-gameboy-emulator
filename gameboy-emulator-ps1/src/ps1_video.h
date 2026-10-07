#ifndef PS1_VIDEO_H
#define PS1_VIDEO_H

#include <stdint.h>

#define GB_W 160
#define GB_H 144

/* ResetGraph + dubbele framebuffer (320x240, 15-bit) + debugfont. */
void ps1_video_init(void);

/* Schrijf een Game Boy-scanline (160 schaduwwaarden, alleen bits 0-1 tellen)
 * in de RAM-framebuffer. Aangeroepen vanuit de Peanut-GB lcd_draw_line. */
void ps1_video_put_line(const uint8_t *pixels, unsigned line);

/* Kopieer de RAM-framebuffer (160x144) naar het midden van de achterbuffer. */
void ps1_video_blit_gb(void);

/* Tekst (debugfont) in de achterbuffer. Meerdere regels mogen met '\n'. */
void ps1_video_print(const char *text);

/* Wacht op VSync en wissel voor-/achterbuffer. */
void ps1_video_flip(void);

/* Volledig scherm: het 160x144-beeld wordt over het hele 320x240-scherm
 * uitgerekt (GPU-texture, geen extra CPU-werk). */
void ps1_video_set_fullscreen(int on);
int  ps1_video_get_fullscreen(void);

/* Kleurenpaletten voor het Game Boy-beeld (4 kleuren, licht -> donker). */
int         ps1_video_palette_count(void);
const char *ps1_video_palette_name(int index);
int         ps1_video_get_palette(void);
void        ps1_video_set_palette(int index);   /* index wordt netjes rondgedraaid */

#endif
