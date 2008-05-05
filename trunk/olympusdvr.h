/* olympusdvr.h: Olympus DVR library
 *
 * Copyright (C) 2007-2008 Tristan Willy <tristan.willy at gmail.com>
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
#ifndef OLYMPUSDVR_H
#define OLYMPUSDVR_H

#include <stdint.h>

/* odvr_open flags */
#define ODVR_TRACE          1

/* misc defined */
#define ODVR_FOLDER_A       1
#define ODVR_FOLDER_B       2
#define ODVR_FOLDER_C       3
#define ODVR_FOLDER_D       4
#define ODVR_NUM_FOLDERS    5

#define ODVR_QUALITY_HQ     2
#define ODVR_QUALITY_SP     0
#define ODVR_QUALITY_LP     1
#define ODVR_QUALITY_XHQ    8

typedef struct odvr * odvr;

typedef struct filestat {
  uint8_t  folder;
  uint8_t  slot;
  uint16_t id;
  uint16_t size;
  uint8_t  month;
  uint8_t  day;
  uint8_t  year;  /* year = 2000 + filestat.year */
  uint8_t  hour;
  uint8_t  min;
  uint8_t  sec;
  uint8_t  quality;
} filestat_t;

/* open and close interfaces */
odvr odvr_open(int);
void odvr_close(odvr);

/* resets the DVR and reconnects to it */
int odvr_reset(odvr, int);

/* return a string describing an error */
const char *odvr_error(odvr);

/* Turn on/off tracing */
int odvr_enable_trace(odvr);
int odvr_disable_trace(odvr);

/* return specific model (multiple models per product ID) */
const char *odvr_model(odvr);
/* returns the number of used slots in a folder */
int         odvr_filecount(odvr, uint8_t folder);
/* returns the number of folders in the device */
int         odvr_foldercount(odvr);
/* returns folder code for ASCII character name */
uint8_t     odvr_foldercode(odvr, const char);
/* returns folder ASCII character name for a given folder code */
char        odvr_foldername(odvr, const uint8_t);
/* get folder/slot metadata, like quality and recording date */
int         odvr_filestat(odvr, uint8_t folder, uint8_t slot, filestat_t *);
/* file size to length convert */
float       odvr_length(filestat_t *);
/* returns a description of a quality level */
const char *odvr_quality_name(uint8_t quality);
/* open a folder/slot for reading */
int         odvr_open_file(odvr, uint8_t folder, uint8_t slot);
/* returns minimum block size */
int         odvr_block_size(odvr);
/* read a block of signed PCM 16 */
int         odvr_read_block(odvr, void *block, int maxbytes, uint8_t quality);
/* open, read, and save a WAV file */
int         odvr_save_wav(odvr, uint8_t folder, uint8_t slot, int fd);
/* delete a whole folder */
int         odvr_clear_folder(odvr, uint8_t folder);

#endif /* OLYMPUSDVR_H */
