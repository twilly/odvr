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
#ifndef ODVR_CFG_H
#define ODVR_CFG_H

#include <glib.h>

#define CONFIGFILE ".odvr-gui.cfg"
#define DEFAULT_WINDOW_WIDTH 500
#define DEFAULT_WINDOW_HEIGHT 600
#define DEFAULT_PROGRESS_WINDOW_WIDTH 500
#define DEFAULT_PROGRESS_WINDOW_HEIGHT 100
#define DEFAULT_DATE_FORMAT "%Y/%m/%d"

typedef struct odvr_config {
    gboolean dirty;
    char *version;
    char *destination_dir;
    gboolean show_wav_size;
    gboolean keep_original_timestamp;
    gint window_height, window_width;
    gint progress_window_height, progress_window_width; 
    char *date_format;
    gboolean report_total_filesize;
} odvr_config_t;

odvr_config_t *odvr_cfg_load_config(void);
void odvr_cfg_save_config(odvr_config_t *cfg);
gboolean odvr_cfg_get_dirty(odvr_config_t *cfg);
void odvr_cfg_set_version(odvr_config_t *cfg, char *version);
char *odvr_cfg_get_version(odvr_config_t *cfg);
void odvr_cfg_set_destination_dir(odvr_config_t *cfg, char *destination_dir);
char *odvr_cfg_get_destination_dir(odvr_config_t *cfg);
void odvr_cfg_set_show_wav_size(odvr_config_t *cfg, gboolean show_wav_size);
gboolean odvr_cfg_get_show_wav_size(odvr_config_t *cfg);
void odvr_cfg_set_keep_original_timestamp(odvr_config_t *cfg, gboolean keep_original_timestamp);
gboolean odvr_cfg_get_keep_original_timestamp(odvr_config_t *cfg);
void odvr_cfg_set_window_size(odvr_config_t *cfg, gint width, gint height);
void odvr_cfg_set_progress_window_size(odvr_config_t *cfg, gint width, gint height);
void odvr_cfg_get_window_size(odvr_config_t *cfg, gint *width, gint *height);
void odvr_cfg_get_progress_window_size(odvr_config_t *cfg, gint *width, gint *height);
void odvr_cfg_set_date_format(odvr_config_t *cfg, char *date_format);
char *odvr_cfg_get_date_format(odvr_config_t *cfg);
void odvr_cfg_set_report_total_filesize(odvr_config_t *cfg, gboolean report_total_filesize);
gboolean odvr_cfg_get_report_total_filesize(odvr_config_t *cfg);

#endif /* ODVR_CFG_H */
