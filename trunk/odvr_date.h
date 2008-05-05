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
#ifndef ODVR_DATE_H
#define ODVR_DATE_H

#include <stdint.h>
#include <gtk/gtk.h>
#include <glib.h>

typedef struct odvrDate {
    GDate    date;
  uint8_t  hour;
  uint8_t  min;
  uint8_t  sec;
} odvrDate_t;


void set_date_format(char *format);
/* Return a string representation of the date */
gchar *dateString(odvrDate_t *date);
/* Compare dates */
gint datecmp(odvrDate_t *date1, odvrDate_t *date2);
/* Compare date and times */
gint timecmp(odvrDate_t *date1, odvrDate_t *date2);
void datecpy(odvrDate_t *date1, odvrDate_t *date2);

#endif /* ODVR_DATE_H */
