/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_BSD_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_BSD_LICENSE_HEADER_END@
 */

#include "Archive.h"
#include "Depot.h"
#include "File.h"
#include "Utils.h"

#include <assert.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char** environ;

Archive::Archive(const char* path) {
	m_serial = 0;
	uuid_generate_random(m_uuid);
	m_path = strdup(path);
	m_name = strdup(basename(m_path));
	m_info = 0;
	m_date_installed = time(NULL);
}

Archive::Archive(uint64_t serial, uuid_t uuid, const char* name, const char* path, uint64_t info, time_t date_installed) {
	m_serial = serial;
	uuid_copy(m_uuid, uuid);
	m_name = name ? strdup(name) : NULL;
	m_path = path ? strdup(path) : NULL;
	m_info = info;
	m_date_installed = date_installed;
}


Archive::~Archive() {
	if (m_path) free(m_path);
	if (m_name) free(m_name);
}

uint64_t	Archive::serial()		{ return m_serial; }
uint8_t*	Archive::uuid()			{ return m_uuid; }
const char*	Archive::name()			{ return m_name; }
const char*	Archive::path()			{ return m_path; }
uint64_t	Archive::info()			{ return m_info; }
time_t		Archive::date_installed()	{ return m_date_installed; }

char* Archive::directory_name(const char* prefix) {
	char* path = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&path, "%s/%s", prefix, uuidstr);
	if (path == NULL) {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
	}
	return path;
}

char* Archive::create_directory(const char* prefix) {
	int res = 0;
	char* path = this->directory_name(prefix);
	IF_DEBUG("creating directory: %s\n", path);
	if (path && res == 0) res = mkdir(path, 0777);
	if (res != 0) {
		fprintf(stderr, "%s:%d: could not create directory: %s: %s (%d)\n", __FILE__, __LINE__, path, strerror(errno), errno);
		free(path);
		path = NULL;
	}
	if (res == 0) res = chown(path, 0, 0);
	return path;
}

int Archive::compact_directory(const char* prefix) {
	int res = 0;
	char* tarpath = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&tarpath, "%s/%s.tar.bz2", prefix, uuidstr);
	IF_DEBUG("compacting %s/%s to %s\n", prefix, uuidstr, tarpath);
	if (tarpath) {
		const char* args[] = {
			"/usr/bin/tar",
			"cjf", tarpath,
			"-C", prefix,
			uuidstr,
			NULL
		};
		res = exec_with_args(args);
		free(tarpath);
	} else {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
		res = -1;
	}
	return res;
}

int Archive::expand_directory(const char* prefix) {
	int res = 0;
	char* tarpath = NULL;
	char uuidstr[37];
	uuid_unparse_upper(m_uuid, uuidstr);
	asprintf(&tarpath, "%s/%s.tar.bz2", prefix, uuidstr);
	IF_DEBUG("expanding %s to %s\n", tarpath, prefix);
	if (tarpath) {
		const char* args[] = {
			"/usr/bin/tar",
			"xjf", tarpath,
			"-C", prefix,
			"-p",	// --preserve-permissions
			NULL
		};
		res = exec_with_args(args);
		free(tarpath);
	} else {
		fprintf(stderr, "%s:%d: out of memory\n", __FILE__, __LINE__);
		res = -1;
	}
	return res;
}


int Archive::extract(const char* destdir) {
	// not implemented
	return -1;
}



RollbackArchive::RollbackArchive() : Archive("<Rollback>") {
	m_info = ARCHIVE_INFO_ROLLBACK;
}



DittoArchive::DittoArchive(const char* path) : Archive(path) {}

int DittoArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/ditto",
		m_path, destdir,
		NULL
	};
	return exec_with_args(args);
}


TarArchive::TarArchive(const char* path) : Archive(path) {}

int TarArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


TarGZArchive::TarGZArchive(const char* path) : Archive(path) {}

int TarGZArchive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xzf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


TarBZ2Archive::TarBZ2Archive(const char* path) : Archive(path) {}

int TarBZ2Archive::extract(const char* destdir) {
	const char* args[] = {
		"/usr/bin/tar",
		"xjf", m_path,
		"-C", destdir,
		NULL
	};
	return exec_with_args(args);
}


Archive* ArchiveFactory(const char* path) {
	Archive* archive = NULL;

	// make sure the archive exists
	struct stat sb;
	int res = stat(path, &sb);
	if (res == -1 && errno == ENOENT) {
		return NULL;
	}

	if (is_directory(path)) {
		archive = new DittoArchive(path);
	} else if (has_suffix(path, ".tar")) {
		archive = new TarArchive(path);
	} else if (has_suffix(path, ".tar.gz") || has_suffix(path, ".tgz")) {
		archive = new TarGZArchive(path);
	} else if (has_suffix(path, ".tar.bz2") || has_suffix(path, ".tbz2")) {
		archive = new TarBZ2Archive(path);
	} else {
		fprintf(stderr, "Error: unknown archive type: %s\n", path);
	}
	return archive;
}
