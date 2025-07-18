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

#include "res.h"

#include <sys/stat.h>

#include "resmanager.h"

Res_doze::Res_doze(const ResName& res) {
	ressize = resmanager->get(res, &_buf);
	if(!_buf)
		fatal_msgbox("Unable to find resource: %s", res.name_.c_str());
}

Res_dos::Res_dos(const char *fil, Res_mode mode) {
	int flag(0);
	_buf = NULL;
	exist = 1;
	switch(mode) {
		case RES_READ:
		case RES_TRY:
		  flag = O_RDONLY;
		  break;
		case RES_WRITE:
		  flag = O_RDWR;
		  break;
		case RES_CREATE:
		  flag = O_CREAT|O_TRUNC|O_RDWR;
		  break;
	}
  #ifdef WIN32
  flag |= O_BINARY;
  #endif
	handle = open(fil, flag, 0666);
	if(handle == -1) {
		if(mode == RES_TRY || mode == RES_CREATE)
			exist = 0;
		else
			fatal_msgbox("Unable to open file '%s'", fil);
  }
}

uint32_t Res_dos::size() const {
  struct stat buf;
  fstat(handle, &buf);
  return buf.st_size;
}

Res_dos::~Res_dos() {
	if(handle != -1)
		close(handle);
	if(_buf)
		delete[] static_cast<uint8_t*>(_buf);
}

void Res_dos::position(uint32_t po) {
	lseek(handle, po, SEEK_SET);
}

int Res_dos::read(void *b, int nb) {
	int n = ::read(handle, b, nb);
	if(n < 0)
		fatal_msgbox("Error reading file");
	return n;
}

void Res_dos::write(const void *b, int nb) {
	if(::write(handle, b, nb) != nb)
		fatal_msgbox("Error writing file");
}

const void* Res_dos::buf() const {
	if(_buf)
		return _buf;
	_buf = new uint8_t[size()];
	if(_buf == NULL)
		fatal_msgbox("Not enough memory");
	if (::read(handle, _buf, size()) < 0)
		fatal_msgbox("error reading file");
	return _buf;
}

bool Res_dos::eof() const {
	return (get_position() >= size()) ? true:false;
}

uint32_t Res_dos::get_position() const {
	return lseek(handle, 0, SEEK_CUR);
}
