/* olympus: Olympus DVR CLI
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "olympusdvr.h"

#define VERSION    "0.1.5"

void download_folder(odvr dev, uint8_t folder);
void download_folder_raw(odvr dev, uint8_t folder);

void print_listing(odvr dev);
int  user_confirmed(const char *);
void print_usage(void);

int main(int argc, char *argv[]){
  int         opt, opt_download, opt_ls, opt_reset, opt_debug,
              opt_clear, opt_delete, opt_yesall, opt_raw;
  int         i, open_flags;
  const char *model;
  odvr        dev;

  opt_ls = opt_download = opt_reset = opt_debug = opt_clear =
    opt_delete = opt_yesall = opt_raw = 0;
  while((opt = getopt(argc, argv, "hvleErDd:cx:y")) != -1){
    switch(opt){
    case 'h':
      print_usage();
      exit(1);
    case 'v':
      fprintf(stderr, "odvr version %s\n", VERSION);
      exit(1);
    case 'e':
      opt_download = 1;
      break;
    case 'E':
      opt_raw = 1;
      break;
    case 'r':
      opt_reset = 1;
      break;
    case 'd':
      if(optarg[1] == '\0')
        opt_download = toupper(optarg[0]);
      if(!((opt_download >= 'A' && opt_download <= 'D') ||
           opt_download == 'S')){
        fprintf(stderr, "Bad download option.\n");
        print_usage();
        exit(1);
      }
      break;
    case 'D':
      opt_debug = 1;
      break;
    case 'l':
      opt_ls = 1;
      break;
    case 'c':
      opt_clear = 1;
      break;
    case 'x':
      if(optarg[1] == '\0')
        opt_delete = toupper(optarg[0]);
      if(!((opt_delete >= 'A' && opt_delete <= 'D') ||
           opt_delete == 'S')){
        fprintf(stderr, "Bad delete option.\n");
        print_usage();
        exit(1);
      }
      break;
    case 'y':
      opt_yesall = 1;
      break;
    case '?':
    case ':':
    default:
      print_usage();
      exit(1);
    }
  }

  open_flags = 0;
  if(opt_debug)
    open_flags = ODVR_TRACE;

  if((dev = odvr_open(open_flags)) == NULL){
    fprintf(stderr, "Failed to open Olympus device: %s\n", odvr_error(NULL));
    exit(1);
  }

  /* fixme: odvr_close() should leave the device in a stable state */
  if(opt_reset){
    printf("Resetting...\n");
    if(odvr_reset(dev, open_flags)){
      fprintf(stderr, "Failed to reset and reaquire device: %s\n", odvr_error(dev));
      exit(1);
    }
  }

  if((model = odvr_model(dev)) == NULL){
    fprintf(stderr, "Couldn't query model name: %s\n", odvr_error(dev));
    exit(1);
  }
  printf("Model: %s\n", model);

  if(opt_ls){
    print_listing(dev);
  }

  if(opt_download == 1){
    for(i = ODVR_FOLDER_A; i <= odvr_foldercount(dev); i++)
      download_folder(dev, i);
  } else if(opt_download > 1 &&
            odvr_foldercode(dev, opt_download) <= odvr_foldercount(dev)){
    download_folder(dev, odvr_foldercode(dev, opt_download));
  }

  if(opt_raw != 0)
    for(i = ODVR_FOLDER_A; i <= odvr_foldercount(dev); i++)
      download_folder_raw(dev, i);

  if(opt_clear){
    if(opt_yesall || user_confirmed("delete all recordings")){
      for(i = ODVR_FOLDER_A; i <= odvr_foldercount(dev); i++){
        if(odvr_clear_folder(dev, i))
          fprintf(stderr, "Couldn't clear folder %c: %s\n", odvr_foldername(dev, i),
            odvr_error(dev));
      }
    }
  }
  if(opt_delete){
    if(!opt_yesall)
      printf("Going to delete folder %c.\n", opt_delete);
    if(opt_yesall || user_confirmed(NULL)){
      if(odvr_clear_folder(dev, odvr_foldercode(dev, opt_delete)))
        fprintf(stderr, "Couldn't clear folder %c: %s\n", opt_delete,
          odvr_error(dev));
    }
  }

  odvr_close(dev);
  exit(0);
}

void print_listing(odvr dev){
  filestat_t stat;
  int        folder, i, numfiles, sec, min, hour;

  for(folder = ODVR_FOLDER_A; folder <= odvr_foldercount(dev); folder++){
    if((numfiles = odvr_filecount(dev, folder)) < 0){
      fprintf(stderr, "Couldn't get file count on Folder %c: %s\n",
          odvr_foldername(dev,folder), odvr_error(dev));
      continue;
    }

    printf("Folder %c (%d files):\n", odvr_foldername(dev, folder), numfiles);
    printf("Slot File     Length       Date          Quality\n");
    for(i = 1; i <= numfiles; i++){
      if(odvr_filestat(dev, folder, i, &stat) < 0){
        fprintf(stderr, "Error getting file stat: %s\n", odvr_error(dev));
        return;
      }
      sec  = odvr_length(&stat) + 0.5;
      hour = sec / 3600;
      sec  = sec % 3600;
      min  = sec / 60;
      sec  = sec % 60;
      printf("%-4d %-6d %02d:%02d:%02d %2d/%02d/%4d %2d:%02d:%02d %s\n",
          stat.slot, stat.id, hour, min, sec, stat.month,
          stat.day, stat.year + 2000,
          stat.hour, stat.min, stat.sec,
          odvr_quality_name(stat.quality));
    }
  }
}

void download_folder(odvr dev, uint8_t folder){
  filestat_t  instat;
  struct stat outstat;
  FILE       *out;
  uint8_t     slot;
  char        fn[128];

  for(slot = 1; slot <= odvr_filecount(dev, folder); slot++){
    if(odvr_filestat(dev, folder, slot, &instat) < 0){
      fprintf(stderr, "Error getting file instat: %s\n", odvr_error(dev));
      continue;
    }

    sprintf(fn, "D%c_%04d.wav", odvr_foldername(dev, folder), instat.id);

    if(stat(fn, &outstat) == 0){
      fprintf(stderr, "\"%s\" already exists. Skipping this file.\n", fn);
      continue;
    }

    if((out = fopen(fn, "w+")) == NULL){
      fprintf(stderr, "Error opening \"%s\" for writing: %s\n",
          fn, strerror(errno));
      continue;
    }

    printf("Downloading \"%s\"...\n", fn);
    if(odvr_save_wav(dev, folder, slot, fileno(out)))
      fprintf(stderr, "Error downloading \"%s\": %s\n", fn, odvr_error(dev));

    fclose(out);
  }
}

void download_folder_raw(odvr dev, uint8_t folder){
  filestat_t  instat;
  struct stat outstat;
  FILE       *out;
  uint8_t     slot;
  char        fn[128];

  for(slot = 1; slot <= odvr_filecount(dev, folder); slot++){
    if(odvr_filestat(dev, folder, slot, &instat) < 0){
      fprintf(stderr, "Error getting file instat: %s\n", odvr_error(dev));
      continue;
    }

    sprintf(fn, "D%c_%04d.raw", odvr_foldername(dev, folder), instat.id);

    if(stat(fn, &outstat) == 0){
      fprintf(stderr, "\"%s\" already exists. Skipping this file.\n", fn);
      continue;
    }

    if((out = fopen(fn, "w+")) == NULL){
      fprintf(stderr, "Error opening \"%s\" for writing: %s\n",
          fn, strerror(errno));
      continue;
    }

    printf("Downloading \"%s\"...\n", fn);
    if(odvr_save_raw(dev, folder, slot, fileno(out)))
      fprintf(stderr, "Error downloading \"%s\": %s\n", fn, odvr_error(dev));

    fclose(out);
  }
}


int user_confirmed(const char *action){
  char answer[64];
  int  i;

  if(action)
    printf("Going to %s.\n", action);
  printf("Are you sure (yes/no)? ");
  if(!fgets(answer, 64, stdin)){
    return 0; /* no input */
  }

  /* lowercase and check for "yes" or "y" */
  for(i = 0; i < 64 && answer[i]; i++)
    answer[i] = tolower(answer[i]);
  if(!strncmp("yes\n", answer, 64) ||
     !strncmp("y\n", answer, 64)){
    return 1;
  }

  return 0;
}

void print_usage(void){
  fprintf(stderr,
    "Usage: odvr [options]\n"
    "-= Options =-\n"
    "  -h             : This help.\n"
    "  -v             : Print version.\n"
    "  -d <folder>    : Download all files in <folder>.\n"
    "  -e             : Download everything.\n"
    "  -l             : List all files.\n"
    "  -x <folder>    : Delete all recordings in <folder>.\n"
    "  -c             : Delete all recordings.\n"
    "  -y             : \"yes\" to all yes/no questions.\n"
    "  -r             : Reset the DVR. This may fix some sync issues.\n"
    "  -D             : Enable debug tracing.\n"
    "  -E             : Download everything in RAW format.\n"
  );
}
