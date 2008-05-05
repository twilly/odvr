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
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "odvr_cfg.h"

static int strcmp0(char *s1, char *s2)
{
    if (s1 && s2)
	return strcmp(s1, s2);
    if ((s1 == NULL) && (s2 == NULL))
	return 0;
    if (s1)
	return 1;
    return -1;
}

/* Load the configuration parameters */
odvr_config_t *odvr_cfg_load_config(void)
{
  char *cfgfilename;
  FILE *f_config;
  odvr_config_t *cfg;

  cfg = g_new0(odvr_config_t, 1);
  if (!cfg)
      return NULL;

  /* Set defaults */
  cfg->dirty = FALSE;
  cfg->version = NULL;
  cfg->destination_dir = g_strconcat (g_get_home_dir(), NULL);
  cfg->show_wav_size = FALSE;
  cfg->keep_original_timestamp = FALSE;
  cfg->window_height = DEFAULT_WINDOW_HEIGHT;
  cfg->window_width = DEFAULT_WINDOW_WIDTH;
  cfg->progress_window_height = DEFAULT_PROGRESS_WINDOW_HEIGHT;
  cfg->progress_window_width = DEFAULT_PROGRESS_WINDOW_WIDTH;
  cfg->date_format = DEFAULT_DATE_FORMAT;
  cfg->report_total_filesize = TRUE;

  cfgfilename = g_build_filename (g_get_home_dir(), CONFIGFILE, NULL);

  f_config = fopen (cfgfilename, "r");

  if (f_config) {
    /* g_print ("reading from file '%s'\n", cfgfilename); */

    char line[1024];
    char *value;

    while (fgets (line, 1023, f_config)) {
      line[strlen(line)-1]= (char) 0; /* eliminate lf */

      if (line[0] == '#')
	continue;
      
      if ((value = strchr (line, ':'))) {

	*value = (char) 0;
	value++;

	if (!strcmp0(line,"version")) {
	    if (strlen(value))
		cfg->version = strdup(value);
	    else
		cfg->version =  NULL;
	}
	else if (!strcmp0(line,"destination_dir")) {
	    if (strlen(value))
		cfg->destination_dir = strdup(value);
	    else
		cfg->destination_dir = g_strconcat (g_get_home_dir(), NULL);
	}
	else if (!strcmp0(line,"show_wav_file_size")) {
	    if (strlen(value))
		cfg->show_wav_size = (atoi(value) != 0);
	    else
		cfg->show_wav_size = FALSE;
	}
	else if (!strcmp0(line,"keep_original_timestamp")) {
	    if (strlen(value))
		cfg->keep_original_timestamp = (atoi(value) != 0);
	    else
		cfg->keep_original_timestamp = FALSE;
	}
	else if (!strcmp0(line,"window_height")) {
	    if (strlen(value))
		cfg->window_height = atoi(value);
	    else
		cfg->window_height = DEFAULT_WINDOW_HEIGHT;
	}
	else if (!strcmp0(line,"window_width")) {
	    if (strlen(value))
		cfg->window_width = atoi(value);
	    else
		cfg->window_width = DEFAULT_WINDOW_WIDTH;
	}
	else if (!strcmp0(line,"progress_window_height")) {
	    if (strlen(value))
		cfg->progress_window_height = atoi(value);
	    else
		cfg->progress_window_height = DEFAULT_WINDOW_HEIGHT;
	}
	else if (!strcmp0(line,"progress_window_width")) {
	    if (strlen(value))
		cfg->progress_window_width = atoi(value);
	    else
		cfg->progress_window_width = DEFAULT_WINDOW_WIDTH;
	}
	else if (!strcmp0(line,"date_format")) {
	    if (strlen(value))
		cfg->date_format = strdup(value);
	    else
		cfg->date_format = DEFAULT_DATE_FORMAT;
	}
	else if (!strcmp0(line,"report_total_filesize")) {
	    if (strlen(value))
		cfg->report_total_filesize = atoi(value);
	    else
		cfg->report_total_filesize = TRUE;
	}
	else
	    g_print("Unknown config key %s\n", line);
      }
    }
    cfg->dirty = FALSE;

    fclose (f_config);
  }

  return cfg;
}

void odvr_cfg_save_config(odvr_config_t *cfg)
{
    char *cfgfilename;
    FILE *f_config;

    if (!cfg)
	return;

  cfgfilename = g_build_filename (g_get_home_dir(), CONFIGFILE, NULL);

  f_config = fopen (cfgfilename, "w");

  if (f_config) {
    fprintf(f_config, "# Configuration file for odvr-gui\n");
    fprintf(f_config, "version:%s\n", cfg->version);
    fprintf(f_config, "destination_dir:%s\n", cfg->destination_dir);
    fprintf(f_config, "show_wav_file_size:%d\n", cfg->show_wav_size?1:0);
    fprintf(f_config, "keep_original_timestamp:%d\n", cfg->keep_original_timestamp?1:0);
    fprintf(f_config, "window_width:%d\n", cfg->window_width);
    fprintf(f_config, "window_height:%d\n", cfg->window_height);
    fprintf(f_config, "progress_window_width:%d\n", cfg->progress_window_width);
    fprintf(f_config, "progress_window_height:%d\n", cfg->progress_window_height);
    fprintf(f_config, "date_format:%s\n", cfg->date_format);
    fprintf(f_config, "report_total_filesize:%d\n", cfg->report_total_filesize);

    fclose(f_config);
  }  
  else {
    g_print("Couldn't open config file %s\n",cfgfilename );
  }
}

#define CFG_GET(TYPE, NAME, DEFAULT)       \
TYPE odvr_cfg_get_##NAME(odvr_config_t *cfg) { \
    if (!cfg)                              \
	return DEFAULT;			   \
    return cfg->NAME;                      \
}

#define CFG_GET_STRING(NAME, DEFAULT)       \
char *odvr_cfg_get_##NAME(odvr_config_t *cfg) { \
    if (!cfg)                               \
	return DEFAULT;			    \
    return cfg->NAME;                       \
}

#define CFG_SET_STRING(NAME)                             \
void odvr_cfg_set_##NAME(odvr_config_t *cfg, char *NAME) \
{                                                        \
    if (!cfg)                                            \
	return;                                          \
    if (strcmp0(cfg->NAME, NAME)) {                      \
	g_free(cfg->NAME);                               \
	cfg->dirty = TRUE;                               \
	cfg->NAME = strdup(NAME);                        \
    }                                                    \
}

#define CFG_SET_ORDINAL(TYPE, NAME)                      \
void odvr_cfg_set_##NAME(odvr_config_t *cfg, TYPE NAME)  \
{                                                        \
    if (!cfg)                                            \
	return;                                          \
    if (cfg->NAME != NAME) {                             \
	cfg->dirty = TRUE;                               \
	cfg->NAME = NAME;                                \
    }                                                    \
}

void odvr_cfg_get_window_size(odvr_config_t *cfg, gint *width, gint *height)
{
    if (!cfg)
	return;
    *height = cfg->window_height;
    *width = cfg->window_width;

}

void odvr_cfg_get_progress_window_size(odvr_config_t *cfg, gint *width, gint *height)
{
    if (!cfg)
	return;
    *height = cfg->progress_window_height;
    *width = cfg->progress_window_width;

}

void odvr_cfg_set_version(odvr_config_t *cfg, char *version)
{
    if (!cfg)
	return;
    if (strcmp0(cfg->version, version)) {
	g_free(cfg->version);
	cfg->version = strdup(version);
    }
}

void odvr_cfg_set_window_size(odvr_config_t *cfg, gint width, gint height)
{
    if (!cfg)
	return;
    if (cfg->window_width != width) {
	cfg->dirty = TRUE;
	cfg->window_width = width;
    }
    if (cfg->window_height != height) {
	cfg->dirty = TRUE;
	cfg->window_height = height;
    }
}

void odvr_cfg_set_progress_window_size(odvr_config_t *cfg, gint width, gint height)
{
    if (!cfg)
	return;
    if (cfg->progress_window_width != width) {
	cfg->dirty = TRUE;
	cfg->progress_window_width = width;
    }
    if (cfg->progress_window_height != height) {
	cfg->dirty = TRUE;
	cfg->progress_window_height = height;
    }
}

CFG_GET(gboolean, dirty, FALSE);
CFG_GET_STRING(destination_dir, NULL);
CFG_GET(gboolean, show_wav_size, FALSE);
CFG_GET(gboolean, keep_original_timestamp, FALSE);
CFG_GET_STRING(date_format, NULL);
CFG_GET(gboolean, report_total_filesize, TRUE);

CFG_SET_STRING(destination_dir);
CFG_SET_ORDINAL(gboolean, show_wav_size);
CFG_SET_ORDINAL(gboolean, keep_original_timestamp);
CFG_SET_STRING(date_format);
CFG_SET_ORDINAL(gboolean, report_total_filesize);
