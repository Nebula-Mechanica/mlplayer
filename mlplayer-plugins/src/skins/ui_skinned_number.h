/*
 * MLPlayer - a cross-platform multimedia player
 * Copyright (c) 2007 Tomasz Moń
 * Copyright (c) 2011 John Lindgren
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 * The MLPlayer team does not consider modular code linking to
 * MLPlayer or using our public API to be a derived work.
 */

#ifndef SKINS_UI_SKINNED_NUMBER_H
#define SKINS_UI_SKINNED_NUMBER_H

#include <gtk/gtk.h>

GtkWidget * ui_skinned_number_new ();
void ui_skinned_number_set (GtkWidget * widget, gchar c);
void ui_skinned_number_set_size(GtkWidget *widget, gint width, gint height);

#endif
