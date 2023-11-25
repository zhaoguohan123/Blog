/*
   Copyright (C) 2009 Red Hat, Inc.

   This software is licensed under the GNU General Public License,
   version 2 (GPLv2) (see COPYING for details), subject to the
   following clarification.

   With respect to binaries built using the Microsoft(R) Windows
   Driver Kit (WDK), GPLv2 does not extend to any code contained in or
   derived from the WDK ("WDK Code").  As to WDK Code, by using or
   distributing such binaries you agree to be bound by the Microsoft
   Software License Terms for the WDK.  All WDK Code is considered by
   the GPLv2 licensors to qualify for the special exception stated in
   section 3 of GPLv2 (commonly known as the system library
   exception).

   There is NO WARRANTY for this software, express or implied,
   including the implied warranties of NON-INFRINGEMENT, TITLE,
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*/

#ifndef __QUIC_H
#define __QUIC_H

#include "quic_config.h"

typedef enum {
    QUIC_IMAGE_TYPE_INVALID,
    QUIC_IMAGE_TYPE_GRAY,
    QUIC_IMAGE_TYPE_RGB16,
    QUIC_IMAGE_TYPE_RGB24,
    QUIC_IMAGE_TYPE_RGB32,
    QUIC_IMAGE_TYPE_RGBA
} QuicImageType;

#define QUIC_ERROR -1
#define QUIC_OK 0

typedef void *QuicContext;

typedef struct QuicUsrContext QuicUsrContext;
struct QuicUsrContext {
    void (*error)(QuicUsrContext *usr, const char *fmt, ...);
    void (*warn)(QuicUsrContext *usr, const char *fmt, ...);
    void (*info)(QuicUsrContext *usr, const char *fmt, ...);
    void *(*malloc)(QuicUsrContext *usr, int size);
    void (*free)(QuicUsrContext *usr, void *ptr);
    int (*more_space)(QuicUsrContext *usr, uint32_t **io_ptr, int rows_completed);
    int (*more_lines)(QuicUsrContext *usr, uint8_t **lines); // on return the last line of previous
                                                             // lines bunch must stil be valid
};

int quic_encode(QuicContext *quic, QuicImageType type, int width, int height,
                uint8_t *lines, unsigned int num_lines, int stride,
                uint32_t *io_ptr, unsigned int num_io_words);

int quic_decode_begin(QuicContext *quic, uint32_t *io_ptr, unsigned int num_io_words,
                      QuicImageType *type, int *width, int *height);
int quic_decode(QuicContext *quic, QuicImageType type, uint8_t *buf, int stride);


QuicContext *quic_create(QuicUsrContext *usr);
void quic_destroy(QuicContext *quic);

void quic_init();

#endif

