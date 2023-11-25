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

//Some modifications by Red Hat any bug is probably our fault

//-----------------------------------------------------------------------------
// MurmurHash2A, by Austin Appleby

// This is a variant of MurmurHash2 modified to use the Merkle-Damgard
// construction. Bulk speed should be identical to Murmur2, small-key speed
// will be 10%-20% slower due to the added overhead at the end of the hash.

// This variant fixes a minor issue where null keys were more likely to
// collide with each other than expected, and also makes the algorithm
// more amenable to incremental implementations. All other caveats from
// MurmurHash2 still apply.

#ifndef __MURMUR_HASH2A_H
#define __MURMUR_HASH2A_H

#include <windef.h>
#include "os_dep.h"

typedef UINT32 uint32_t;
typedef UINT16 uint16_t;
typedef UINT8 uint8_t;

#define mmix(h,k) { k *= m; k ^= k >> r; k *= m; h *= m; h ^= k; }

_inline uint32_t MurmurHash2A(const void * key, uint32_t len, uint32_t seed )
{
    const uint32_t m = 0x5bd1e995;
    const uint32_t r = 24;
    uint32_t l = len;
    uint32_t t = 0;

    const uint8_t * data = (const uint8_t *)key;

    uint32_t h = seed;

    while (len >= 4) {
        uint32_t k = *(uint32_t*)data;

        mmix(h,k);

        data += 4;
        len -= 4;
    }

    switch (len) {
    case 3: t ^= data[2] << 16;
    case 2: t ^= data[1] << 8;
    case 1: t ^= data[0];
    };

    mmix(h,t);
    mmix(h,l);

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

_inline uint32_t MurmurHash2AJump3(const uint32_t * key, uint32_t len, uint32_t seed )
{
    uint32_t m = 0x5bd1e995;
    uint32_t r = 24;
    uint32_t l = len << 2;

    const uint8_t * data = (const uint8_t *)key;

    uint32_t h = seed;

    while (len >= 4) {
        uint32_t k = *(uint32_t*)data;
        uint32_t tmp;

        data += 4;
        tmp = *(uint32_t *)data;
        k = k << 8;
        k |= (uint8_t)tmp;
        mmix(h,k);

        k = tmp << 8;
        k = k & 0xffff0000;
        data += 4;
        tmp = *(uint32_t *)data;
        k |= (uint16_t)(tmp >> 8);
        mmix(h,k);

        data += 4;
        k = *(uint32_t *)data;
        k = k << 8;
        k |=  (uint8_t)tmp;
        mmix(h,k);

        data += 4;
        len -= 4;
    }

    while (len >= 1) {
        uint32_t k = *(uint32_t*)data;

        k = k << 8;
        mmix(h,k);

        data += 4;
        len--;
    }

    h *= m;
    mmix(h,l);

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}


_inline uint32_t murmurhash2a(const void *key, size_t length, uint32_t initval)
{
    return MurmurHash2A(key, length, initval);
}

_inline uint32_t murmurhash2ajump3(const uint32_t *key, size_t length, uint32_t initval)
{
    return MurmurHash2AJump3(key, length, initval);
}
#endif

