/*
 * E-UAE - The portable Amiga Emulator
 *
 * Generate pixel output code.
 *
 * (c) 2006 Richard Drummond
 */

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

/* Output for big-endian target if true, little-endian is false. */
int do_bigendian;

typedef int DEPTH_T;

#define DEPTH_8BPP 0
#define DEPTH_16BPP 1
#define DEPTH_32BPP 2
#define DEPTH_MAX DEPTH_32BPP

static const char *get_depth_str (DEPTH_T bpp)
{
	if (bpp == DEPTH_8BPP)
		return "8";
	else if (bpp == DEPTH_16BPP)
		return "16";
	else
		return "32";
}

static const char *get_depth_type_str (DEPTH_T bpp)
{
	if (bpp == DEPTH_8BPP)
		return "uae_u8";
	else if (bpp == DEPTH_16BPP)
		return "uae_u16";
	else
		return "uae_u32";
}

typedef int HMODE_T;

#define HMODE_NORMAL	0
#define HMODE_DOUBLE	1
#define HMODE_DOUBLE2X	2
#define HMODE_HALVE1	3
#define HMODE_HALVE1F	4
#define HMODE_HALVE2	5
#define HMODE_HALVE2F	6
#define HMODE_MAX HMODE_HALVE2F

static const char *get_hmode_str (HMODE_T hmode)
{
	if (hmode == HMODE_DOUBLE)
		return "_stretch1";
	else if (hmode == HMODE_DOUBLE2X)
		return "_stretch2";
	else if (hmode == HMODE_HALVE1)
		return "_shrink1";
	else if (hmode == HMODE_HALVE1F)
		return "_shrink1f";
	else if (hmode == HMODE_HALVE2)
		return "_shrink2";
	else if (hmode == HMODE_HALVE2F)
		return "_shrink2f";
	else
		return "";
}


typedef enum
{
	CMODE_NORMAL,
	CMODE_DUALPF,
	CMODE_EXTRAHB,
	CMODE_HAM
} CMODE_T;
#define CMODE_MAX CMODE_HAM


static FILE *outfile;
static unsigned int outfile_indent = 0;

void set_outfile (FILE *f)
{
	outfile = f;
}

int set_indent (int indent)
{
	int old_indent = outfile_indent;
	outfile_indent = indent;
	return old_indent;
}

void outln (const char *s)
{
	unsigned int i;
	for (i = 0; i < outfile_indent; i++)
		fputc (' ', outfile);
	fprintf (outfile, "%s\n", s);
}

void outlnf (const char *s, ...)
{
	va_list ap;
	unsigned int i;
	for (i = 0; i < outfile_indent; i++)
		fputc (' ', outfile);
	va_start (ap, s);
	vfprintf (outfile, s, ap);
	fputc ('\n', outfile);
}

static void out_linetoscr_decl (DEPTH_T bpp, HMODE_T hmode, int aga, int spr)
{
	outlnf ("static int NOINLINE linetoscr_%s%s%s%s (int spix, int dpix, int dpix_end)",
		get_depth_str (bpp),
		get_hmode_str (hmode), aga ? "_aga" : "", spr ? "_spr" : "");
}

static void out_linetoscr_do_srcpix (DEPTH_T bpp, HMODE_T hmode, int aga, CMODE_T cmode, int spr)
{
	if (aga && cmode != CMODE_DUALPF) {
		if (spr)
			outln (     "    sprpix_val = pixdata.apixels[spix];");
		outln ( 	"    spix_val = pixdata.apixels[spix] ^ xor_val;");
	} else if (cmode != CMODE_HAM) {
		outln ( 	"    spix_val = pixdata.apixels[spix];");
		if (spr)
			outln (     "    sprpix_val = spix_val;");
	}
}

static void out_linetoscr_do_dstpix (DEPTH_T bpp, HMODE_T hmode, int aga, CMODE_T cmode, int spr)
{
	if (aga && cmode == CMODE_HAM) {
		outln (	    "    dpix_val = CONVERT_RGB (ham_linebuf[spix]);");
		if (spr)
			outln ( "    sprpix_val = dpix_val;");
	} else if (cmode == CMODE_HAM) {
		outln (		"    dpix_val = xcolors[ham_linebuf[spix]];");
		if (spr)
			outln ( "    sprpix_val = dpix_val;");
	} else if (aga && cmode == CMODE_DUALPF) {
		outln (     "    {");
		outln (		"        uae_u8 val = lookup[spix_val];");
		outln (		"        if (lookup_no[spix_val])");
		outln (		"            val += dblpfofs[bpldualpf2of];");
		outln (		"        val ^= xor_val;");
		outln (		"        dpix_val = colors_for_drawing.acolors[val];");
		outln (		"    }");
	} else if (cmode == CMODE_DUALPF) {
		outln (		"    dpix_val = colors_for_drawing.acolors[lookup[spix_val]];");
	} else if (aga && cmode == CMODE_EXTRAHB) {
		outln (		"    if (spix_val >= 32 && spix_val < 64) {");
		outln (		"        unsigned int c = (colors_for_drawing.color_regs_aga[spix_val - 32] >> 1) & 0x7F7F7F;");
		outln (		"        dpix_val = CONVERT_RGB (c);");
		outln (		"    } else");
		outln (		"        dpix_val = colors_for_drawing.acolors[spix_val];");
	} else if (cmode == CMODE_EXTRAHB) {
		outln (		"    if (spix_val <= 31)");
		outln (		"        dpix_val = colors_for_drawing.acolors[spix_val];");
		outln (		"    else");
		outln (		"        dpix_val = xcolors[(colors_for_drawing.color_regs_ecs[spix_val - 32] >> 1) & 0x777];");
	} else
		outln (		"    dpix_val = colors_for_drawing.acolors[spix_val];");
}

static void out_linetoscr_do_incspix (DEPTH_T bpp, HMODE_T hmode, int aga, CMODE_T cmode, int spr)
{
	if (hmode == HMODE_HALVE1F) {
		outln (         "    {");
		outln (         "    uae_u32 tmp_val;");
		outln (		"    spix++;");
		outln (		"    tmp_val = dpix_val;");
		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		outlnf (	"    dpix_val = merge_2pixel%d (dpix_val, tmp_val);", bpp == 0 ? 8 : bpp == 1 ? 16 : 32);
		outln (		"    spix++;");
		outln (         "    }");
	} else if (hmode == HMODE_HALVE2F) {
		outln (         "    {");
		outln (         "    uae_u32 tmp_val, tmp_val2, tmp_val3;");
		outln (		"    spix++;");
		outln (		"    tmp_val = dpix_val;");
		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		outln (		"    spix++;");
		outln (		"    tmp_val2 = dpix_val;");
		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		outln (		"    spix++;");
		outln (		"    tmp_val3 = dpix_val;");
		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		outlnf (        "    tmp_val = merge_2pixel%d (tmp_val, tmp_val2);", bpp == 0 ? 8 : bpp == 1 ? 16 : 32);
		outlnf (        "    tmp_val2 = merge_2pixel%d (tmp_val3, dpix_val);", bpp == 0 ? 8 : bpp == 1 ? 16 : 32);
		outlnf (	"    dpix_val = merge_2pixel%d (tmp_val, tmp_val2);", bpp == 0 ? 8 : bpp == 1 ? 16 : 32);
		outln (		"    spix++;");
		outln (         "    }");
	} else if (hmode == HMODE_HALVE1) {
		outln (		"    spix += 2;");
	} else if (hmode == HMODE_HALVE2) {
		outln (		"    spix += 4;");
	} else {
		outln (		"    spix++;");
	}
}


static void put_dpix (const char *var)
{
	outlnf ("    buf[dpix++] = %s;", var);
}

static void out_sprite (DEPTH_T bpp, HMODE_T hmode, CMODE_T cmode, int aga, int cnt)
{
	if (aga) {
		if (cnt == 1) {
			outlnf ( "    if (spritepixels[dpix].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 0, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			put_dpix ("out_val");
		} else if (cnt == 2) {
			outlnf ( "    {");
			outlnf ( "    uae_u32 out_val1 = out_val;");
			outlnf ( "    uae_u32 out_val2 = out_val;");
			outlnf ( "    if (spritepixels[dpix + 0].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 0, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val1 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			outlnf ( "    if (spritepixels[dpix + 1].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 1, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val2 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			put_dpix ("out_val1");
			put_dpix ("out_val2");
			outlnf ( "    }");
		} else if (cnt == 4) {
			outlnf ( "    {");
			outlnf ( "    uae_u32 out_val1 = out_val;");
			outlnf ( "    uae_u32 out_val2 = out_val;");
			outlnf ( "    uae_u32 out_val3 = out_val;");
			outlnf ( "    uae_u32 out_val4 = out_val;");
			outlnf ( "    if (spritepixels[dpix + 0].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 0, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val1 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			outlnf ( "    if (spritepixels[dpix + 1].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 1, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val2 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			outlnf ( "    if (spritepixels[dpix + 2].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 2, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val3 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			outlnf ( "    if (spritepixels[dpix + 3].data) {");
			outlnf ( "        sprcol = render_sprites (dpix + 3, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
			outlnf ( "        if (sprcol)");
			outlnf ( "            out_val4 = colors_for_drawing.acolors[sprcol];");
			outlnf ( "    }");
			put_dpix ("out_val1");
			put_dpix ("out_val2");
			put_dpix ("out_val3");
			put_dpix ("out_val4");
			outlnf ( "    }");
		}
	} else {
		outlnf ( "    if (spritepixels[dpix].data) {");
		outlnf ( "        sprcol = render_sprites (dpix, %d, sprpix_val, %d);", cmode == CMODE_DUALPF ? 1 : 0, aga);
		outlnf ( "        if (sprcol) {");
		outlnf ( "            uae_u32 spcol = colors_for_drawing.acolors[sprcol];");
		outlnf ( "            out_val = spcol;");
		outlnf ( "        }");
		outlnf ( "    }");
		while (cnt-- > 0)
			put_dpix ("out_val");
	}
}


static void out_linetoscr_mode (DEPTH_T bpp, HMODE_T hmode, int aga, int spr, CMODE_T cmode)
{
	int old_indent = set_indent (8);

	if (aga && cmode == CMODE_DUALPF) {
		outln (        "int *lookup    = bpldualpfpri ? dblpf_ind2_aga : dblpf_ind1_aga;");
		outln (        "int *lookup_no = bpldualpfpri ? dblpf_2nd2     : dblpf_2nd1;");
	} else if (cmode == CMODE_DUALPF)
		outln (        "int *lookup = bpldualpfpri ? dblpf_ind2 : dblpf_ind1;");


	/* TODO: add support for combining pixel writes in 8-bpp modes. */

	if (bpp == DEPTH_16BPP && hmode != HMODE_DOUBLE && hmode != HMODE_DOUBLE2X && spr == 0) {
		outln (		"int rem;");
		outln (		"if (((long)&buf[dpix]) & 2) {");
		outln (		"    uae_u32 spix_val;");
		outln (		"    uae_u32 dpix_val;");

		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_incspix (bpp, hmode, aga, cmode, spr);

		put_dpix ("dpix_val");
		outln (		"}");
		outln (		"if (dpix >= dpix_end)");
		outln (		"    return spix;");
		outln (		"rem = (((long)&buf[dpix_end]) & 2);");
		outln (		"if (rem)");
		outln (		"    dpix_end--;");
	}


	outln (		"while (dpix < dpix_end) {");
	if (spr)
		outln (		"    uae_u32 sprpix_val;");
	outln (		"    uae_u32 spix_val;");
	outln (		"    uae_u32 dpix_val;");
	outln (		"    uae_u32 out_val;");
	outln (		"");

	out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
	out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
	out_linetoscr_do_incspix (bpp, hmode, aga, cmode, spr);

	outln (		"    out_val = dpix_val;");

	if (hmode != HMODE_DOUBLE && hmode != HMODE_DOUBLE2X && bpp == DEPTH_16BPP && spr == 0) {
		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_incspix (bpp, hmode, aga, cmode, spr);

		if (do_bigendian)
			outln (	"    out_val = (out_val << 16) | (dpix_val & 0xFFFF);");
		else
			outln (	"    out_val = (out_val & 0xFFFF) | (dpix_val << 16);");
	}

	if (hmode == HMODE_DOUBLE) {
		if (bpp == DEPTH_8BPP) {
			outln (	"    *((uae_u16 *)&buf[dpix]) = (uae_u16) out_val;");
			outln (	"    dpix += 2;");
		} else if (bpp == DEPTH_16BPP) {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 2);
			} else {
				outln (	"    *((uae_u32 *)&buf[dpix]) = out_val;");
				outln (	"    dpix += 2;");
			}
		} else {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 2);
			} else {
				put_dpix ("out_val");
				put_dpix ("out_val");
			}
		}
	} else if (hmode == HMODE_DOUBLE2X) {
		if (bpp == DEPTH_8BPP) {
			outln (	"    *((uae_u32 *)&buf[dpix]) = (uae_u32) out_val;");
			outln (	"    dpix += 4;");
		} else if (bpp == DEPTH_16BPP) {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 4);
			} else {
				outln (	"    *((uae_u32 *)&buf[dpix]) = out_val;");
				outln (	"    dpix += 2;");
				outln (	"    *((uae_u32 *)&buf[dpix]) = out_val;");
				outln (	"    dpix += 2;");
			}
		} else {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 4);
			} else {
				put_dpix ("out_val");
				put_dpix ("out_val");
				put_dpix ("out_val");
				put_dpix ("out_val");
			}
		}
	} else {
		if (bpp == DEPTH_16BPP) {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 1);
			} else {
				outln (	"    *((uae_u32 *)&buf[dpix]) = out_val;");
				outln (	"    dpix += 2;");
			}
		} else {
			if (spr) {
				out_sprite (bpp, hmode, cmode, aga, 1);
			} else {
				put_dpix ("out_val");
			}
		}
	}

	outln (		"}");


	if (bpp == DEPTH_16BPP && hmode != HMODE_DOUBLE && hmode != HMODE_DOUBLE2X && spr == 0) {
		outln (		"if (rem) {");
		outln (		"    uae_u32 spix_val;");
		outln (		"    uae_u32 dpix_val;");

		out_linetoscr_do_srcpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_dstpix (bpp, hmode, aga, cmode, spr);
		out_linetoscr_do_incspix (bpp, hmode, aga, cmode, spr);

		put_dpix ("dpix_val");
		outln (		"}");
	}

	set_indent (old_indent);

	return;
}

static void out_linetoscr (DEPTH_T bpp, HMODE_T hmode, int aga, int spr)
{
	if (aga)
		outln  ("#ifdef AGA");

	out_linetoscr_decl (bpp, hmode, aga, spr);
	outln  (	"{");

	outlnf (	"    %s *buf = (%s *) xlinebuffer;", get_depth_type_str (bpp), get_depth_type_str (bpp));
	if (spr)
		outln ( "    uae_u8 sprcol;");
	if (aga)
		outln (	"    uae_u8 xor_val = bplxor;");
	outln  (	"");

	outln  (	"    if (dp_for_drawing->ham_seen) {");
	out_linetoscr_mode (bpp, hmode, aga, spr, CMODE_HAM);
	outln  (	"    } else if (bpldualpf) {");
	out_linetoscr_mode (bpp, hmode, aga, spr, CMODE_DUALPF);
	outln  (	"    } else if (bplehb) {");
	out_linetoscr_mode (bpp, hmode, aga, spr, CMODE_EXTRAHB);
	outln  (	"    } else {");
	out_linetoscr_mode (bpp, hmode, aga, spr, CMODE_NORMAL);

	outln  (	"    }\n");
	outln  (	"    return spix;");
	outln  (	"}");

	if (aga)
		outln (	"#endif");
	outln  (	"");
}

int main (int argc, char *argv[])
{
	DEPTH_T bpp;
	int aga, spr;
	HMODE_T hmode;
	unsigned int i;

	do_bigendian = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-')
			continue;
		if (argv[i][1] == 'b' && argv[i][2] == '\0')
			do_bigendian = 1;
	}

	set_outfile (stdout);

	outln ("/*");
	outln (" * E-UAE - The portable Amiga emulator.");
	outln (" *");
	outln (" * This file was generated by genlinetoscr. Don't edit.");
	outln (" */");
	outln ("");

	for (bpp = DEPTH_16BPP; bpp <= DEPTH_MAX; bpp++) {
		for (aga = 0; aga <= 1 ; aga++) {
			if (aga && bpp == DEPTH_8BPP)
				continue;
			for (spr = 0; spr <= 1; spr++) {
				for (hmode = HMODE_NORMAL; hmode <= HMODE_MAX; hmode++)
					out_linetoscr (bpp, hmode, aga, spr);
			}
		}
	}
	return 0;
}
