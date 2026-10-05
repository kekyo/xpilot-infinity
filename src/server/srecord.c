/* 
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 2000-2002 Uoti Urpala <uau@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "xpserver.h"

int   playback = 0;
int   record = 0;
int   *playback_ints;
int   *playback_errnos;
short *playback_shorts;
char  *playback_data;
char  *playback_sched;
int   *playback_ei;
char  *playback_es;
int   *playback_opttout;

int   rrecord;
int   rplayback;
int   recOpt;

enum types {REC_CHAR, REC_INT, REC_SHORT, REC_ERRNO };
#define BUF_INITIALIZER(p,t,s,tr,st,nr) { p, t, s, tr, st, nr }
static struct buf {
    void ** const curp;
    const enum types type;
    const int size;
    const int threshold;
    void *start;
    int num_read;
} bufs[] =
{
    BUF_INITIALIZER((void **)&playback_ints, REC_INT, 5000, 4000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_errnos, REC_ERRNO, 5000, 4000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_shorts, REC_SHORT, 25000, 23000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_data, REC_CHAR, 200000, 100000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_sched, REC_CHAR, 50000, 40000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_ei, REC_INT, 2000, 1000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_es, REC_CHAR, 5000, 4000, NULL, 0),
    BUF_INITIALIZER((void **)&playback_opttout, REC_INT, 2000, 100, NULL, 0)
};

const int num_types = sizeof(bufs) / sizeof(struct buf);

static FILE *recf1;


static void Convert_from_host(void *start, int len, int type)
{
    int *iptr, *iend, err;
    short *sptr, *sendp;

    switch (type) {
    case REC_CHAR:
	return;
    case REC_INT:
	iptr = (int *)start;
	iend = iptr + len / 4;
	while (iptr < iend) {
	    *iptr = htonl(*iptr);
	    iptr++;
	}
	return;
    case REC_SHORT:
	sptr = (short *)start;
	sendp = sptr + len / 2;
	while (sptr < sendp) {
	    *sptr = htons(*sptr);
	    sptr++;
	}
	return;
    case REC_ERRNO:
	iptr = (int *)start;
	iend = iptr + len / 4;
	while (iptr < iend) {
	    err = *iptr;
	    switch (err) {
	    case EAGAIN:
		err = 1;
		break;
	    case EINTR:
		err = 2;
		break;
	    default:
		err = 0;
		break;
	    }
	    *iptr++ = htonl(err);
	}
	return;
    default:
	warn("BUG");
	exit(1);
    }
}
static void Convert_to_host(void *start, int len, int type)
{
    int *iptr, *iend, err;
    short *sptr, *sendp;

    switch (type) {
    case REC_CHAR:
	return;
    case REC_INT:
	iptr = (int *)start;
	iend = iptr + len / 4;
	while (iptr < iend) {
	    *iptr = ntohl(*iptr);
	    iptr++;
	}
	return;
    case REC_SHORT:
	sptr = (short *)start;
	sendp = sptr + len / 2;
	while (sptr < sendp) {
	    *sptr = ntohs(*sptr);
	    sptr++;
	}
	return;
    case REC_ERRNO:
	iptr = (int *)start;
	iend = iptr + len / 4;
	while (iptr < iend) {
	    err = ntohl(*iptr);
	    switch (err) {
	    case 0:
		/* Just some number that isn't tested against anywhere
		 * in the server code. */
		err = ERANGE;
		break;
	    case 1:
		err = EAGAIN;
		break;
	    case 2:
		err = EINTR;
		break;
	    default:
		warn("Unrecognized errno code in recording");
		exit(1);
	    }
	    *iptr++ = err;
	}
	return;
    default:
	warn("BUG");
	exit(1);
    }
}

#define RECSTAT

static void Dump_data(void)
{
    int i, len, len2;

    *playback_sched++ = 127;
#ifdef RECSTAT
    printf("Recording sizes: ");
#endif
    for (i = 0; i < num_types; i++) {
	len = (char *)*bufs[i].curp - (char *)bufs[i].start;
#ifdef RECSTAT
	printf("%d ", len);
#endif
	Convert_from_host(bufs[i].start, len, bufs[i].type);
	len2 = htonl(len);
	fwrite(&len2, 4, 1, recf1);
	fwrite(bufs[i].start, 1, len, recf1);
	*bufs[i].curp = bufs[i].start;
    }
    if (options.recordFlushInterval)
	fflush(recf1);
#ifdef RECSTAT
    printf("\n");
#endif
}

void Get_recording_data(void)
{
    int i, len;

    for (i = 0; i < num_types; i++) {
	if (fread(&len, 4, 1, recf1) < 1) {
	    error("Couldn't read more data (end of file?)");
	    exit(1);
	}
	len = ntohl(len);
	if (len > bufs[i].size - 4) {
	    warn("Incorrect chunk length reading recording");
	    exit(1);
	}
	if ((char*)*bufs[i].curp - (char*)bufs[i].start != bufs[i].num_read) {
	    warn("Recording out of sync");
	    exit(1);
	}
	fread(bufs[i].start, 1, len, recf1);
	bufs[i].num_read = len;
	*bufs[i].curp = bufs[i].start;
	Convert_to_host(bufs[i].start, len, bufs[i].type);
	/* Some of the int buffers must be terminated with INT_MAX */
	if (bufs[i].type == REC_INT)
	    *(int *)((char *)bufs[i].start + len) = INT_MAX;
    }
}

void Init_recording(void)
{
    static int oldMode = 0;
    int i;

    if (!options.recordFileName) {
	if (options.recordMode != 0)
	    warn("Can't do anything with recordings when recordFileName "
		  "isn't specified.");
	options.recordMode = 0;
	return;
    }
    if (sizeof(int) != 4 || sizeof(short) != 2) {
	warn("Recordings won't work on this machine.");
	warn("This code assumes sizeof(int) == 4 && sizeof(short) == 2");
	return;
    }
    if (EWOULDBLOCK != EAGAIN) {
	warn("This system has weird error codes");
	return;
    }

    recOpt = 1; /* Less robust but produces smaller files. */
    if (oldMode == 0) {
	oldMode = options.recordMode + 10;
	if (options.recordMode == 1) {
	    record = rrecord = 1;
	    recf1 = Xp_fopen(options.recordFileName, "wb");
	    if (!recf1) {
		error("Opening record file failed");
		exit(1);
	    }
	    for (i = 0; i < num_types; i++) {
		bufs[i].start = malloc(bufs[i].size);
		*bufs[i].curp = bufs[i].start;
	    }
	    return;
	} else if (options.recordMode == 2) {
	    rplayback = 1;
	    for (i = 0; i < num_types; i++) {
		bufs[i].start = malloc(bufs[i].size);
		*bufs[i].curp = bufs[i].start;
		bufs[i].num_read = 0;
	    }
	    recf1 = Xp_fopen(options.recordFileName, "rb");
	    if (!recf1) {
		error("Opening record file failed");
		exit(1);
	    }
	    Get_recording_data();
	    return;
	} else if (options.recordMode == 0)
	    return;
	else {
	    warn("Trying to start recording or playback when the server is\n"
		  "already running, impossible.");
	    return;
	}
    }
    if (options.recordMode != 0 || oldMode < 11 || oldMode > 12) {
	options.recordMode = oldMode - 10;
	return;
    }
    if (oldMode == 11) {
	Dump_data();
	fclose(recf1);
	oldMode = 10;
    }
    if (oldMode == 12) {
	oldMode = 10;
	warn("End of playback.");
	End_game();
    }
}

void Handle_recording_buffers(void)
{
    int i;
    static time_t t;
    time_t tt;

    if (options.recordMode != 1)
	return;

    tt = time(NULL);
    if (options.recordFlushInterval) {
	if (tt > t + options.recordFlushInterval) {
	    if (t == 0)
		t = tt;
	    else {
		t = tt;
		Dump_data();
		return;
	    }
	}
    }

    for (i = 0; i < num_types; i++)
	if ((char*)*bufs[i].curp - (char*)bufs[i].start > bufs[i].threshold) {
	    t = tt;
	    Dump_data();
	    break;
	}
}
