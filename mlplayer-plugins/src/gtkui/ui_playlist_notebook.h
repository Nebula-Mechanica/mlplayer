/*  MLPlayer - Cross-platform multimedia player
 *  Copyright (C) 2005-2010  MLPlayer development team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; under version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses>.
 *
 *  The MLPlayer team does not consider modular code linking to
 *  MLPlayer or using our public API to be a derived work.
 */

#ifndef UI_PLAYLIST_NOTEBOOK_H
#define UI_PLAYLIST_NOTEBOOK_H

#include <gtk/gtk.h>

#define UI_PLAYLIST_NOTEBOOK ui_playlist_get_notebook()

GtkNotebook *ui_playlist_get_notebook(void);
GtkWidget *ui_playlist_notebook_new();
void ui_playlist_notebook_create_tab(gint playlist);
void ui_playlist_notebook_edit_tab_title(GtkWidget *ebox);
void ui_playlist_notebook_populate(void);
void ui_playlist_notebook_empty (void);
void ui_playlist_notebook_update (void * data, void * user);
void ui_playlist_notebook_activate (void * data, void * user);
void ui_playlist_notebook_set_playing (void * data, void * user);
void ui_playlist_notebook_position (void * data, void * user);

void playlist_show_headers (gboolean show);

#endif
