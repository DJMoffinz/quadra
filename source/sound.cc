/* -*- Mode: C++; c-basic-offset: 2; tab-width: 2; indent-tabs-mode: nil -*-
 * 
 * Quadra, an action puzzle game
 * Copyright (C) 1998-2000  Ludus Design
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "sound.h"

#include <stdio.h>

#include "error.h"
#include "main.h"
#include "types.h"
#include "SDL_endian.h"

#define CHANNELNUMBER 2
#define SAMPLINGRATE 44100
#define AUDIOFORMAT AUDIO_S16
//#define AUDIOFORMAT AUDIO_U8
#define MAXVOICES 8
#define VOLUMESHIFT 2

/* General idea:
 *
 * Sounds are normalized to the format/frequency used by the
 * device when they are loaded. By doing this, we can do all
 * the interpolations that we want in advance.
 */

struct riff_header {
  unsigned int signature;
  unsigned int length;
  unsigned int type;
};

struct chunk_header {
  unsigned int type;
  unsigned int size;
};

struct fmt_chunk {
  unsigned short format;        /* format type */
  unsigned short channels;      /* number of channels
				   (i.e. mono, stereo...) */
  unsigned int sampling;        /* sample rate */
  unsigned int bytespersec;     /* for buffer estimation */
  unsigned short blockalign;    /* block size of data */
  unsigned short bitspersample; /* Number of bits per sample of mono data */
};

Sound *sound = NULL;

class SampleData {
  SDL_atomic_t refcnt;

  ~SampleData() {
    if(audio_data)
      free(audio_data);
  }
  
public:
	void *audio_data;
	unsigned int sampling;
	unsigned int length;

  SampleData(void* _data, unsigned int _freq, unsigned int _len):
    audio_data(_data), sampling(_freq), length(_len) {
    SDL_AtomicSet(&refcnt, 1);
  }
  void ref() {
    SDL_AtomicIncRef(&refcnt);
  }
  void unref() {
    if(SDL_AtomicDecRef(&refcnt))
      delete this;
  }
};

class Playing_sfx {
public:
	SampleData *sam;
	unsigned int vo, f, pos;
	int pa;
	unsigned int delta_inc, delta_position, inc;
	Playing_sfx(SampleData *_sam, int _vol, int _pan, int _freq, int _realfreq);
	virtual ~Playing_sfx();
};

Sound* Sound::New() {
  SDL_AudioSpec wantedspec;
  SDL_AudioSpec spec;

  memset(&wantedspec, 0, sizeof(wantedspec));
  wantedspec.freq = SAMPLINGRATE;
  wantedspec.format = AUDIOFORMAT;
  wantedspec.channels = CHANNELNUMBER;
  wantedspec.callback = Sound::audio_callback;

  SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &wantedspec, &spec, 0);

  if(device <= 1) {
    SDL_Log("SDL_OpenAudio failed: %s", SDL_GetError());
    return NULL;
  }

  if(spec.freq != SAMPLINGRATE)
    SDL_Log("sound: warning, could not set sampling rate");

  if(spec.format != AUDIOFORMAT) {
    SDL_Log("sound: warning, did not get preferred audio format");
  }

  if(spec.format != AUDIO_S16 && spec.format != AUDIO_U8) {
    SDL_Log("sound: unsupported audio format, disabling");
    SDL_CloseAudioDevice(device);
    return NULL;
  }


  if(spec.channels != CHANNELNUMBER)
    SDL_Log("sound: warning, could not set number of channels");

  if(spec.channels != 1 && spec.channels != 2) {
    SDL_Log("sound: neither mono or stereo supported, disabling");
    SDL_CloseAudioDevice(device);
    return NULL;
  }

  return new Sound(spec, device);
}

Sound::Sound(const SDL_AudioSpec& _spec, const SDL_AudioDeviceID _device):
  spec(_spec), device(_device) {
  sound = this;
  // Unpausing the audio starts the callback.
  SDL_PauseAudioDevice(this->device, 0);
  SDL_Log("sound: opened succesfully");
}

void Sound::audio_callback(void *userdata, Uint8 *stream, int len) {
  memset(stream, sound->spec.silence, len);

  unsigned int frag_temp(len);

  if(sound->spec.format == AUDIO_S16)
    frag_temp = frag_temp >> 1;

  if(sound->spec.channels == 2)
    frag_temp = frag_temp >> 1;

  for(unsigned int i = 0; i < (unsigned int)sound->plays.size(); i++) {
    Playing_sfx* p = sound->plays[i];

    if(sound->spec.format == AUDIO_U8) { // if 8-bit output
      uint8_t* output = (uint8_t*)stream;
      uint8_t* input = (uint8_t*)p->sam->audio_data;

      for(unsigned int j=0; j<frag_temp; j++) {
        uint8_t tmpl = *(input + p->pos);
        uint8_t tmpr;

        tmpl = ((int) tmpl * p->vo) >> 8;

        if(sound->spec.channels == 2) { // stereo output
          tmpr = tmpl;
          if(p->pa > 0)
            tmpl = ((int) tmpl * (p->pa)) >> 8;
          else if(p->pa < 0)
            tmpr = ((int) tmpr * (-p->pa)) >> 8;

          *output++ += tmpr;
        }

        *output++ += tmpl;

        if(p->delta_position + p->delta_inc < p->delta_position)
          p->pos++; /* if delta overflows */

        p->delta_position += p->delta_inc;
        p->pos += p->inc;
        if((unsigned int)p->pos >= p->sam->length)
          break;
      }
    } else { // AUDIO_S16 format
      signed short* output = (signed short*)stream;
      signed short* input = (signed short*)p->sam->audio_data;
      signed short w;

      for(unsigned int j=0; j<frag_temp; j++) {
        signed short tmpl = SDL_SwapLE16(*(input + p->pos));
        tmpl = (tmpl * p->vo) >> 8;
        if(sound->spec.channels == 2) { // stereo output
          signed short tmpr = tmpl;
          if(p->pa > 0)
            tmpl = (tmpl * (p->pa)) >> 8;
          else if(p->pa < 0)
            tmpr = (tmpr * (-p->pa)) >> 8;

          w = SDL_SwapLE16(*output);
          *output++ = SDL_SwapLE16(tmpr + w);
        }
        w = SDL_SwapLE16(*output);
        *output++ = SDL_SwapLE16(tmpl + w);
        if(p->delta_position + p->delta_inc < p->delta_position) {
          p->pos++; /* if delta overflows */
        }

        p->delta_position += p->delta_inc;
        p->pos += p->inc;
        if((unsigned int)p->pos >= p->sam->length)
          break;
      }
    }

    if((unsigned int)p->pos >= p->sam->length) {
      sound->plays.erase(sound->plays.begin() + i);
      i--;
      delete p;
    }
  }
}

Sound::~Sound() {
  SDL_CloseAudioDevice(this->device);
}

Sample::Sample(const Res& re):
  data(NULL) {
  if(sound)
    loadriff(re);
}

void Sample::loadriff(const Res& _res) {
	SDL_AudioSpec spec;
	Uint8 *audio_buf;
	Uint32 audio_len;
	SDL_AudioSpec *wav = SDL_LoadWAV_RW(SDL_RWFromConstMem(_res.buf(), _res.size()), true, &spec, &audio_buf, &audio_len);
	if (!wav) {
		SDL_Log("SDL_LoadWAV_RW: Can't load WAVE file: bad file format?");
    exit(1);
	}
	
	data = sound->normalize((char*) audio_buf, audio_len, spec.freq, spec.format);
	SDL_FreeWAV(audio_buf);
}

void Sound::start(SampleData* _sam, int _vol, int _pan, int _freq) {
  if(!_sam)
    return;

  SDL_LockAudioDevice(this->device);

  if(sound->plays.size() < MAXVOICES)
    sound->plays.push_back(new Playing_sfx(_sam, _vol, _pan, _freq, spec.freq));

  SDL_UnlockAudioDevice(this->device);
}

SampleData* Sound::normalize(char* _sample, unsigned int _size,
                             unsigned int _freq, unsigned int _bps) {
	unsigned int length;

  length = (_size * (sound->spec.freq >> 7)) / (_freq >> 7);
  length = (length * (sound->spec.format & 0xff)) / _bps;

	void *audio_data = malloc(length); // length is in bytes here

  if(!audio_data) {
    SDL_Log("couldn't allocate sample");
    exit(1);
  }

  if(_bps == 8) {
    if((sound->spec.format & 0xff) == 16)
      length = length >> 1; // transforms length into a short

    unsigned int pos, inc, delta, delta_pos, old_pos;
    pos = delta_pos = 0;
    old_pos = 1;
    inc = _size / length;
    delta = (unsigned int) (4294967295U / length) * (_size % length);

    for(unsigned int i = 0; i < length; i++) {
      int tube;
      signed short w;
      if(pos == old_pos && ((_bps == 8 && pos < _size - 1) || (_bps == 16 && pos < _size - 1))) {
        if((sound->spec.format & 0xff) == 8) {
          tube = (uint8_t)_sample[pos+1] >> VOLUMESHIFT;
          // cheap interpolation
          tube = (tube+((uint8_t *)audio_data)[i-1]) >> 1;
        } else {
          tube = (128 - (uint8_t)_sample[pos+1]) << (8-VOLUMESHIFT);
          // cheap interpolation
          w = SDL_SwapLE16(((signed short *)audio_data)[i-1]);
          tube = (tube+w) >> 1;
        }
      } else {
        if((sound->spec.format & 0xff) == 8)
          tube = (uint8_t)_sample[pos] >> VOLUMESHIFT;
        else
          tube = (128 - (uint8_t)_sample[pos]) << (8-VOLUMESHIFT);
        old_pos = pos;
      }
      if((sound->spec.format & 0xff) == 8)
        ((uint8_t *)audio_data)[i] = tube;
      else
        ((signed short*)audio_data)[i] = SDL_SwapLE16(tube);

      pos += inc;
      if(delta_pos + delta < delta_pos) // if delta overflows
        pos++;
      delta_pos += delta;
    }
  } else {
    SDL_Log("Sound: wave 16-bit not currently supported");
    exit(1);
  }

  return new SampleData(audio_data, _freq, length);
}

void Sample::play(int _vol, int _pan, int _freq) {
  if(this && sound)
    sound->start(data, _vol, _pan, _freq);
}

Sample::~Sample() {
  if(data)
    data->unref();
}

Playing_sfx::Playing_sfx(SampleData *_sam, int _vol, int _pan, int _freq, int _realfreq):
  sam(_sam), pos(0), delta_position(0) {
  sam->ref();

  if(_vol < -4096)
    _vol = -4096;
  vo = (_vol + 4096) >> 4;

  if(_pan < -4096)
    _pan = -4096;
  if(_pan > 4096)
    _pan = 4096;
  _pan = _pan >> 4;
  if(_pan > 0)
    pa = 256 - _pan;
  else if(_pan < 0)
    pa = -_pan - 256;
  else if(_pan == 0)
    pa = 0;

  _freq = _freq * _realfreq / sam->sampling;
  // we must adjust the asked frequency according the original
  // frequency of the sample
  f = _freq;
  // compute the whole increment
  inc = _freq / _realfreq;
  // then compute the delta increment which will overflow at 2^32
  delta_inc =  (unsigned int) (4294967295U / _realfreq) * (_freq % _realfreq);
}

Playing_sfx::~Playing_sfx() {
  sam->unref();
}

