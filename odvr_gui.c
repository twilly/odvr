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
#include <stdint.h>
#include <gtk/gtk.h>
#include <string.h>
#include "odvr_date.h"
#include "odvr_cfg.h"

#include "odvr_gui.h"

void gui_err(const char *errorString, const char *errorDetail)
{
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new(NULL,
				    GTK_DIALOG_DESTROY_WITH_PARENT,
				    GTK_MESSAGE_ERROR,
				    GTK_BUTTONS_OK,
				    "%s\n%s", errorString, errorDetail);
    
    gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);
	    
}

gint yes_no_acknowledge(gchar *question)
{
  GtkWidget *messageDialog;
  gint response;

  messageDialog = gtk_message_dialog_new(
					 NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 question);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 GTK_STOCK_YES, GTK_RESPONSE_YES,
			 NULL);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 GTK_STOCK_NO, GTK_RESPONSE_NO,
			 NULL);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 "Yes to all", ODVR_RESPONSE_YES_ALL,
			 NULL);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 "No to all", ODVR_RESPONSE_NO_ALL,
			 NULL);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 NULL);
   				 
  response = gtk_dialog_run (GTK_DIALOG (messageDialog));
  gtk_widget_destroy (messageDialog);

  return response;
}

gint acknowledge_ok_cancel(gchar *question)
{
  GtkWidget *messageDialog;
  gint response;

  messageDialog = gtk_message_dialog_new(
					 NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 question);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 GTK_STOCK_OK, GTK_RESPONSE_OK,
			 NULL);
  gtk_dialog_add_buttons(GTK_DIALOG(messageDialog), 
			 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			 NULL);
   				 
  response = gtk_dialog_run (GTK_DIALOG (messageDialog));
  gtk_widget_destroy (messageDialog);

  return response;
}

gint acknowledge(gchar *question)
{
  GtkWidget *messageDialog;
  gint response;

  messageDialog = gtk_message_dialog_new(
					 NULL,
					 GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_OK,
					 question);
   				 
  response = gtk_dialog_run (GTK_DIALOG (messageDialog));
  gtk_widget_destroy (messageDialog);

  return response;
}

/* Exit the program without confirmation or saving configuration */
gboolean delete_event( GtkWidget *widget)
{
  gtk_main_quit ();

  return FALSE;
}

/* Exit the program without after saving configuration */
gboolean quit_event( GtkWidget *widget, gpointer   data )
{
    odvrData_t *odvrData = (odvrData_t *)(data);
    odvr_config_t *cfg = odvrData->cfg;
    char *destinationDir;
    gint width, height;

    destinationDir = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(odvrData->ddButton));
    if (strcmp(odvr_cfg_get_destination_dir(cfg), destinationDir))
	odvr_cfg_set_destination_dir(cfg, destinationDir);

    gtk_window_get_size(GTK_WINDOW(odvrData->window), &width, &height);
    odvr_cfg_set_window_size(odvrData->cfg, width, height);


    if (odvr_cfg_get_dirty(cfg))
	odvr_cfg_save_config(cfg);

    gtk_main_quit ();

    return FALSE;
}

static void length_func (GtkTreeViewColumn *col,
		  GtkCellRenderer   *renderer,
		  GtkTreeModel      *model,
		  GtkTreeIter       *iter,
		  gpointer           user_data)
{
    float length;
    gchar  buf[64];
    int sec, min, hour;

    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	g_object_set(renderer, "text", NULL, NULL);
	return;
    }

    gtk_tree_model_get(model, iter, COL_LENGTH, &length, -1);
    sec = length + 0.5;
    hour = sec / 3600;
    sec  = sec % 3600;
    min  = sec / 60;
    sec  = sec % 60;
    g_snprintf(buf, sizeof(buf), "%02d:%02d:%02d", hour, min, sec);
    g_object_set(renderer, "text", buf, NULL);
}

static void folder_func (GtkTreeViewColumn *col,
	     GtkCellRenderer   *renderer,
	     GtkTreeModel      *model,
	     GtkTreeIter       *iter,
	     gpointer           user_data)
{
    GdkPixbuf *folderIcon;
    GtkTreePath *path;
    GtkTreeIter parent;

    path = gtk_tree_model_get_path(model, iter);

    gtk_tree_model_get(model, iter, COL_FOLDER, &folderIcon, -1);


    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	    g_object_set(renderer, "pixbuf", folderIcon, NULL);
    }
    else
	g_object_set(renderer, "pixbuf", NULL, NULL);


    gtk_tree_path_free(path);

    return;
}

static void slot_func (GtkTreeViewColumn *col,
	   GtkCellRenderer   *renderer,
	   GtkTreeModel      *model,
	   GtkTreeIter       *iter,
	   gpointer           user_data)
{
    int slot;
    gchar  buf[64];
    GtkTreeIter parent;

    gtk_tree_model_get(model, iter, COL_SLOT, &slot, -1);

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	g_object_set(renderer, "text", NULL, NULL);
	return;
    }

    g_snprintf(buf, sizeof(buf), "%d", slot);
    g_object_set(renderer, "text", buf, NULL);
}

static void file_func (GtkTreeViewColumn *col,
	   GtkCellRenderer   *renderer,
	   GtkTreeModel      *model,
	   GtkTreeIter       *iter,
	   gpointer           user_data)
{
    int id;
    gchar  buf[64];
    GtkTreeIter parent;

    gtk_tree_model_get(model, iter, COL_ID, &id, -1);

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	g_snprintf(buf, sizeof(buf),"(%d)",
		   gtk_tree_model_iter_n_children(model, iter));
	g_object_set(renderer, "text", buf, NULL);
	return;
    }

    g_snprintf(buf, sizeof(buf), "%d", id);
    g_object_set(renderer, "text", buf, NULL);
}

static void date_func (GtkTreeViewColumn *col,
		  GtkCellRenderer   *renderer,
		  GtkTreeModel      *model,
		  GtkTreeIter       *iter,
		  gpointer           user_data)
{
    odvrDate_t *fileDate;
    gchar *buf;
    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	g_object_set(renderer, "text", NULL, NULL);
	return;
    }

    gtk_tree_model_get(model, iter, COL_DATE, &fileDate, -1);

    buf = dateString(fileDate);
    g_object_set(renderer, "text",buf, NULL);
}

static void quality_func (GtkTreeViewColumn *col,
		  GtkCellRenderer   *renderer,
		  GtkTreeModel      *model,
		  GtkTreeIter       *iter,
		  gpointer           user_data)
{
    int quality;
    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	g_object_set(renderer, "text", NULL, NULL);
	return;
    }
    gtk_tree_model_get(model, iter, COL_QUALITY, &quality, -1);
    g_object_set(renderer, "text", odvr_quality_name(quality), NULL);
}


GtkWidget *create_view (odvrData_t *odvrData)
{
  GtkTreeViewColumn   *col;
  GtkCellRenderer     *renderer;
  GtkWidget           *view;

  view = gtk_tree_view_new();

  /* Folder icon */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable(col, TRUE);
  gtk_tree_view_column_set_reorderable(col, FALSE);
  gtk_tree_view_column_set_title(col, "Folder/Slot");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, folder_func, 
					  NULL, NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, slot_func, 
					  NULL, NULL);

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable(col, TRUE);
  gtk_tree_view_column_set_reorderable(col, FALSE);
  gtk_tree_view_column_set_title(col, "File");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, file_func, 
					  NULL, NULL);

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable (col, TRUE);
  gtk_tree_view_column_set_reorderable(col, TRUE);
  gtk_tree_view_column_set_title(col, "Length");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, length_func, 
					  NULL, NULL);

  if (odvr_cfg_get_show_wav_size(odvrData->cfg)) {
      /* Display wav file size */
      col = gtk_tree_view_column_new();
      gtk_tree_view_column_set_resizable (col, TRUE);
      gtk_tree_view_column_set_reorderable(col, TRUE);
      gtk_tree_view_column_set_title(col, "Size");
      gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
      renderer = gtk_cell_renderer_text_new();
      gtk_tree_view_column_pack_start(col, renderer, TRUE);
      gtk_tree_view_column_add_attribute(col, renderer, "text", COL_SIZE); 
  }

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable (col, TRUE);
  gtk_tree_view_column_set_reorderable(col, TRUE);
  gtk_tree_view_column_set_title(col, "Date");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, date_func, 
					  NULL, NULL);

  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_resizable (col, TRUE);
  gtk_tree_view_column_set_reorderable(col, TRUE);
  gtk_tree_view_column_set_title(col, "Quality");
  gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_set_cell_data_func(col, renderer, quality_func, 
					  NULL, NULL);

  gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(view), TRUE);

  return view;
}
