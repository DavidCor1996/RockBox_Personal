/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#if !defined(DISABLE_STDIO_FILESTREAM)

// Disable symbol overrides; the host build uses stdio and the embedded build
// has to include Rockbox plugin headers that declare printf-like callbacks.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "backends/fs/stdiostream.h"

#ifdef ROCKBOX_SCUMMVM_EMBEDDED

#include "lib/plugin_cxx_compat.h"

static int rockboxFd(void *handle) {
	return handle ? *(int *)handle : -1;
}

StdioStream::StdioStream(void *handle) : _handle(handle) {
	assert(handle);
}

StdioStream::~StdioStream() {
	int fd = rockboxFd(_handle);
	if (fd >= 0)
		rb->close(fd);
	delete (int *)_handle;
}

bool StdioStream::err() const {
	return false;
}

void StdioStream::clearErr() {
}

bool StdioStream::eos() const {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return true;

	off_t pos = rb->lseek(fd, 0, SEEK_CUR);
	return pos < 0 || pos >= rb->filesize(fd);
}

int32 StdioStream::pos() const {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return -1;
	return rb->lseek(fd, 0, SEEK_CUR);
}

int32 StdioStream::size() const {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return -1;
	return rb->filesize(fd);
}

bool StdioStream::seek(int32 offs, int whence) {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return false;
	return rb->lseek(fd, offs, whence) >= 0;
}

uint32 StdioStream::read(void *ptr, uint32 len) {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return 0;

	ssize_t got = rb->read(fd, ptr, len);
	return got > 0 ? (uint32)got : 0;
}

uint32 StdioStream::write(const void *ptr, uint32 len) {
	int fd = rockboxFd(_handle);
	if (fd < 0)
		return 0;

	ssize_t wrote = rb->write(fd, ptr, len);
	return wrote > 0 ? (uint32)wrote : 0;
}

bool StdioStream::flush() {
	return true;
}

StdioStream *StdioStream::makeFromPath(const Common::String &path, bool writeMode) {
	int fd = rb->open(path.c_str(),
	                  writeMode ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY,
	                  0666);
	if (fd < 0)
		return 0;

	int *handle = new int;
	if (!handle) {
		rb->close(fd);
		return 0;
	}

	*handle = fd;
	return new StdioStream(handle);
}

#else

#include <stdio.h>

StdioStream::StdioStream(void *handle) : _handle(handle) {
	assert(handle);
}

StdioStream::~StdioStream() {
	fclose((FILE *)_handle);
}

bool StdioStream::err() const {
	return ferror((FILE *)_handle) != 0;
}

void StdioStream::clearErr() {
	clearerr((FILE *)_handle);
}

bool StdioStream::eos() const {
	return feof((FILE *)_handle) != 0;
}

int32 StdioStream::pos() const {
	return ftell((FILE *)_handle);
}

int32 StdioStream::size() const {
	int32 oldPos = ftell((FILE *)_handle);
	fseek((FILE *)_handle, 0, SEEK_END);
	int32 length = ftell((FILE *)_handle);
	fseek((FILE *)_handle, oldPos, SEEK_SET);

	return length;
}

bool StdioStream::seek(int32 offs, int whence) {
	return fseek((FILE *)_handle, offs, whence) == 0;
}

uint32 StdioStream::read(void *ptr, uint32 len) {
	return fread((byte *)ptr, 1, len, (FILE *)_handle);
}

uint32 StdioStream::write(const void *ptr, uint32 len) {
	return fwrite(ptr, 1, len, (FILE *)_handle);
}

bool StdioStream::flush() {
	return fflush((FILE *)_handle) == 0;
}

StdioStream *StdioStream::makeFromPath(const Common::String &path, bool writeMode) {
	FILE *handle = fopen(path.c_str(), writeMode ? "wb" : "rb");

#ifdef __amigaos4__
	//
	// Work around for possibility that someone uses AmigaOS "newlib" build
	// with SmartFileSystem (blocksize 512 bytes), leading to buffer size
	// being only 512 bytes. "Clib2" sets the buffer size to 8KB, resulting
	// smooth movie playback. This forces the buffer to be enough also when
	// using "newlib" compile on SFS.
	//
	if (handle && !writeMode) {
		setvbuf(handle, NULL, _IOFBF, 8192);
	}
#endif

#if defined(__WII__)
	// disable newlib's buffering, the device libraries handle caching
	if (handle)
		setvbuf(handle, NULL, _IONBF, 0);
#endif

	if (handle)
		return new StdioStream(handle);
	return 0;
}

#endif

#endif
