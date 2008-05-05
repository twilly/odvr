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
#include <sys/types.h>
#include <string.h>

#include "odvr_date.h"

static gchar *date_format = NULL;

void set_date_format(char *format)
{
    if (date_format)
	g_free(date_format);
    date_format = strdup(format);
}

/* Return a string representation of the date */
gchar *dateString(odvrDate_t *fileDate)
{
#define BUFSIZE 32
    gchar *buf;
    gchar *dateString;

    buf = g_malloc(BUFSIZE);
    dateString = g_malloc(BUFSIZE);
    g_date_strftime(dateString, BUFSIZE, date_format, &fileDate->date);
    if (buf && dateString)
	g_snprintf(buf, BUFSIZE, "%s-%02d:%02d:%02d",
		   dateString,
		   fileDate->hour, 
		   fileDate->min, 
		   fileDate->sec);
    g_free(dateString);
    return buf;
}

gint datecmp(odvrDate_t *date1, odvrDate_t *date2)
{
    return g_date_compare(&date1->date, &date2->date);
}

gint timecmp(odvrDate_t *date1, odvrDate_t *date2)
{
    if (datecmp(date1, date2) != 0)
	return datecmp(date1, date2);
    if (date1->hour != date2->hour)
	return date1->hour - date2->hour;
    if (date1->min != date2->min)
	return date1->min - date2->min;
    return date1->sec - date2->sec;
}

void datecpy(odvrDate_t *date1, odvrDate_t *date2)
{
    date1->date = date2->date;
    date1->hour = date2->hour;
    date1->min = date2->min;
    date1->sec = date2->sec;
}
