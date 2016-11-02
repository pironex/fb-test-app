/*
 * test.c
 *
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 * Copyright (C) 2009-2012 Tomi Valkeinen

 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include <linux/fb.h>

#include "common.h"

static struct fb_info fb_info;

/*
R7	23	B0	0
R6	22	G0	8
R5	21	R0	16
R4	20	B1	1
R3	19	G1	9
R2	18	R1	17
R1	17	B2	2
R0	16	R2	18
G7	15	B7	7
G6	14	B6	6
G5	13	B5	5
G4	12	B4	4
G3	11	B3	3
G2	10	G7	15
G1	9	G6	14
G0	8	G5	13
B7	7	G4	12
B6	6	G3	11
B5	5	G2	10
B4	4	R7	23
B3	3	R6	22
B2	2	R5	21
B1	1	R4	20
B0	0	R3	19
*/
unsigned char SFFIX[24] = {
	19, 20, 21, 22, 23, 10, 11, 12,
	13, 14, 15,  3,  4,  5,  6,  7,
	18,  2, 17,  9,  1, 16,  8,  0,
};

static inline unsigned fix_color(unsigned old)
{
	unsigned n = 0;
	for (unsigned i = 0; i < 24; ++i) {
		n |= (((old >> i) & 0x0000001) << SFFIX[i]);
	}
	//printf("o %06x n %06x\n", old, n);
	return n;
}

static int SF_FIX = 1;

static void draw_pixel(struct fb_info *fb_info, int x, int y, unsigned color)
{
	void *fbmem;

	if (SF_FIX == 1)
		color = fix_color(color);

	fbmem = fb_info->ptr;
	if (fb_info->var.bits_per_pixel == 8) {
		unsigned char *p;

		fbmem += fb_info->fix.line_length * y;

		p = fbmem;

		p += x;

		*p = color;
	} else if (fb_info->var.bits_per_pixel == 16) {
		unsigned short c;
		unsigned r = (color >> 16) & 0xff;
		unsigned g = (color >> 8) & 0xff;
		unsigned b = (color >> 0) & 0xff;
		unsigned short *p;

		r = r * 32 / 256;
		g = g * 64 / 256;
		b = b * 32 / 256;

		c = (r << 11) | (g << 5) | (b << 0);

		fbmem += fb_info->fix.line_length * y;

		p = fbmem;

		p += x;

		*p = c;
	} else if (fb_info->var.bits_per_pixel == 24) {
		 unsigned char *p;

		p = (unsigned char *)fbmem + fb_info->fix.line_length * y + 3 * x;
		*p++ = color;
		*p++ = color >> 8;
		*p = color >> 16;
	} else {
		unsigned int *p;

		fbmem += fb_info->fix.line_length * y;

		p = fbmem;

		p += x;

		*p = color;
	}
}

static void fill_screen(struct fb_info *fb_info)
{
	unsigned x, y;
	unsigned h = fb_info->var.yres;
	unsigned w = fb_info->var.xres;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (x < 20 && y < 20)
				draw_pixel(fb_info, x, y, 0xffffff);
			else if (x < 20 && (y > 20 && y < h - 20))
				draw_pixel(fb_info, x, y, 0xff);
			else if (y < 20 && (x > 20 && x < w - 20))
				draw_pixel(fb_info, x, y, 0xff00);
			else if (x > w - 20 && (y > 20 && y < h - 20))
				draw_pixel(fb_info, x, y, 0xff0000);
			else if (y > h - 20 && (x > 20 && x < w - 20))
				draw_pixel(fb_info, x, y, 0xffff00);
			else if (x == 20 || x == w - 20 ||
					y == 20 || y == h - 20)
				draw_pixel(fb_info, x, y, 0xffffff);
			else if (x == y || w - x == h - y)
				draw_pixel(fb_info, x, y, 0xff00ff);
			else if (w - x == y || x == h - y)
				draw_pixel(fb_info, x, y, 0x00ffff);
			else if (x > 20 && y > 20 && x < w - 20 && y < h - 20) {
				int t = x * 3 / w;
				unsigned r = 0, g = 0, b = 0;
				unsigned c;
				if (fb_info->var.bits_per_pixel == 16) {
					if (t == 0)
						b = (y % 32) * 256 / 32;
					else if (t == 1)
						g = (y % 64) * 256 / 64;
					else if (t == 2)
						r = (y % 32) * 256 / 32;
				} else {
					if (t == 0)
						b = (y % 256);
					else if (t == 1)
						g = (y % 256);
					else if (t == 2)
						r = (y % 256);
				}
				c = (r << 16) | (g << 8) | (b << 0);
				draw_pixel(fb_info, x, y, c);
			} else {
				draw_pixel(fb_info, x, y, 0);
			}
		}

	}

	fb_put_string(fb_info, w / 3 * 2, 30, "RED", 3, 0xffffff, 1, 3);
	fb_put_string(fb_info, w / 3, 30, "GREEN", 5, 0xffffff, 1, 5);
	fb_put_string(fb_info, 20, 30, "BLUE", 4, 0xffffff, 1, 4);
}

void fill_screen_solid(struct fb_info *fb_info, unsigned int color)
{

	unsigned x, y;
	unsigned h = fb_info->var.yres;
	unsigned w = fb_info->var.xres;

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++)
			draw_pixel(fb_info, x, y, color);
	}
}

static void do_fill_screen(struct fb_info *fb_info, int pattern)
{

	switch (pattern) {
	case 1:
		fill_screen_solid(fb_info, 0xff0000);
		break;
	case 2:
		fill_screen_solid(fb_info, 0x00ff00);
		break;
	case 3:
		fill_screen_solid(fb_info, 0x0000ff);
		break;
	case 4:
		fill_screen_solid(fb_info, 0xffffff);
		break;
	case 0:
	default:
		fill_screen(fb_info);
		break;
	}
}

//#define BOX (150)
static void draw_color_boxes(struct fb_info *fb_info, int add)
{
	unsigned ySize = fb_info->var.yres / 3;
	unsigned xSize = fb_info->var.xres / 8;
	unsigned BOX = (xSize < ySize) ? xSize : ySize;
	unsigned BOXX = xSize;
	unsigned BOXY = ySize;
	unsigned c = 0, cb = 0;
	unsigned tc = (add == 2 ? 0 : 0xffffff );
	unsigned px, py;
	char text[32];
	fill_screen_solid(fb_info, 0);
	for (int rgb = 0; rgb < 3; rgb++) {
		if (add < 2)
			cb = 0;
		for (int s = 0; s < 8; s++) {
			c = (0x00800000 >> ((rgb * 8) + s));
			//c = (0x00000001 << ((rgb * 8) + s));
			cb = (add > 0) ? (cb | c) : c;
			printf("R%d, S%d, x%d, y%d. C%08x\n", rgb, s, BOX * s, BOX * rgb, c);
			for (int y = 0; y < BOXY; y++) {
				py = (rgb * BOXY) + y;
				for (int x = 0; x < BOXX; x++) {
					px = (s * BOXX) + x;
					draw_pixel(fb_info, px, py , cb);
				}
			}
			snprintf(text, 7, "%06x", cb /* (cb >> (8 * (2 -rgb))) */);
			fb_put_string(fb_info, s * BOXX + 10, rgb * BOXY + 20, text, 10, tc, 0, 0);
		}
		snprintf(text, 10, "%s", (rgb == 0 ? "red" : rgb == 1 ? "green" : "blue"));
		fb_put_string(fb_info, 10, rgb * BOXY + 10, text, 10, tc, 0, 0);
	}
}

void show_help(void)
{
	printf("Usage: fb-test -f fbnum -r -g -b -w -p pattern\n");
	printf("Where -f fbnum   = framebuffer device number\n");
	printf("      -r         = fill framebuffer with red\n");
	printf("      -g         = fill framebuffer with green\n");
	printf("      -b         = fill framebuffer with blue\n");
	printf("      -w         = fill framebuffer with white\n");
	printf("      -p pattern = fill framebuffer with pattern number\n");
}

int main(int argc, char **argv)
{
	int opt;
	int req_fb = 0;
	int req_pattern = 0;
	int special_color = 0;

	if (getenv("NO_SF_FIX") != 0) {
		SF_FIX = 0;
	}
	printf("fb-test %d.%d.%d (%s)\n", VERSION, PATCHLEVEL, SUBLEVEL,
		VERSION_NAME);

	while ((opt = getopt(argc, argv, "hrgbwup:s:f:t:")) != -1) {
		switch (opt) {
		case 'f':
			req_fb = atoi(optarg);
			break;
		case 'p':
			req_pattern = atoi(optarg);
			break;
		case 'r':
			req_pattern = 1;
			break;
		case 'g':
			req_pattern = 2;
			break;
		case 'b':
			req_pattern = 3;
			break;
		case 'w':
			req_pattern = 4;
			break;
		case 's':
			special_color = strtoul(optarg, NULL, 0);
			req_pattern = -1;
			break;
		case 't':
			req_pattern = -2;
			special_color = strtoul(optarg, NULL, 0);
			break;
		case 'h':
			show_help();
			return 0;
		default:
			exit(EXIT_FAILURE);
		}
	}

	fb_open(req_fb, &fb_info);
	if (req_pattern == -1) {
		printf("Filling screen with RGB: 0x%08x\n", special_color);
		fill_screen_solid(&fb_info, (unsigned int)special_color);
		//fb_put_string(&fb_info, 100, 100, "FILL", 200, 0xffffff, 0, 0);
	} else if (req_pattern == -2) {
		draw_color_boxes(&fb_info, special_color);
	} else {
		do_fill_screen(&fb_info, req_pattern);
	}

	return 0;
}
