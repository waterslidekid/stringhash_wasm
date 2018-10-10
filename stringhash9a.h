/*
No copyright is claimed in the United States under Title 17, U.S. Code.
All Other Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

//STRINGHASH9A - a existance check table.. like an expiring bloom filter.. only not.
// uses buckets each with 21 items in it.. it expires 
#ifndef _STRINGHASH9A_H
#define _STRINGHASH9A_H

//#define DEBUG 1
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "evahash64.h"
#include "dprint.h"

//macros
#define SHT9A_ID  "STRINGHASH9A"
#define SHT5_ID   "STRINGHASH5 "
#define SH9A_DEPTH 16 //should be matched with cache line size..

#define SH9A_DIGEST_MASK 0xFFFFFF00U
#define SH9A_DIGEST_MASK2 0x00FFFFFFU
#define SH9A_DIGEST_BITS 24
#define SH9A_DIGEST_SHIFT 8
#define SH9A_DIGEST_DEFAULT 0x00000100U
#define SH9A_LEFTOVER_MASK 0x000000FFU

//macro for specifying uint64_t in a print statement.. architecture dependant..
#ifndef PRIu64
#if __WORDSIZE == 64
#define PRIu64 "lu"
#else
#define PRIu64 "llu"
#endif
#endif

//28 bits of digest, 4 bits of pointer to data
//items in list sorted LRU
typedef struct _sh9a_bucket_t {
     uint32_t digest[SH9A_DEPTH];
} sh9a_bucket_t;

typedef struct _stringhash9a_t {
     sh9a_bucket_t * buckets;
     uint32_t max_records;
     uint64_t mem_used;
     uint32_t ibits;
     uint32_t index_size;
     uint32_t hash_seed;
     uint64_t drops;
     uint8_t epoch;
     uint32_t insert_cnt;
     uint32_t max_insert_cnt;
     uint64_t mask_index;
     uint32_t table_bit;
} stringhash9a_t;

//prototypes
stringhash9a_t * stringhash9a_create(uint32_t);
int stringhash9a_check(stringhash9a_t *, void *, int);
uint64_t stringhash9a_drop_cnt(stringhash9a_t *);
int stringhash9a_set(stringhash9a_t *, void *, int);
int stringhash9a_delete(stringhash9a_t *, void *, int);
void stringhash9a_flush(stringhash9a_t *);
void stringhash9a_destroy(stringhash9a_t *);

#endif // _STRINGHASH9A_H


