#ifndef _TESTUTILS_H
#define _TESTUTILS_H
/*
 *  tslib/tests/testutils.h
 *
 *  Copyright (C) 2004 Michael Opdenacker <michaelo@handhelds.org>
 *
 * This file is placed under the GPL.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 *
 * Misc utils for ts test programs
 */

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define BLUE "\033[34m"
#define YELLOW "\033[33m"

struct ts_button
{
	int x, y, w, h;
	int fill_colidx[2], border_colidx[2], font_colidx[2];
	char *text;
	int flags;
#define BUTTON_ACTIVE 0x00000001
};

void button_draw(struct ts_button *button);
int button_handle(struct ts_button *button, int x, int y, unsigned int pressure);
void getxy(struct tsdev *ts, int *x, int *y);
void getxy_validate(struct tsdev *ts, int *x, int *y);
void ts_flush(struct tsdev *ts);
void print_version(void);

struct ts_textbox
{
	int x, y, w, h;
	int fill_colidx, border_colidx, font_colidx;
	int text_cap;
	char text[5];
};
void textbox_draw(struct ts_textbox *textbox);
void textbox_addchar(struct ts_textbox *textbox, char c);
void textbox_delchar(struct ts_textbox *textbox);
void textbox_clear(struct ts_textbox *textbox);

#endif /* _TESTUTILS_H */
