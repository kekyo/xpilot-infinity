/*
 * XPilot Infinity, a multiplayer space war game.
 *
 * Copyright (C) 1991-2001 by
 *
 *      The XPilot Authors   <xpilot@xpilot.org>
 *      Juha Lindström       <juhal@users.sourceforge.net>
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
/*
 * OpenAL audio driver.
 */

#include "xpclient.h"
#if defined(_WINDOWS)
#include <al.h>
#include <alut.h>
#else
#include <AL/al.h>
#include <AL/alut.h>
#endif

#define MAX_SOUNDS 16
#define VOL_THRESHOLD 10

typedef struct {
    ALuint    buffer;
    ALfloat   gain;
    ALboolean loop;
} sample_t;

typedef struct sound {
    sample_t     *sample;
    int          volume;
    long         updated;
    ALuint       source;
    struct sound *next;
} sound_t;

static sound_t *ring;
static sound_t *looping;
static sound_t soundinfo[MAX_SOUNDS];
static ALuint  source[MAX_SOUNDS];
static bool audio_device_initialized;


static void sample_parse_info(char *filename, sample_t *sample)
{
    char *token;
    
    sample->gain = 1.0;
    sample->loop = 0;

    strtok(filename, ",");
    if (!(token = strtok(NULL, ","))) return;
    sample->gain = atof(token);
    if (!(token = strtok(NULL, ","))) return;
    sample->loop = atoi(token);
}

static sample_t *sample_load(char *filename)
{
    ALenum    err;
    sample_t  *sample;
    
    if (!(sample = (sample_t*)malloc(sizeof(sample_t)))) {
	error("failed to allocate memory for a sample");
	return NULL;
    }
    sample_parse_info(filename, sample);

    sample->buffer = alutCreateBufferFromFile(filename);
    if (sample->buffer == AL_NONE) {
	err = alutGetError();
	error("failed to load sound file %s: %x %s",
	      filename, err, alutGetErrorString(err));
	free(sample);
	return NULL;
    }

    return sample;
}

static void sample_free(sample_t *sample)
{
    if (sample) {
/* alDeleteBuffers hangs on linux sometimes */
#ifdef _WINDOWS 
	if (sample->buffer)
	    alDeleteBuffers(1, &sample->buffer);
#endif
	free(sample);
    }
}

int audioDeviceInit(char *display)
{
    int    i;
    ALenum err;

    if (!alutInit(NULL, NULL)) {
	err = alutGetError();
	warn("OpenAL sound disabled: %s", alutGetErrorString(err));
	return -1;
    }
    alListenerf(AL_GAIN, 1.0);
    alDopplerFactor(1.0);
    alSpeedOfSound(343.0f);
    alGetError();
    alGenSources(MAX_SOUNDS, source);
    if ((err = alGetError()) != AL_NO_ERROR) {
	error("failed to create sources %x %s", 
	      err, alGetString(err));
	return -1;
    }
    for (i = 0; i < MAX_SOUNDS; i++) {
	soundinfo[i].sample = NULL;
	soundinfo[i].volume = 0;
	soundinfo[i].updated = 0;
	soundinfo[i].source = source[i];
	soundinfo[i].next = &soundinfo[(i + 1) % MAX_SOUNDS];
    }
    ring = soundinfo;
    looping = NULL;
    audio_device_initialized = true;

    return 0;
}

void audioDevicePlay(char *filename, int type, int volume, void **priv)
{
    sound_t  *iter, *next;
    sample_t *sample = (sample_t *)(*priv);

    if (!sample) {
	sample = sample_load(filename);
	if (!sample) {
	    error("failed to load sample %s\n", filename);
	    return;
	}
	*priv = sample;
    }

    /* if the sample is a looping one, first try to find a matching
     * sound from the list of looping sounds already playing. */
    if (sample->loop) {
	for (iter = looping; iter; iter = iter->next) {
	    if (iter->sample == sample
		&& ABS(iter->volume - volume) < VOL_THRESHOLD) {
		alSourcef(iter->source, AL_GAIN, 
			  sample->gain * volume / 100.0f);
		iter->volume = volume;
		iter->updated = loops;
		return;
	    }
	}
    }

    if (ring->next == ring) 
	return; /* only one sound left in the ring */

    /* Pick the next sound from the ring and play the sample with it.
     * If it is a looping sound move it away from the ring to the
     * looping list. Else move it to the end of the ring. */
    next = ring->next;    
    if (sample->loop) {
	ring->next = next->next;
	next->next = looping;
	looping = next;
    } else {
	ring = next;
    }
    
    next->sample  = sample;
    next->volume  = volume;
    next->updated = loops;
	
    alSourcef(next->source, AL_GAIN, sample->gain * volume / 100.0f);
    alSourcei(next->source, AL_BUFFER, sample->buffer);
    alSourcei(next->source, AL_LOOPING, sample->loop);
    alSourcePlay(next->source);
}

void audioDeviceEvents(void)
{
}

void audioDeviceUpdate(void)
{
    sound_t *iter, *prev, *tmp;

    /* Go through the looping list and stop all those sounds
     * that haven't been updated during this frame. The stopped
     * sounds are moved back to the ring. */
    for (prev = NULL, iter = looping; iter;) {
	if (iter->updated < loops - 1) {
	    alSourceStop(iter->source);
	    if (prev) prev->next = iter->next;
	    else looping = iter->next;
	    tmp = iter;
	    iter = iter->next;
	    tmp->next = ring->next;
	    ring->next = tmp;
	} else {
	    prev = iter;
	    iter = iter->next;
	}
    }
}

void audioDeviceFree(void *priv) 
{
    if (priv)
	sample_free((sample_t *)priv);
}

void audioDeviceClose(void)
{
    if (!audio_device_initialized)
	return;

    alDeleteSources(MAX_SOUNDS, source);
#ifdef _WINDOWS /* alutExit hangs on linux sometimes */
    alutExit();
#endif
    audio_device_initialized = false;
}
