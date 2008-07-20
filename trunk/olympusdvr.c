/* olympusdvr.c: Olympus DVR library
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
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <sndfile.h>
#include <usb.h>
#include "olympusdvr.h"

#define FOLDER_IDX(x)         ((x) - 1)
#define SLOT_IDX(x)           ((x) - 1)
#define IDX_FOLDER(x)         ((x) + 1)
#define IDX_SLOT(x)           ((x) + 1)
#define MAX_MODEL_LENGTH      32
#define MAX_ERROR_LENGTH      256
#define QUIRK_RESET_ON_CLOSE  1
#define QUIRK_HAS_S_FOLDER    2

/* IS_MODEL(odvr handle, model string): true if h->model eq model string */
#define IS_MODEL(h, m)    \
  (!strncmp(h->model, (m), strlen((m)) + 1))

struct odvr {
  struct usb_device *usb_dev;
  struct usb_dev_handle *usb_handle;
  char   model[MAX_MODEL_LENGTH];
  int    filecount[ODVR_NUM_FOLDERS];
  filestat_t *metadata[ODVR_NUM_FOLDERS][100];
  char   error[MAX_ERROR_LENGTH];
  int    quirks;
  int    trace;
  int    num_folders;
};

char        global_error[MAX_ERROR_LENGTH];

static int set_error(odvr h, char *format, ...){
  va_list args;
  char    *dst;
  int     retc;

  if(h == NULL){
    dst = global_error;
  } else {
    dst = h->error;
  }

  va_start(args, format);
  retc = vsnprintf(dst, MAX_ERROR_LENGTH, format, args);
  va_end(args);

  return retc;
}

static void free_metadata(odvr h){
  int f, i;

  for(f = 0; f < ODVR_NUM_FOLDERS; f++){
    for(i = 0; i < 99 && h->metadata[f][i]; i++){
      free(h->metadata[f][i]);
      h->metadata[f][i] = NULL;
    }
  }
}

/* return usb device for ovdr devices */
static struct usb_device *find_odvr(void){
  struct usb_bus *b;
  struct usb_device *d;

  for(b = usb_get_busses(); b; b = b->next){
    for(d = b->devices; d; d = d->next){
      if(d->descriptor.idVendor == 0x07b4 &&
         d->descriptor.idProduct == 0x020d)
        return d;
    }
  }

  return NULL;
}

void trace_hexdump(const char *label, void *data, int len){
  int i;
  unsigned char *d_idx = (unsigned char *)data;

  fprintf(stderr, "Trace: %s (%d bytes):", label, len);
  for(i = 0; i < len; i++){
    if(!(i % 16))
      fprintf(stderr, "\n");
    fprintf(stderr, "%02X", *d_idx++);
    if(!((i+1) % 2))
      fprintf(stderr, " ");
  }
  fprintf(stderr, "\n");
}

static int try_usb_bulk_write(odvr h, int endpoint, void *packet, int len){
  int retc;

  if(h->trace){
    trace_hexdump("Bulk Write", packet, len);
  }

  retc = usb_bulk_write(h->usb_handle, endpoint, packet, len, 1000);
  if(retc == -ETIMEDOUT){
    set_error(h, "timed out writing to EP%d (device out of sync?)", endpoint);
    return -5;
  }
  if(retc < 0){
    set_error(h, "bulk write of %d bytes to EP%d failed with code %d: %s",
        len, endpoint, retc, usb_strerror());
    return -10;
  }
  if(retc != len){
    set_error(h, "only wrote %d bytes of %d requested to bulk EP%d",
        retc, len, endpoint);
    return -20;
  }

  return retc;
}

static int try_usb_bulk_read(odvr h, int endpoint, void *packet, int len){
  int retc;

  retc = usb_bulk_read(h->usb_handle, endpoint, packet, len, 1000);
  if(retc == -ETIMEDOUT){
    set_error(h, "timed out reading from EP%d (device out of sync?)",
        endpoint);
    return -5;
  }
  if(retc < 0){
    set_error(h, "bulk read of %d bytes to EP%d failed with code %d: %s",
        len, endpoint, retc, usb_strerror());
    return -10;
  }
  if(retc != len){
    set_error(h, "only read %d bytes of %d requested to bulk EP%d",
        retc, len, endpoint);
    return -20;
  }

  if(h->trace){
    trace_hexdump("Bulk Read", packet, len);
  }

  return retc;
}

/* Change state and set flags depending on device.
 * Requires model name to already be known! */
static void detect_quirks(odvr h){
  h->quirks = 0;

  /* These models have a fourth 'S' folder */
  if(IS_MODEL(h, "VN-240PC") ||
     IS_MODEL(h, "VN-480PC") ||
     IS_MODEL(h, "VN-960PC")){
     h->num_folders = 4;
     h->quirks |= QUIRK_HAS_S_FOLDER;
  }
  /* These models have a fourth 'D' folder */
  if(IS_MODEL(h, "2100PC") ||
     IS_MODEL(h, "3100PC") ||
     IS_MODEL(h, "4100PC")){
     h->num_folders = 4;
  }

  /* Somewhere in the Linux kernel, and not libusb, a SET_INTERFACE,
   * with correct parameters, is sent out to the device. This is *BAD* in our
   * situation since the VN-120PC, and possibly up to the VN-480PC has buggy
   * firmware. They will fail if you try to set the alternative interface.
   *
   * Our solution here is to reset the device on close. This will leave buggy
   * devices in a valid state.
   */
  if(IS_MODEL(h, "VN-120PC") ||
     IS_MODEL(h, "VN-240PC") ||
     IS_MODEL(h, "VN-480PC")){
    h->quirks |= QUIRK_RESET_ON_CLOSE;
  }
}

/* open odvr
 * fixme: string errors */
odvr odvr_open(int flags){
  struct odvr *h;
  int          i, errflg;
  char        *errors[] =
    { NULL,
      "couldn't locate USB busses",
      "couldn't locate USB devices",
      "couldn't locate device (unknown product ID?)",
      "couldn't open device",
      "couldn't set configuration",
      "couldn't claim interface",
      "couldn't set alternate interface" };

  if((h = malloc(sizeof(struct odvr))) == NULL)
    return NULL;

  errflg = 0;
  usb_init();
  if(!errflg && usb_find_busses() < 0)
    errflg = 1;
  if(!errflg && usb_find_devices() < 0)
    errflg = 2;
  if(!errflg && (h->usb_dev = find_odvr()) == NULL)
    errflg = 3;
  if(!errflg && (h->usb_handle = usb_open(h->usb_dev)) == NULL)
    errflg = 4;
  if(errflg){
    free(h);
    set_error(NULL, errors[errflg]);
    return NULL;
  }

  if(!errflg && usb_claim_interface(h->usb_handle, 0) < 0)
    errflg = 6;
  if(errflg){
    usb_close(h->usb_handle);
    free(h);
    set_error(NULL, errors[errflg]);
    return NULL;
  }

  /* clear metadata and file counts */
  for(i = 0; i < ODVR_NUM_FOLDERS; i++){
    h->metadata[i][0] = NULL;
    h->filecount[i] = -1;
  }

  if(flags & ODVR_TRACE){
    odvr_enable_trace(h);
  } else {
    odvr_disable_trace(h);
  }

  /* Set the number of folders. Quirks may change this. */
  h->num_folders = 3;

  /* get the model */
  h->model[0] = 0;
  /* HACK HACK HACK: Issue #15
   * Querying the model on a VN-4100PC (and other new models?) can leave the
   * device in a state where the model name isn't returned on even # runs.
   * Workaround: try to get the model name twice. */
  if(odvr_model(h) == NULL)
    odvr_model(h);

  /* determine quirks */
  detect_quirks(h);

  return h;
}

void odvr_close(odvr h){
  if(h->quirks & QUIRK_RESET_ON_CLOSE)
    usb_reset(h->usb_handle);

  usb_close(h->usb_handle);
  free_metadata(h);
  free(h);
}

const char *odvr_error(odvr h){
  if(h)
    return h->error;
  return global_error;
}

int odvr_enable_trace(odvr h){
  h->trace = 1;
  return 0;
}

int odvr_disable_trace(odvr h){
  h->trace = 0;
  return 0;
}

/* reset the USB connection and reconnect
 * fixme: this shouldn't even be needed */
int odvr_reset(odvr h, int flags){
  int i;
  odvr new;

  usb_reset(h->usb_handle);

  for(i = 0; i < 20 && (new = odvr_open(flags)) == NULL; i++){
    usleep(500000);
  }

  if(new == NULL)
    return -1;

  /* copy back new handle
   * note: this can be dangerous if odvr changes! */
  memcpy(h, new, sizeof(struct odvr));
  free(new);

  return 0;
} 

static int valid_status(odvr h, const uint8_t *packet){
  if(! (packet[0] == 0x06 ||
        packet[0] == 0xff ||
        packet[0] == 0x00)){
    set_error(h, "unknown DVR status code");
    return 0;
  }

  return 1;
}

static int cmd_check(odvr h){
  uint8_t packet[8];
  int retc;

  /* fixme: proper interrupt status handling. ex: a thread to handle this */
  packet[0] = 0xff;
  while(packet[0] != 0x00 &&
      (retc =
       usb_interrupt_read(h->usb_handle, 3, (void *)packet, 8, 75)) > 0){
    if(h->trace){
      trace_hexdump("Interrupt Read", packet, 8);
    }

    if(retc != 8){
      set_error(h, "strange status code length");
      return -1;
    }
    if(!valid_status(h, packet))
      return -1;
  }

  if(packet[0] == 0x00 ||
     retc == -ETIMEDOUT){
    /* no status left to pickup */
    return 0;
  }

  if(retc < 0){
    set_error(h, "failed to read DVR status code (%d): %s",
        retc, usb_strerror());
    return -10;
  }

  return -100;
}

static int cmd_check_blocking(odvr h){
  uint8_t packet[8];
  int retc;

  if((retc =
        usb_interrupt_read(h->usb_handle, 3, (void *)packet, 8, 0)) < 0){
    set_error(h, "failed to read status code");
    return -10;
  }
  if(h->trace)
    trace_hexdump("Interrupt Read", packet, 8);

  if(!valid_status(h, packet))
    return -20;

  return 0;
}

static int prepare(odvr h){
  uint8_t packet[64];

  memset(packet, 0, 64);
  packet[0] = 0xf0;

  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return -10;

  return 0;
}

/* note: we cannot rely on h->quirks here */
const char *odvr_model(odvr h){
  uint8_t packet[64];
  char    *loc;

  if(h->model[0]){
    /* model already queried */
    return h->model;
  }

  prepare(h);

  memset(packet, 0, 64);
  packet[0] = 0xb2;
  
  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return NULL;

  if(try_usb_bulk_read(h, 1, packet, 64) < 0)
    return NULL;

  /* null-terminate and copy name */
  packet[63] = 0;
  if((loc = strstr((char *)packet, "PC")) != NULL){
    loc[2] = 0;
    strncpy(h->model, (char *)packet, MAX_MODEL_LENGTH);
  } else {
    snprintf(h->model, MAX_MODEL_LENGTH, "unknown");
  }

  if(cmd_check(h))
    return NULL;

  return h->model;
}

int odvr_filecount(odvr h, uint8_t folder){
  uint8_t packet[64];
  int     num, fidx;

  if(folder < 1 || folder > odvr_foldercount(h))
    return -5;
  fidx = FOLDER_IDX(folder);

  /* cache hit? */
  if(h->filecount[fidx] >= 0)
    return h->filecount[fidx];

  prepare(h);

  memset(packet, 0, 64);
  packet[0] = 0x47;
  packet[1] = folder;

  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return -10;

  if(try_usb_bulk_read(h, 1, packet, 64) < 0)
    return -20;

  num = packet[0];

  if(cmd_check(h))
    return -30;

  h->filecount[fidx] = num;
  return num;
}

int odvr_foldercount(odvr h){
    return h->num_folders;
}

uint8_t odvr_foldercode(odvr h, const char name){
  if(toupper(name) == 'S')
    return odvr_foldercount(h);

  return toupper(name) - 'A' + 1;
}

char odvr_foldername(odvr h, const uint8_t folder){
  if(folder == odvr_foldercount(h) &&
     h->quirks & QUIRK_HAS_S_FOLDER)
    return 'S';

  return folder - 1 + 'A';
}

static int get_folder_metadata(odvr h, uint8_t folder){
  uint8_t  packet[64];
  int      i, num_files, fidx, sidx;

  num_files = odvr_filecount(h, folder);
  if(num_files < 0)
    return -5;
  if(num_files == 0)
    return 0;
 
  /* pre-allocate metadata */
  fidx = FOLDER_IDX(folder);
  for(sidx = 0; sidx < num_files; sidx++){
    if((h->metadata[fidx][sidx] = malloc(sizeof(filestat_t))) == NULL)
      break;
  }
  if(sidx < num_files){
    /* allocation failed */
    while(--sidx >= 0){
      free(h->metadata[fidx][sidx]);
      h->metadata[fidx][sidx] = NULL;
    }
    return -20;
  }
  h->metadata[fidx][sidx] = NULL; /* mark end */

  for(sidx = 0; sidx < num_files; sidx++){
    memset(packet, 0, 64);
    packet[0] = sidx == 0 ? 0x40 : 0x41;
    packet[1] = folder;

    if(try_usb_bulk_write(h, 2, packet, 64) < 0)
      return -30;

    /* read h->metadata */
    if(try_usb_bulk_read(h, 1, packet, 64) < 0)
      return -37;

    /* stuff we know already */
    h->metadata[fidx][sidx]->folder = IDX_FOLDER(fidx);
    h->metadata[fidx][sidx]->slot   = IDX_SLOT(sidx);

    /* file ID */
    h->metadata[fidx][sidx]->id    = *((uint16_t *)(packet + 10));

    /* timestamp */
    h->metadata[fidx][sidx]->month = packet[1];
    h->metadata[fidx][sidx]->day   = packet[2];
    h->metadata[fidx][sidx]->year  = packet[0];
    h->metadata[fidx][sidx]->hour  = packet[3];
    h->metadata[fidx][sidx]->min   = packet[4];
    h->metadata[fidx][sidx]->sec   = packet[5];

    /* Quality */
    h->metadata[fidx][sidx]->quality = packet[8];

    /* dump extra stuff on the floor */
    for(i = 0; i < 5; i++){
      if(try_usb_bulk_read(h, 1, packet, 64) < 0)
        return -40;
    }

    if(cmd_check(h))
      return -45;

    /* last packet had our size */
    h->metadata[fidx][sidx]->size = *((uint16_t *)(packet + 40));
  }

  return 0;
}

/* I don't know what the units are to the size paramter are yet, so we fudge
 * this a little. */
float odvr_length(filestat_t *stat){
  float divisor[] = { 15.74, 8.56, 23.73, 0, 0, 6.03, 2.03, 12.03, 24.16 };

  if(stat->quality >= sizeof(divisor)/sizeof(divisor[0]))
      return 0;
  if(divisor[stat->quality] == 0)
      return 0;

  return stat->size / divisor[stat->quality];
}

uint32_t odvr_wavfilesize(filestat_t *stat){
  float multiplier[] = { 2038, 3748, 1351, 0, 0, 5320, 15805, 2667, 1328 };

  if(stat->quality >= sizeof(multiplier)/sizeof(multiplier[0]))
      return 0;

  return stat->size * multiplier[stat->quality];
}

int odvr_filestat(odvr h, uint8_t folder, uint8_t slot, filestat_t *stat){
  int fidx, sidx, nfiles;
  
  if(folder < 1 || folder > odvr_foldercount(h))
    return -10;

  fidx   = FOLDER_IDX(folder);
  nfiles = odvr_filecount(h, folder);

  sidx = SLOT_IDX(slot);
  if(sidx < 0 || sidx > 99 || slot > nfiles)
    return -20;

  /* there are files, but we haven't got the folder metadata */
  if(h->metadata[fidx][0] == NULL){
    if(get_folder_metadata(h, folder))
      return -30;
  }

  *stat = *h->metadata[fidx][sidx];
  return 0;
}

const char *odvr_quality_name(uint8_t quality){
  const char *quality_names[9] =
    { "SP", "LP", "HQ", NULL, NULL, "SP", "LP", "HQ",
      "XHQ" };

  if(quality >= sizeof(quality_names)/sizeof(quality_names[0])){
    snprintf(global_error, MAX_ERROR_LENGTH, "Unknown (%d)", quality);
    return global_error;
  }

  if(quality_names[quality] == NULL){
    snprintf(global_error, MAX_ERROR_LENGTH, "Unknown (%d)", quality);
    return global_error;
  }

  return quality_names[quality];
}

static void read_header(uint16_t *header, uint16_t *state, uint16_t *cur, uint16_t *last){
  *state = header[0] & 0x03fff;

  *cur   = ((header[1] & 0x0fff) << 2) |
    ((header[0] >> 14) & 0x03);

  *last  = ((header[2] & 0x03ff) << 4) |
    ((header[1] >> 12) & 0x0f);
}

static uint8_t three_shift(uint16_t *ptr){
  uint8_t out;
  uint16_t *a = ptr, *b = ptr+1, *c= ptr+2;

  out = *a & 0x07;
  *a >>= 3;
  *a |=  (*b & 0x07) << 13;
  *b >>= 3;
  *b |=  (*c & 0x07) << 13;
  *c >>= 3;

  return out;
}

static int16_t get_sample(uint8_t nib, uint16_t *state, uint16_t *cur, uint16_t *last){
  int32_t pred;
  int32_t diff;

  /* calculate diff */
  if(nib & 4){
    diff = (*state & ~0x03) - ((nib - 3) << 1) * (*state & ~0x01);
    diff >>= 2;
    diff -= ((nib >> 1) & 1) & (*state & 1);
  } else {
    diff = (*state & ~0x03) + (nib << 1) * *state;
    diff >>= 2;
  }

  /* apply diff and predict */
  pred = (*cur << 1) - *last + diff;
  if(pred > 16383) {
    pred = 16383;
  } else {
    if(pred < 0) {
      pred = 0;
    }
  }

  /* shift sample history */
  *last = *cur;
  *cur = pred;

  /* update state */
  if((nib & 0x03) <= 1)
    *state = *state + (*state >> 3) - (*state >> 2);
  else
    *state = *state + (*state >> 2) + (nib & 1) * (*state >> 1);
  if(*state < 2)
    *state = 2;
  if(*state > 16383)
    *state = 16383;

  /* return signed 16bit PCM sample from computed unsigned 14bit PCM sample */
  return (*cur - 8192) * 4;
}

static int convert_pcm16(uint8_t *data, int len, uint16_t *out,
                         uint8_t quality){
  uint16_t state, cur, last, *stream;
  int      i;

  read_header((void *) data, &state, &cur, &last);
  stream = (uint16_t *)data; /* note: adjusted in first itteration */
  for(i = 0; i < (((len - 6) * 8) / 3); i++){
    /* move to next 3-word chunk */
    if(!(i % 16))
      stream += 3;
    /* 3-bit shift and convert */
    *out++ = get_sample(three_shift(stream), &state, &cur, &last);
  }

  return i;
}

/* returns maximum number of bytes in a block */
int odvr_block_size(odvr h){
  return 2698;
}

/* returns 16bit signed PCM */
int odvr_read_block(odvr h, void *block, int maxbytes, uint8_t quality){
  uint8_t chunk[576 + 6]; /* +6 for shift overrun */
  int     len, nsamples;

  if(maxbytes < odvr_block_size(h)){
    set_error(h, "odvr_read_block() requires a larger block size");
    return -5;
  }

  if(try_usb_bulk_read(h, 1, chunk, 576) < 0)
    return -10;

  len = chunk[0];
  len <<= 8;
  len |= chunk[1];
  len *= 2;

  if(len == 0){
    /* ack the download */
    if(cmd_check(h))
      return -15;
  }

  /* VN-xx00PC XHQ seems to trim 8 bytes off the end */
  if(quality == ODVR_QUALITY_XHQ)
    len -= 8;

  if(len > 0){
    nsamples = convert_pcm16(chunk + 2, len, block, quality);
    if(nsamples < 0)
      return -20;
  } else {
    nsamples = 0;
  }

  return nsamples;
}

int odvr_open_file(odvr h, uint8_t folder, uint8_t slot){
  uint8_t packet[64];

  /* select folder and file */
  memset(packet, 0, 64);
  packet[0] = 0x18; /* select file */
  packet[1] = folder;
  packet[2] = slot;

  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return -10;

  if(try_usb_bulk_read(h, 1, packet, 64) < 0)
    return -20;

  if(cmd_check(h))
    return -25;

  /* download selected file. */
  memset(packet, 0, 64);
  packet[0] = 0x80;

  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return -30;

  if(cmd_check(h))
    return -35;

  return 0;
}

int odvr_save_wav(odvr h, uint8_t folder, uint8_t slot, int fd){
  int16_t    block[4096];
  filestat_t stat;
  int        ns;
  SNDFILE   *out;
  SF_INFO    out_fmt = {
    .channels   = 1,
    .format     = SF_FORMAT_WAV | SF_FORMAT_PCM_16,
  };

  if(odvr_filestat(h, folder, slot, &stat))
    return -10;

  /* Change sample rate based on quality */
  switch(stat.quality){
    case ODVR_QUALITY_HQ:
      out_fmt.samplerate = 16000;
      break;
    case ODVR_QUALITY_SP:
      out_fmt.samplerate = 10600;
      break;
    case ODVR_QUALITY_LP:
      out_fmt.samplerate = 5750;
      break;
    case ODVR_QUALITY_XHQ:
      out_fmt.samplerate = 16000;
      break;
    case ODVR_QUALITY_NEW_SP:
    case ODVR_QUALITY_NEW_LP:
    case ODVR_QUALITY_NEW_HQ:
      set_error(h, "quality unsupported on your device");
      return -13;
    default:
      /* we don't know about this type :( */
      set_error(h, "unknown quality");
      return -15;
  } 

  /* check valid ouput */
  if(!sf_format_check(&out_fmt)){
    set_error(h, "sndfile complained about output format");
    return -20;
  }

  /* open output wav */
  if((out = sf_open_fd(fd, SFM_WRITE, &out_fmt, 0)) == NULL){
    set_error(h, "sndfile failed to open WAV file for writing");
    return -30;
  }

  /* download */
  if(odvr_open_file(h, folder, slot) < 0)
    return -35;
  while((ns = odvr_read_block(h, block,
                              4096 * sizeof(uint16_t), stat.quality)) > 0){
    if(sf_write_short(out, block, ns) != ns){
      set_error(h, "sndfile failed to write complete PCM block");
      sf_close(out);
      return -40;
    }
  }

  /* error in odvr_read_block */
  if(ns < 0)
    return ns;

  sf_close(out);
  return 0;
}

int odvr_clear_folder(odvr h, uint8_t folder){
  uint8_t packet[64];

  memset(packet, 0, 64);
  packet[0] = 0xa0;
  packet[1] = folder;
  packet[2] = 0xff;
  packet[3] = 0xff;
 
  if(prepare(h))
    return -10;

  if(try_usb_bulk_write(h, 2, packet, 64) < 0)
    return -20;

  if(cmd_check_blocking(h))
    return -30;

  return 0;
}

