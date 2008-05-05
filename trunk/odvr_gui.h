/* olympus: Olympus DVR GUI
 *
 * Copyright (C) 2008 Conor McLoughlin <conor dot home at gmail dot com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef ODVR_GUI_H
#define ODVR_GUI_H

#include <stdint.h>
#include <gtk/gtk.h>
#include "olympusdvr.h"
#include "odvr_date.h"
#include "odvr_cfg.h"

typedef struct odvrData {
    odvr       dev;
    GtkWidget *window;
    GdkPixbuf *folderIcon[ODVR_NUM_FOLDERS];
    GtkWidget *menu_bar ;
    GtkWidget *buttonBox;
    GtkWidget *destinationDirBox;
    GtkWidget *ddDialog;
    GtkWidget *ddButton;
    GtkWidget *overallProgressBar;
    GtkWidget *fileProgressBar;
    GtkTreeSelection *selection;
    GtkWidget *view;
    /* 
     * I keep a pointer to the D and S buttons and menus so that I hide or reveal them
     * depending on the model of the device
     */
    GtkWidget *buttonD, *buttonS;
    GtkWidget *transferMenuD, *transferMenuS;
    GtkWidget *clearMenuD, *clearMenuS;
    GtkTreeStore *file_store;
    gint quality_used; /* Bitmap */
    odvr_config_t *cfg;
} odvrData_t;

enum {
    COL_FOLDER = 0,
    COL_FOLDER_ID,
    COL_SLOT,
    COL_ID,
    COL_SIZE,
    COL_LENGTH,
    COL_DATE,
    COL_QUALITY,
    NUM_COLS
};

#define ODVR_RESPONSE_YES_ALL 1
#define ODVR_RESPONSE_NO_ALL  2

void gui_err(const char *errorString, const char *errorDetail);

gint yes_no_acknowledge(gchar *question);

gint acknowledge_ok_cancel(gchar *question);

gint acknowledge(gchar *question);

/* Exit the program without confirmation or saving configuration */
gboolean delete_event( GtkWidget *widget);
gboolean quit_event( GtkWidget *widget, gpointer data);

GtkWidget *create_view (odvrData_t *odvrData);

#endif  /* ODVR_GUI_H */
