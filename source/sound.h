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

#ifndef _HEADER_SOUND
#define _HEADER_SOUND

#include <vector>

#include "SDL.h"

#include "res.h"

class Playing_sfx;
class SampleData;

class Sound {
  SDL_AudioSpec spec;
  SDL_AudioDeviceID device;
	std::vector<Playing_sfx*> plays;
	static void audio_callback(void *userdata, Uint8 *stream, int len);
	Sound(const SDL_AudioSpec& _spec, const SDL_AudioDeviceID _device);
public:
  static Sound* New();
  SampleData* normalize(char* _sample, unsigned int _size,
                        unsigned int _freq, unsigned int _bps);
  void start(SampleData* _sam, int _vol, int _pan, int _freq);
	~Sound();
};

class Sample {
  SampleData* data;
	void loadriff(const Res& _res);
public:
	Sample(const Res& re);
	void play(int _vol, int _pan, int _freq);
	~Sample();
};

extern Sound* sound;

#endif
