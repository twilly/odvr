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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixdata.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <ctype.h>
#include <time.h>
#include <utime.h>
#include <signal.h>
#include "olympusdvr.h"
#include "odvr_date.h"
#include "odvr_gui.h"

#define NAME "Olympus Digital Voice Recorder"
#define VERSION "0.1.4.1-cml"

enum {
    ACTION_SELECT_ALL = 0,
    ACTION_SELECT_NONE,
    ACTION_SELECT_BY_QUALITY,
    ACTION_SELECT_BY_DATE,
    ACTION_TRANSFER_FOLDER,
    ACTION_TRANSFER_SELECTED,
    ACTION_TRANSFER_ALL,
    ACTION_CLEAR,
};

typedef struct actionData {
    char action;
    char folderId;
    gint response;
    gint quality_selected; /* Bitmap */
    odvrDate_t start_date_selected, end_date_selected;
    gint total_bytes_to_transfer, total_bytes_transferred;
    gint bytes_to_transfer, bytes_transferred;
    gint files_to_transfer, files_transferred;
    odvrData_t *odvrData;
    gboolean abortTransfer;
} actionData_t;

char folderNames[] = {'A','B','C','D','S'};
extern GdkPixdata TBA, TBB, TBC, TBD, TBS, TBT;
extern GdkPixdata FCA, FCB, FCC, FCD, FCS;
GdkPixdata *transferFolderIcons[5];
GdkPixdata *folderIcons[5] ;


void scan_device(odvrData_t *odvrData)
{
    GtkTreeStore *file_store = odvrData->file_store;
    filestat_t stat;
    int file;
    GtkTreeIter iter, folder_iter;
    odvrDate_t *creationDate;
    int folder;
    odvr dev = odvrData->dev;
    odvrData->quality_used = 0;

    for(folder = ODVR_FOLDER_A; folder <= odvr_foldercount(dev); folder++) 
    {

	if (odvr_foldername(dev, folder) == 'D') {
	    gtk_widget_show (odvrData->buttonD);
	    gtk_widget_show (odvrData->transferMenuD);
	    gtk_widget_show (odvrData->clearMenuD);
	}
	if (odvr_foldername(dev, folder) == 'S') {
	    gtk_widget_show (odvrData->buttonS);
	    gtk_widget_show (odvrData->transferMenuS);
	    gtk_widget_show (odvrData->clearMenuS);
	}

	creationDate = g_new0(odvrDate_t, 1);
	gtk_tree_store_append(file_store, &folder_iter, NULL);
	if ( (folder == odvr_foldercount(dev)) && 
	     (odvr_foldername(dev, folder) == 'S')) 
	    gtk_tree_store_set(file_store, &folder_iter,
			       COL_FOLDER, odvrData->folderIcon[odvr_foldercount(dev)],
			       -1);
	else 
	    gtk_tree_store_set(file_store, &folder_iter,
			       COL_FOLDER, odvrData->folderIcon[folder-ODVR_FOLDER_A],
			       -1);

	for (file = 1; file <= odvr_filecount(dev, folder); file++) 
        {
	    if(odvr_filestat(dev, folder, file, &stat) < 0) 
            {
		gui_err("Error getting file stat", odvr_error(dev));
		exit(1);
	    }
      
	    creationDate = g_new(odvrDate_t, 1);
	    g_date_set_dmy(&creationDate->date, stat.day, stat.month, stat.year+2000);
	    creationDate->hour = stat.hour;
	    creationDate->min = stat.min;
	    creationDate->sec = stat.sec;

	    if (stat.quality < 8*sizeof(odvrData->quality_used))
		odvrData->quality_used |= (1<<stat.quality);

	    /* Append a row to the tree store and add data to it. */
	    gtk_tree_store_append(file_store, &iter, &folder_iter);
	    gtk_tree_store_set(file_store, &iter,
			       COL_FOLDER_ID, folder,
			       COL_SLOT, stat.slot,
			       COL_ID, stat.id,
			       COL_SIZE, odvr_wavfilesize(&stat),
			       COL_LENGTH, odvr_length(&stat),
			       COL_DATE, creationDate,
			       COL_QUALITY, stat.quality,
			       -1);
	}      
    }
}

static void update_progress(actionData_t *actionData)
{
    gdouble progressFraction = 0.0;
    char        progress[128];

    sprintf(progress, "%d of %d", actionData->files_transferred, actionData->files_to_transfer);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(actionData->odvrData->overallProgressBar),
			      progress);
    if (odvr_cfg_get_report_total_filesize(actionData->odvrData->cfg))
	progressFraction = (gdouble)(actionData->total_bytes_transferred)/
	    actionData->total_bytes_to_transfer;
    else
	progressFraction = (gdouble)(actionData->files_transferred)/
	    actionData->files_to_transfer;

    if (progressFraction > 1.0)
	progressFraction = 1.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(actionData->odvrData->overallProgressBar),
				  progressFraction);
    

    progressFraction = (gdouble)(actionData->bytes_transferred)/actionData->bytes_to_transfer;
    sprintf(progress, "%d%%", (int) (progressFraction*100));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(actionData->odvrData->fileProgressBar),
			      progress);
    if (progressFraction > 1.0)
	progressFraction = 1.0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(actionData->odvrData->fileProgressBar),
				  progressFraction);
    
    sprintf(progress, "%d%%", (int) (progressFraction*100));
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(actionData->odvrData->fileProgressBar),
			      progress);

    while (g_main_context_iteration(NULL, FALSE));
}

#define ERR_STRING_SIZE 4096

static gint transfer_file( actionData_t *actionData, uint8_t folder, uint8_t slot, gint previousResponse)
{
    odvr dev = actionData->odvrData->dev;
    filestat_t  instat;
    char        fn[128];
    struct stat outstat;
    FILE       *out;
    gint response = previousResponse;
    pid_t pID ;
    char message[256];
    int status;
    char *errorString;
    int shmFd;

    if(odvr_filestat(dev, folder, slot, &instat) < 0){
	sprintf(message, "Error getting file instat: %s\n", odvr_error(dev));
	acknowledge(message);

	return GTK_RESPONSE_CANCEL;
    }

    sprintf(fn, "%s/D%c_%04d.wav", 
	    gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (actionData->odvrData->ddButton)),
	    odvr_foldername(dev, folder), instat.id);

    if(stat(fn, &outstat) == 0) {
	/* File already exists */
	if (previousResponse == ODVR_RESPONSE_NO_ALL) {
	    actionData->files_transferred++;
	    update_progress(actionData);
	    return ODVR_RESPONSE_NO_ALL;
	}

	if (previousResponse != ODVR_RESPONSE_YES_ALL) {
	    sprintf(message, "File %s exists. Overwrite?", fn);
	    response = yes_no_acknowledge(message);
	    if ((response != GTK_RESPONSE_YES) && (response != ODVR_RESPONSE_YES_ALL)) {
		actionData->files_transferred++;
		update_progress(actionData);
		return response;
	    }
	}
    }

    if((out = fopen(fn, "w+")) == NULL){
	sprintf(message, "Error opening \"%s\" for writing: %s\n",
		fn, strerror(errno));
	acknowledge(message);
	actionData->files_transferred++;
	update_progress(actionData);
	return response;
    }

    actionData->bytes_transferred = 0;
    actionData->bytes_to_transfer = odvr_wavfilesize(&instat);

    /* Fork a process to read the wav file */
    /* Create a shared memory area for any error string returned by the child */
    shmFd = open("/dev/zero", O_RDWR);
    if (shmFd < 0) {
	sprintf(message, 
		"Fatal error - Failed to open file descriptor for shared memory");
	acknowledge(message);
        exit(1);
    }

    errorString = mmap(0, ERR_STRING_SIZE, 
		       PROT_READ | PROT_WRITE, MAP_SHARED,
		       shmFd, 0);
    close(shmFd);

    pID = fork();
    if (pID < 0) { /* failed to fork */
	sprintf(message, "Fatal error - Failed to fork");
	acknowledge(message);
        exit(1);
    }
    else if (pID == 0) { /* child */
	/* Setup sbort signal handler */
	if(odvr_save_wav(dev, folder, slot, fileno(out))) {
	    sprintf(errorString, "Error downloading \"%s\": %s\n", 
		    fn, odvr_error(dev));
	    _exit(1);
	}
	_exit(0);
    }
    else  { /* parent */
	struct timespec sleepTime = {0,10};
	gint previous_bytes_transferred = 0;
	while (waitpid(pID , &status, WNOHANG) == 0) {
	    stat(fn, &outstat);
	    actionData->bytes_transferred = outstat.st_size;
	    actionData->total_bytes_transferred += 
		actionData->bytes_transferred - previous_bytes_transferred;
	    update_progress(actionData);
	    if (actionData->abortTransfer) {
		response = GTK_RESPONSE_CANCEL;
		kill(pID, SIGTERM);
	    }
	    nanosleep(&sleepTime, NULL);
	    previous_bytes_transferred = actionData->bytes_transferred;
	}

	if (actionData->abortTransfer) {
	    /* Aborting a transfer may leave the device in a bad state */
	    odvr_reset(dev, 0);
	}

	if (status && !actionData->abortTransfer) {
	    acknowledge(errorString);
	}

	munmap(errorString, ERR_STRING_SIZE);
    }

    fclose(out);


    if (odvr_cfg_get_keep_original_timestamp(actionData->odvrData->cfg)) {
	/* Set the timestamp of the transferred file back to its original date/time */
	struct tm cal_time;
	time_t ttime;
	struct utimbuf fileTime;

	cal_time.tm_year = instat.year+2000-1900;
	cal_time.tm_mon  = instat.month-1;
	cal_time.tm_mday = instat.day;
	cal_time.tm_hour = instat.hour;
	cal_time.tm_min  = instat.min;
	cal_time.tm_sec  = instat.sec;
	cal_time.tm_wday = 0;
	cal_time.tm_yday = 0;
	cal_time.tm_isdst = 0;
	ttime = mktime(&cal_time);


	if (ttime != -1) {
	    fileTime.actime = ttime;
	    fileTime.modtime = ttime;
	    utime(fn, &fileTime);
	}
    }

    actionData->files_transferred++;
    update_progress(actionData);

    return response;
}

static void transfer_selected(GtkTreeModel *model,
			      GtkTreePath *path,
			      GtkTreeIter *iter,
			      gpointer data)
{
    gint     slot, folder, id;
    actionData_t *actionData = data;
    GtkTreeIter parent;

    if (actionData->response == GTK_RESPONSE_CANCEL)
	return;
    if (actionData->abortTransfer)
	return;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) 
	return; /* Transfer files rather than folders. */

    gtk_tree_model_get(model, iter, COL_SLOT, &slot, -1);
    gtk_tree_model_get(model, iter, COL_ID, &id, -1);
    gtk_tree_model_get(model, iter, COL_FOLDER_ID, &folder, -1);

    actionData->response = transfer_file(actionData, folder, slot, actionData->response);

    gtk_tree_selection_unselect_iter(actionData->odvrData->selection,
				     iter);
    while (g_main_context_iteration(NULL, FALSE));

}

static void bytecount_selected(GtkTreeModel *model,
			      GtkTreePath *path,
			      GtkTreeIter *iter,
			      gpointer data)
{
    gint     wavSize;
    actionData_t *actionData = data;
    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) 
	return; /* Transfer files rather than folders. */

    gtk_tree_model_get(model, iter, COL_SIZE, &wavSize, -1);

    actionData->total_bytes_to_transfer += wavSize;

    return;
}

/* Set the requested folder selected before transfer */
static gboolean select_folder(GtkTreeModel *model,
			      GtkTreePath *path,
			      GtkTreeIter *iter,
			      gpointer data)
{
    gint     folderId;
    actionData_t *actionData = data;
    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) {
	return FALSE; /* Transfer files rather than folders. */
    }

    gtk_tree_model_get(model, iter, COL_FOLDER_ID, &folderId, -1);
    if (folderId-1 != actionData->folderId) {
	gtk_tree_selection_unselect_iter(actionData->odvrData->selection, iter);
	return FALSE;
    }

    gtk_tree_selection_select_iter(actionData->odvrData->selection, iter);
    
    return FALSE;
}

static void progress_callback(actionData_t *actionData)
{
    actionData->abortTransfer = TRUE;
}

static GtkWidget *createProgressDialog(actionData_t *actionData)
{
    GtkWidget *progressDialog;
    GtkWidget *progressBar;
    gint width, height;

    progressDialog = gtk_dialog_new_with_buttons("File transfer",
						 NULL,
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 NULL);

    odvr_cfg_get_progress_window_size(actionData->odvrData->cfg, &width, &height);
    gtk_window_set_default_size(GTK_WINDOW(progressDialog), width, height);

    gtk_dialog_add_button(GTK_DIALOG (progressDialog),  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

    progressBar = gtk_progress_bar_new ();
    actionData->odvrData->overallProgressBar = progressBar;
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(progressDialog)->vbox), progressBar, FALSE, TRUE, 5);
    gtk_widget_show (progressBar);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar),0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar),NULL);

    progressBar = gtk_progress_bar_new ();
    actionData->odvrData->fileProgressBar = progressBar;
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG(progressDialog)->vbox), progressBar, FALSE, TRUE, 5);
    gtk_widget_show (progressBar);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progressBar),0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progressBar),NULL);

    g_signal_connect_swapped (progressDialog,
		      "response", 
		      G_CALLBACK (progress_callback),
		      actionData);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(actionData->odvrData->overallProgressBar),0.0);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(actionData->odvrData->fileProgressBar),0.0);

    gtk_window_set_modal(GTK_WINDOW(progressDialog), TRUE);
    gtk_widget_show (progressDialog);

    return progressDialog;
}

static void destroyProgressDialog(GtkWidget *progressDialog, actionData_t *actionData)
{
    gint width, height;

    gtk_window_get_size(GTK_WINDOW(progressDialog), &width, &height);
    odvr_cfg_set_progress_window_size(actionData->odvrData->cfg, width, height);

    actionData->odvrData->overallProgressBar = NULL;
    actionData->odvrData->fileProgressBar = NULL;
    gtk_widget_hide (progressDialog);
    gtk_object_destroy(GTK_OBJECT(progressDialog));
}

static void transfer(actionData_t *actionData)
{
    GtkWidget *progressDialog;

    actionData->response = GTK_RESPONSE_NONE;
    actionData->abortTransfer = FALSE;

    progressDialog = createProgressDialog(actionData);

    if (actionData->action == ACTION_TRANSFER_FOLDER) {
	/* Mark the selection */
	gtk_tree_model_foreach (gtk_tree_view_get_model(GTK_TREE_VIEW(actionData->odvrData->view)),
				select_folder,
				(gpointer) actionData);
    }
    else if (actionData->action == ACTION_TRANSFER_SELECTED) {
	/* Nothing special to do here */

    }
    else if (actionData->action == ACTION_TRANSFER_ALL) {
	/* Mark everything selected */
	gtk_tree_selection_select_all (GTK_TREE_SELECTION(actionData->odvrData->selection));
    }
    else {
	g_message ("Unknown transfer action"); 
    }


    actionData->response = GTK_RESPONSE_NONE;
    actionData->files_transferred = 0;
    actionData->files_to_transfer = 
	gtk_tree_selection_count_selected_rows(GTK_TREE_SELECTION(actionData->odvrData->selection));
    actionData->total_bytes_transferred = 0;
    actionData->total_bytes_to_transfer = 0;

    /* Find the total bytes to transfer. 
       We only need to do this if we use it in the progress bar */
    if (odvr_cfg_get_report_total_filesize(actionData->odvrData->cfg))
	gtk_tree_selection_selected_foreach (GTK_TREE_SELECTION(actionData->odvrData->selection),
					     bytecount_selected,
					     (gpointer) actionData);
    
    /* Transfer the selected files */
    gtk_tree_selection_selected_foreach (GTK_TREE_SELECTION(actionData->odvrData->selection),
					 transfer_selected,
					 (gpointer) actionData);

    destroyProgressDialog(progressDialog, actionData);
    
}

static gboolean quality_selected(GtkTreeModel *model,
				 GtkTreePath *path,
				 GtkTreeIter *iter,
				 gpointer data)
{
    gint quality;
    actionData_t *actionData = data;

    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) 
	return FALSE; /* Don't select folders, continue */

    gtk_tree_model_get(model, iter, COL_QUALITY, &quality, -1);
    
    if (quality >= 8*sizeof(actionData->quality_selected)) {
	gtk_tree_selection_unselect_iter(actionData->odvrData->selection,
					 iter);
	return FALSE; /* Invalid quality setting */
    }

    if (actionData->quality_selected & (1<<quality))
	gtk_tree_selection_select_iter(actionData->odvrData->selection,
				       iter);
    else
	gtk_tree_selection_unselect_iter(actionData->odvrData->selection,
					 iter);

    return FALSE;
}

static void select_by_quality(actionData_t *actionData )
{
    gint quality;
    GtkWidget *qualityDialog;
    GtkWidget *qualityCheckButton[ODVR_QUALITY_NUM]; 

    qualityDialog = gtk_dialog_new_with_buttons("Select quality settings",
						NULL,
						GTK_DIALOG_DESTROY_WITH_PARENT,
						NULL);

    /* Create toggle buttons for each of the qualities available.
       Set any currently selected to active. */
    for (quality = 0; quality < ODVR_QUALITY_NUM; quality++) {
	if (actionData->odvrData->quality_used & (1<<quality)) {
	    qualityCheckButton[quality] = 
		gtk_check_button_new_with_label(odvr_quality_name(quality));
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(qualityCheckButton[quality]),  
					 (actionData->quality_selected & (1<<quality)));
	    gtk_box_pack_start (GTK_BOX(GTK_DIALOG(qualityDialog)->vbox), 
				GTK_WIDGET(qualityCheckButton[quality]), FALSE, FALSE, 0);
	    gtk_widget_show(qualityCheckButton[quality]);
	}
	else
	    qualityCheckButton[quality] = NULL;
    }

    gtk_dialog_add_button(GTK_DIALOG (qualityDialog),  GTK_STOCK_OK, GTK_RESPONSE_OK);

    /* Pop up the box to select the quality settings to select */
    gtk_dialog_run (GTK_DIALOG (qualityDialog));
    /* Extract the selected quality settings */
    actionData->quality_selected = 0;
    for (quality = 0; quality < ODVR_QUALITY_NUM; quality++) {
	if (qualityCheckButton[quality])
	    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(qualityCheckButton[quality])))
		actionData->quality_selected |= (1<<quality);
    }
    gtk_widget_destroy (GTK_WIDGET(qualityDialog));

    gtk_tree_model_foreach (gtk_tree_view_get_model(GTK_TREE_VIEW(actionData->odvrData->view)),
			    quality_selected,
			    (gpointer) actionData);

}

static gboolean date_select(GtkTreeModel *model,
		      GtkTreePath *path,
		      GtkTreeIter *iter,
		      gpointer data)
{
    actionData_t *actionData = data;

    GtkTreeIter parent;

    if (!gtk_tree_model_iter_parent(model, &parent, iter)) 
	return FALSE; /* Don't select folders. */

/* If date is in range, select, else unselect */
    gtk_tree_selection_unselect_iter(actionData->odvrData->selection,
				     iter);

    return FALSE;
}


static void select_by_date(actionData_t *actionData )
{

/* Ask for date range */

    gtk_tree_model_foreach (gtk_tree_view_get_model(GTK_TREE_VIEW(actionData->odvrData->view)),
			    date_select,
			    (gpointer) actionData);
}

static void select_files( actionData_t *actionData )
{
    switch (actionData->action) {
    case ACTION_SELECT_ALL:
	gtk_tree_selection_select_all (GTK_TREE_SELECTION(actionData->odvrData->selection));
	break;
    case ACTION_SELECT_NONE:
	gtk_tree_selection_unselect_all (GTK_TREE_SELECTION(actionData->odvrData->selection));
	break;
    case ACTION_SELECT_BY_QUALITY:
	select_by_quality(actionData);
	break;
    case ACTION_SELECT_BY_DATE:
	select_by_date(actionData);
	g_message ("Select by date");
	break;
    default:
	g_message ("Unknown select");
    }
}

static void clear(  actionData_t *actionData )
{
    char buf[64];
    
    sprintf(buf, "This will remove all files from folder %c\n Are you sure?",
	    odvr_foldername(actionData->odvrData->dev, actionData->folderId+1));
    if (acknowledge_ok_cancel(buf) == GTK_RESPONSE_OK) {
	odvr_clear_folder(actionData->odvrData->dev, actionData->folderId+1);
	
    }
}

/* transfer button pressed */
static gboolean transfer_event( GtkWidget *widget,
                              actionData_t *actionData )
{
    transfer(actionData);

    return FALSE;

}

static void about( gpointer callback_data,
		   guint callback_action,
		   GtkWidget *menu_item )
{
    gchar *authors[] = {"Conor McLoughin", "Tristan Willy", NULL};

    gtk_show_about_dialog(NULL,
			  "version", VERSION,
			  "authors", authors,
			  "copyright", "2007-2008 Conor McLoughin, Tristan Willy",
			  "license", "This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.",
			  "wrap-license", TRUE,
			  "website", "http://code.google.com/p/odvr/",
			  NULL);

			  
}




gboolean view_selection_funct (GtkTreeSelection *selection,
			       GtkTreeModel *model,
			       GtkTreePath *path,
			       gboolean path_currently_selected,
			       gpointer userdata)
{
    GtkTreeIter iter, parent;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
	if (!gtk_tree_model_iter_parent(model, &parent, &iter)) {
	    return FALSE; /* Folder cannot be selected */
	}
	else
	    return TRUE; /* Not a folder. Can be selected */	    
    }
    return TRUE;
}


void add_menus(odvrData_t *odvrData, int numFolders)
{
    GtkWidget *file_menu, *transfer_menu, *clear_menu, *help_menu, *select_menu;
    GtkWidget *file_item, *transfer_item, *clear_item, *help_item, *select_item;
    GtkWidget *menu_item;
    GtkWidget *quit_item;
    actionData_t actionData;
    int folder;

    odvrData->menu_bar = gtk_menu_bar_new();
    gtk_widget_show (odvrData->menu_bar);

    /* File menu */
    file_menu = gtk_menu_new();
    gtk_widget_show (file_menu);

    quit_item = gtk_menu_item_new_with_label ("Quit");
    gtk_menu_shell_append (GTK_MENU_SHELL (file_menu), quit_item);
    g_signal_connect(G_OBJECT (quit_item), "activate",
		     G_CALLBACK (quit_event), 
		     (gpointer) odvrData);
    gtk_widget_show (quit_item);

    file_item = gtk_menu_item_new_with_label ("File");
    gtk_widget_show (file_item);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (file_item), file_menu);
    gtk_menu_bar_append (GTK_MENU_BAR (odvrData->menu_bar), file_item);


    /* Transfer menu */
    transfer_menu = gtk_menu_new();
    gtk_widget_show (transfer_menu);

    for(folder = 0; folder < sizeof(folderNames); folder++) {
	char buf[] = "Folder ?";
	buf[7] = folderNames[folder];
	menu_item = gtk_menu_item_new_with_label (buf);
	gtk_menu_shell_append (GTK_MENU_SHELL (transfer_menu), menu_item);
	actionData.action = ACTION_TRANSFER_FOLDER;
	actionData.folderId = folder;
	actionData.odvrData = odvrData;
	g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
				  G_CALLBACK (transfer), 
				  (gpointer) g_memdup(&actionData, sizeof(actionData)) );

	gtk_widget_show (menu_item);
	if (folderNames[folder] == 'D') {
	    odvrData->transferMenuD = menu_item;
	    gtk_widget_hide (menu_item);
	}
	if (folderNames[folder] == 'S') {
	    odvrData->transferMenuS = menu_item;
	    gtk_widget_hide (menu_item);
	}
    }

    menu_item = gtk_menu_item_new_with_label ("Selected files");
    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (transfer_menu), menu_item);
    actionData.action = ACTION_TRANSFER_SELECTED;
    actionData.odvrData = odvrData;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (transfer),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );

    menu_item = gtk_menu_item_new_with_label ("All files");
    gtk_widget_show (menu_item);
    gtk_menu_shell_append (GTK_MENU_SHELL (transfer_menu), menu_item);
    actionData.action = ACTION_TRANSFER_ALL;
    actionData.odvrData = odvrData;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (transfer),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );


    transfer_item = gtk_menu_item_new_with_label ("Transfer");
    gtk_widget_show (transfer_item);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (transfer_item), transfer_menu);
    gtk_menu_bar_append (GTK_MENU_BAR (odvrData->menu_bar), transfer_item);


    /* Select menu */
    select_menu = gtk_menu_new();
    gtk_widget_show (select_menu);

    menu_item = gtk_menu_item_new_with_label ("All");
    gtk_menu_shell_append (GTK_MENU_SHELL (select_menu), menu_item);
    actionData.action = ACTION_SELECT_ALL;
    actionData.odvrData = odvrData;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (select_files),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );
    gtk_widget_show (menu_item);


    menu_item = gtk_menu_item_new_with_label ("None");
    gtk_menu_shell_append (GTK_MENU_SHELL (select_menu), menu_item);
    actionData.action = ACTION_SELECT_NONE;
    actionData.odvrData = odvrData;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (select_files),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );
    gtk_widget_show (menu_item);

    menu_item = gtk_menu_item_new_with_label ("By quality");
    gtk_menu_shell_append (GTK_MENU_SHELL (select_menu), menu_item);
    actionData.action = ACTION_SELECT_BY_QUALITY;
    actionData.odvrData = odvrData;
    actionData.quality_selected = 0;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (select_files),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );
    gtk_widget_show (menu_item);

#if 0
    menu_item = gtk_menu_item_new_with_label ("By date");
    gtk_menu_shell_append (GTK_MENU_SHELL (select_menu), menu_item);
    actionData.action = ACTION_SELECT_BY_DATE;
    actionData.odvrData = odvrData;
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
			      G_CALLBACK (select_files),
			      (gpointer) g_memdup(&actionData, sizeof(actionData)) );
    gtk_widget_show (menu_item);
#endif

    select_item = gtk_menu_item_new_with_label ("Select");
    gtk_widget_show (select_item);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (select_item), select_menu);
    gtk_menu_bar_append (GTK_MENU_BAR (odvrData->menu_bar), select_item);



    /* Clear menu */
    clear_menu = gtk_menu_new();
    gtk_widget_show (clear_menu);

    for(folder = 0; folder < sizeof(folderNames); folder++) {
	char buf[] = "Folder ?";
	buf[7] = folderNames[folder];
	menu_item = gtk_menu_item_new_with_label (buf);
	gtk_menu_shell_append (GTK_MENU_SHELL (clear_menu), menu_item);
	actionData.action = ACTION_CLEAR;
	actionData.folderId = folder;
	actionData.odvrData = odvrData;
	g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
				  G_CALLBACK (clear),
				  (gpointer) g_memdup(&actionData, sizeof(actionData)) );

	gtk_widget_show (menu_item);
	if (folderNames[folder] == 'D') {
	    odvrData->clearMenuD = menu_item;
	    gtk_widget_hide (menu_item);
	}
	if (folderNames[folder] == 'S') {
	    odvrData->clearMenuS = menu_item;
	    gtk_widget_hide (menu_item);
	}
    }

    clear_item = gtk_menu_item_new_with_label ("Clear");
    gtk_widget_show (clear_item);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (clear_item), clear_menu);
    gtk_menu_bar_append (GTK_MENU_BAR (odvrData->menu_bar), clear_item);


    /* Help menu */
    help_menu = gtk_menu_new();
    gtk_widget_show (help_menu);

    menu_item = gtk_menu_item_new_with_label ("About");
    gtk_menu_shell_append (GTK_MENU_SHELL (help_menu), menu_item);
    g_signal_connect_swapped (G_OBJECT (menu_item), "activate",
                              G_CALLBACK (about), (gpointer) 0);
    gtk_widget_show (menu_item);

    help_item = gtk_menu_item_new_with_label ("Help");
    gtk_menu_item_set_right_justified(GTK_MENU_ITEM(help_item), TRUE);
    gtk_widget_show (help_item);

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (help_item), help_menu);
    gtk_menu_bar_append (GTK_MENU_BAR (odvrData->menu_bar), help_item);

}

gint add_buttons(odvrData_t *odvrData, int numFolders)
{
    GtkWidget *button = NULL;
    GtkWidget *image;
    int folder;
    actionData_t *actionData;
    GError *error=NULL;
    GdkPixbuf *pixBuf;

    /* Add botton box */
    if (odvrData->buttonBox == NULL) {
	odvrData->buttonBox = gtk_hbox_new (FALSE, 5);
	gtk_widget_show (odvrData->buttonBox);
    }

    for(folder = 0; folder < sizeof(folderNames); folder++) {
	char toolTip[] = "Transfer folder A";

	/* Create "Transfer folder" button */
	button = gtk_button_new();
	toolTip[16]= folderNames[folder];
	gtk_widget_set_tooltip_text(button, toolTip);
	
	pixBuf = gdk_pixbuf_from_pixdata(transferFolderIcons[folder], FALSE, &error);
	if (error) {
	    gui_err("gdk_pixbuf_from_pixdata failed.", error->message);
	}
	image = gtk_image_new_from_pixbuf(pixBuf);

	odvrData->folderIcon[folder] = gdk_pixbuf_from_pixdata(folderIcons[folder], 
							       FALSE, &error);
	if (error) {
	    gui_err("Could not load icon.", error->message);
	    g_free(error);
	    error=NULL;
	    return -1;
	}

	gtk_button_set_image(GTK_BUTTON(button), image);

	actionData = g_new(actionData_t, 1);
	actionData->action = ACTION_TRANSFER_FOLDER;
	actionData->folderId = folder;
	actionData->odvrData = odvrData;

	g_signal_connect (G_OBJECT (button), "clicked",
			  G_CALLBACK (transfer_event), actionData);

	/* Put the button in the box */
	gtk_box_pack_start (GTK_BOX (odvrData->buttonBox), button, FALSE, FALSE, 0);
		
	if ( (folderNames[folder] == 'D') ||  (folderNames[folder] == 'S') )
	{
	    gtk_widget_hide (button);
	    if (folderNames[folder] == 'D')
		odvrData->buttonD = button;
	    if (folderNames[folder] == 'S')
		odvrData->buttonS = button;
	}
	else
	{
	    gtk_widget_show (button);
	}
	
    }

    pixBuf = gdk_pixbuf_from_pixdata(&TBT, FALSE, &error);
    image = gtk_image_new_from_pixbuf(pixBuf);
    button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, "Transfer selected files");
    gtk_button_set_image(GTK_BUTTON(button), image);

    actionData = g_new(actionData_t, 1);
    actionData->action = ACTION_TRANSFER_SELECTED;
    actionData->folderId = -1;
    actionData->odvrData = odvrData;
    g_signal_connect (G_OBJECT (button), "clicked",
		      G_CALLBACK (transfer_event), actionData);

    gtk_box_pack_start (GTK_BOX (odvrData->buttonBox), button, FALSE, FALSE, 0);
    gtk_widget_show (button);

    return 0;
}

void add_destinationDir(odvrData_t *odvrData)
{
    char *destinationDir;

    GtkWidget *label, *ddDialog;

    /* Add box for destination directory */
    if (odvrData->destinationDirBox == NULL) {
	odvrData->destinationDirBox = gtk_vbox_new (FALSE, 1);
	gtk_widget_show (odvrData->destinationDirBox);
    }

    label = gtk_label_new(NULL);
    gtk_label_set_markup (GTK_LABEL (label), 
			  "<span size=\"large\">Destination directory</span>");

    gtk_box_pack_start (GTK_BOX (odvrData->destinationDirBox), label, FALSE, FALSE, 0);
    gtk_widget_show (label);

    /* Create button to select destination file/directory */
    ddDialog = gtk_file_chooser_dialog_new("Destination directory",
					   NULL,
					   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
					   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					   GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
					   NULL);
    odvrData->ddButton = gtk_file_chooser_button_new_with_dialog(ddDialog);

    gtk_box_pack_start (GTK_BOX (odvrData->destinationDirBox), odvrData->ddButton, FALSE, TRUE, 0);
    gtk_widget_show (odvrData->ddButton);

    /* Set the default or configured destination directory */
    destinationDir = odvr_cfg_get_destination_dir(odvrData->cfg);
    if (destinationDir) {
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(odvrData->ddButton),
					    destinationDir);
    }


}


int main(int argc, char *argv[])
{
    odvrData_t odvrData;
    odvr        dev;
    int         open_flags;
    GtkWidget *mainVbox;
    GtkWidget *fileWindow = NULL;
    GtkWidget *view;
    GtkWidget *separator;
    GtkTreeStore *file_store;
    GtkTreeSelection *selection;
    const char *model = NULL;
    int numFolders = ODVR_NUM_FOLDERS;
    gint width, height;

    gtk_init (&argc, &argv);

    memset(&odvrData, 0, sizeof(odvrData));

    odvrData.cfg = odvr_cfg_load_config();
    odvr_cfg_set_version(odvrData.cfg, VERSION);

    transferFolderIcons[0] = &TBA;
    transferFolderIcons[1] = &TBB;
    transferFolderIcons[2] = &TBC;
    transferFolderIcons[3] = &TBD;
    transferFolderIcons[4] = &TBS;
    folderIcons[0] = &FCA;
    folderIcons[1] = &FCB;
    folderIcons[2] = &FCC;
    folderIcons[3] = &FCD;
    folderIcons[4] = &FCS;

    open_flags = 0;

    while((dev = odvr_open(open_flags)) == NULL) {
	if (acknowledge_ok_cancel("Failed to open Olympus device.\nConnect and try again") ==
	    GTK_RESPONSE_CANCEL)
	    return -1;

    }
    odvrData.dev = dev;

    if (dev)
    {
	if((model = odvr_model(dev)) == NULL) 
        {
	    g_print("Couldn't query model name. %s", 
		    odvr_error(dev));
	}
	
	numFolders = odvr_foldercount(dev);
    }

    /* Create a new window */
    odvrData.window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW (odvrData.window), 
			  NAME);

    odvr_cfg_get_window_size(odvrData.cfg, &width, &height);
    gtk_window_set_default_size(GTK_WINDOW(odvrData.window), width, height);

    set_date_format(odvr_cfg_get_date_format(odvrData.cfg));

    /* Set a handler for delete_event that immediately
     * exits GTK. */
    g_signal_connect (G_OBJECT (odvrData.window), "delete_event",
                      G_CALLBACK (delete_event), NULL);

    /* Sets the border width of the window. */
    gtk_container_set_border_width (GTK_CONTAINER (odvrData.window), 5);

    mainVbox = gtk_vbox_new (FALSE, 5);
    gtk_widget_show (mainVbox);
    gtk_container_add (GTK_CONTAINER (odvrData.window), mainVbox);

    add_menus(&odvrData, numFolders);
    gtk_box_pack_start(GTK_BOX(mainVbox), odvrData.menu_bar, FALSE, FALSE, 1);


    /* Create a list store to store the files on the folder. */
    file_store = gtk_tree_store_new(8,
				    GDK_TYPE_PIXBUF,  /* Folder  */
				    G_TYPE_UINT,      /* Folder  */
				    G_TYPE_UINT,      /* Slot    */
				    G_TYPE_UINT,      /* File    */
				    G_TYPE_UINT,      /* Size    */
				    G_TYPE_FLOAT,     /* Length  */
				    G_TYPE_POINTER,   /* Date    */
				    G_TYPE_UINT);     /* Quality */
    
    odvrData.file_store = file_store;

    if (add_buttons(&odvrData, numFolders) < 0)
	return -1;

    gtk_box_pack_start(GTK_BOX(mainVbox), odvrData.buttonBox, FALSE, FALSE, 1);

    separator = gtk_hseparator_new ();	
    gtk_box_pack_start (GTK_BOX (mainVbox), separator, FALSE, TRUE, 1);
    gtk_widget_show (separator);


    add_destinationDir(&odvrData);
    gtk_box_pack_start(GTK_BOX(mainVbox), odvrData.destinationDirBox, FALSE, FALSE, 1);

    separator = gtk_hseparator_new ();	
    gtk_box_pack_start (GTK_BOX (mainVbox), separator, FALSE, TRUE, 1);
    gtk_widget_show (separator);

    fileWindow = gtk_scrolled_window_new (NULL, NULL);
    gtk_widget_show (fileWindow);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(fileWindow),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);



    if (dev)
    {	
	gtk_window_set_title (GTK_WINDOW (odvrData.window), 
			  model);

	scan_device(&odvrData);
    }

    view = create_view(&odvrData);
    odvrData.view = view;

    gtk_widget_show_all (view);

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(file_store));
    g_object_unref(file_store); /* destroy model automatically with view */
    
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    odvrData.selection = selection;
    gtk_tree_selection_set_select_function(selection, 
					   view_selection_funct, 
					   NULL, NULL);
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    gtk_container_add(GTK_CONTAINER(fileWindow), view);
    gtk_container_add(GTK_CONTAINER(mainVbox), fileWindow);

    gtk_widget_show (odvrData.window);


    gtk_main ();


    exit(0);
}
