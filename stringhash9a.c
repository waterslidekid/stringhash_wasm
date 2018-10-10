/* 
   compile using:
   emcc stringhash9a.c -o sh9.js -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS="['_stringhash9a_create','_stringhash9a_set', '_stringhash9a_check','_stringhash9a_destroy','_main','_malloc']" -s EXTRA_EXPORTED_RUNTIME_METHODS="['lengthBytesUTF8', 'stringToUTF8', 'writeArrayToMemory']" -O2

*/

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

//#define DEBUG 1
#include "stringhash9a.h"

int main () {
     stringhash9a_t * sht = stringhash9a_create(4 * 1000000);
     //stringhash9a_t * sht = stringhash9a_create(20000000);
     if (!sht) {
          printf("unable to allocate\n");
          return -1;
     }
     printf("strh9 max records %u\n", sht->max_records);
     printf("strh9 mem used %"PRIu64"\n", sht->mem_used);
     printf("hash result %d\n", stringhash9a_set(sht, "foo", 3));
     printf("hash result %d\n", stringhash9a_set(sht, "bar", 3));
     printf("hash result %d\n", stringhash9a_set(sht, "barf", 4));
     printf("hash result %d\n", stringhash9a_set(sht, "foo", 3));
     printf("hash result %d\n", stringhash9a_set(sht, "boo", 3));
     printf("hash result %d\n", stringhash9a_set(sht, "barf", 4));


     uint32_t i;
     uint32_t collisions = 0;
#define MAX_TEST (1000000) 
     for (i = 0 ; i < MAX_TEST; i++) {
            if (stringhash9a_set(sht, &i, 4)) {
                 collisions++;
            }
     }
     printf("collisions %u, %.5f\n", collisions, (double)collisions * 100 / MAX_TEST );

     uint32_t missing = 0;
     for (i = 0 ; i < MAX_TEST; i++) {
          uint32_t j = i + MAX_TEST/2;
          if (!stringhash9a_check(sht, &j, 4)) {
               missing++;
          }
     }
     printf("missing %u, %.5f\n", missing, (double)missing * 100 / MAX_TEST );

     return 0;
}

//compute log2 of an unsigned int
// by Eric Cole - http://graphics.stanford.edu/~seander/bithacks.htm
uint32_t sh9a_uint32_log2(uint32_t v) {
     static const int MultiplyDeBruijnBitPosition[32] =
     {
          0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
          31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
     };

     v |= v >> 1; // first round down to power of 2
     v |= v >> 2;
     v |= v >> 4;
     v |= v >> 8;
     v |= v >> 16;
     v = (v >> 1) + 1;

     return MultiplyDeBruijnBitPosition[(uint32_t)(v * 0x077CB531U) >> 27];

}

int check_sh9a_max_records(uint32_t max_records) {

     //note the minimum table size for sh9a
     if(max_records < 84) {
          dprint("Caution: minimum size for sh9a max_records is 84...resizing\n");
          // the minimum size of 84 will be enforced by the sh9a_create_ibits() below
     }

     // 42 == 21 items per bucket, 2 tables 
     uint32_t ibits = sh9a_uint32_log2((uint32_t)(max_records/42)) + 1;
     int mxr = (1<<(ibits)) * 21 * 2;

     return mxr;
}



stringhash9a_t * sh9a_create_ibits(uint32_t ibits) {
     stringhash9a_t * sht;
     sht = (stringhash9a_t *)calloc(1, sizeof(stringhash9a_t));
     if (!sht) {
          dprint("failed calloc of stringhash9a hash table");
          return NULL;
     }

     sht->ibits = ibits;
     sht->index_size = 1<<(ibits);
     dprint("index size %u", sht->index_size);
     sht->max_insert_cnt = sht->index_size >> 4;
     sht->table_bit = 1<<(ibits);

     dprint("table bit %u", sht->table_bit);
     sht->mask_index = ((uint64_t)~0)>>(64-(ibits));
     dprint("maskindex %"PRIu64, sht->mask_index);
     sht->max_records = sht->index_size * 21 * 2;

     sht->hash_seed = (uint32_t)rand();
     sht->epoch = 1;

     // now to allocate memory...
     sht->buckets = (sh9a_bucket_t *)calloc(sht->index_size * 2,
                                            sizeof(sh9a_bucket_t));

     if (!sht->buckets) {
          free(sht);
          dprint("failed calloc of stringhash9a buckets");
          return NULL;
     }

     // tally up the hash table memory use
     sht->mem_used = sizeof(stringhash9a_t) + 2 * (uint64_t)sht->index_size * sizeof(sh9a_bucket_t);

     return sht;
}

stringhash9a_t * stringhash9a_create(uint32_t max_records) {
     stringhash9a_t * sht;

     //note the minimum table size for sh9a
     if(max_records < 84) {
          dprint("Caution: minimum size for sh9a max_records is 84...resizing\n");
          // the minimum size of 84 will be enforced by the sh9a_create_ibits() below
     }

     //create the stringhash9a table from scratch
     // 42 == 21 items per bucket, 2 tables 
     uint32_t ibits = sh9a_uint32_log2((uint32_t)(max_records/42)) + 1;

     sht = sh9a_create_ibits(ibits);
     if (!(sht)) {
          dprint("stringhash9a_create failed");
          return NULL;
     }

     return sht;
}


//steal bytes from digests.. populate
#define sh9a_build_leftover(i, lookup, digest) do { \
    if (i <= 14) { \
	const uint32_t partial = (digest) & SH9A_LEFTOVER_MASK; \
	const unsigned int shift = 8 * (((i) % 3) + 1); \
	const unsigned int idx = (i)/3; \
	(lookup)[idx] |= partial << shift; \
    } \
} while (0)

//could be optimized a bit more for loop unrolling
void sh9a_sort_lru_lower(uint32_t * d, uint8_t mru) {
     uint32_t a = d[mru] & SH9A_DIGEST_MASK;

#define SH9A_SET_DIGEST(X,Y) X = ((X & SH9A_LEFTOVER_MASK) | (Y & SH9A_DIGEST_MASK))
     switch(mru) {
     case 15:
          SH9A_SET_DIGEST(d[15],d[14]);
     case 14:
          SH9A_SET_DIGEST(d[14],d[13]);
     case 13:
          SH9A_SET_DIGEST(d[13],d[12]);
     case 12:
          SH9A_SET_DIGEST(d[12],d[11]);
     case 11:
          SH9A_SET_DIGEST(d[11],d[10]);
     case 10:
          SH9A_SET_DIGEST(d[10],d[9]);
     case 9:
          SH9A_SET_DIGEST(d[9],d[8]);
     case 8:
          SH9A_SET_DIGEST(d[8],d[7]);
     case 7:
          SH9A_SET_DIGEST(d[7],d[6]);
     case 6:
          SH9A_SET_DIGEST(d[6],d[5]);
     case 5:
          SH9A_SET_DIGEST(d[5],d[4]);
     case 4:
          SH9A_SET_DIGEST(d[4],d[3]);
     case 3:
          SH9A_SET_DIGEST(d[3],d[2]);
     case 2:
          SH9A_SET_DIGEST(d[2],d[1]);
     case 1:
          SH9A_SET_DIGEST(d[1],d[0]);
     }

     d[0] &= SH9A_LEFTOVER_MASK;
     d[0] |= a;
}
/*
void print_lru(uint32_t * d, uint32_t m) {
     fprintf(stdout, "%u", m);
     uint32_t i;
     for (i = 0; i < 15; i++) {
          fprintf(stdout, " %x", (d[i] & SH9A_DIGEST_MASK)>>8);
     }
     for (i = 0; i < 5; i++) {
          uint32_t u = (d[i*3] & SH9A_LEFTOVER_MASK);
          u |= (d[(i*3)+1] & SH9A_LEFTOVER_MASK) << 8;
          u |= (d[(i*3)+2] & SH9A_LEFTOVER_MASK) << 16;
          fprintf(stdout, " %x", u);
     }
     fprintf(stdout, "\n");
}*/

//move mru item to front..  ridiculous code bloat - but it's supposed
// to loop unravel
void sh9a_sort_lru_upper(uint32_t * d, uint8_t mru) {
     uint32_t a = 0;
     uint32_t i;

     switch (mru) {
     case 16:
          a = ((d[0] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[1] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[2] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 17:
          a = ((d[3] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[4] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[5] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 18:
          a = ((d[6] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[7] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[8] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 19:
          a = ((d[9] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[10] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[11] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 20:
          a = ((d[12] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[13] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[14] & SH9A_LEFTOVER_MASK)<<24);
          break;
     }

     switch (mru) {
     case 20:
          d[12] &= SH9A_DIGEST_MASK;
          d[12] |= d[9] & SH9A_LEFTOVER_MASK;
          d[13] &= SH9A_DIGEST_MASK;
          d[13] |= d[10] & SH9A_LEFTOVER_MASK;
          d[14] &= SH9A_DIGEST_MASK;
          d[14] |= d[11] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 19:
          d[9] &= SH9A_DIGEST_MASK;
          d[9] |= d[6] & SH9A_LEFTOVER_MASK;
          d[10] &= SH9A_DIGEST_MASK;
          d[10] |= d[7] & SH9A_LEFTOVER_MASK;
          d[11] &= SH9A_DIGEST_MASK;
          d[11] |= d[8] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 18:
          d[6] &= SH9A_DIGEST_MASK;
          d[6] |= d[3] & SH9A_LEFTOVER_MASK;
          d[7] &= SH9A_DIGEST_MASK;
          d[7] |= d[4] & SH9A_LEFTOVER_MASK;
          d[8] &= SH9A_DIGEST_MASK;
          d[8] |= d[5] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 17:
          d[3] &= SH9A_DIGEST_MASK;
          d[3] |= d[0] & SH9A_LEFTOVER_MASK;
          d[4] &= SH9A_DIGEST_MASK;
          d[4] |= d[1] & SH9A_LEFTOVER_MASK;
          d[5] &= SH9A_DIGEST_MASK;
          d[5] |= d[2] & SH9A_LEFTOVER_MASK;
          //no break;
     }

     uint32_t b = d[15];
     for (i = 15; i > 0; i--) {
          d[i] &= SH9A_LEFTOVER_MASK;
          d[i] |= d[i-1] & SH9A_DIGEST_MASK;
     }
     d[0] &= SH9A_DIGEST_MASK;
     d[0] |= (b & 0xFF00)>>8;
     d[1] &= SH9A_DIGEST_MASK;
     d[1] |= (b & 0xFF0000)>>16;
     d[2] &= SH9A_DIGEST_MASK;
     d[2] |= (b & 0xFF000000)>>24;

     d[0] &= SH9A_LEFTOVER_MASK;
     d[0] |= a; 
}

// GCC pragma diagnostics only work for gcc 4.2 and later
#define GCC_VERSION (  __GNUC__       * 10000  \
                                              + __GNUC_MINOR__ *   100   \
                                              + __GNUC_PATCHLEVEL__)
#if GCC_VERSION >= 40200
#pragma GCC diagnostic ignored "-Wuninitialized"
#endif
#undef GCC_VERSION

//given an index and bucket.. find state data...
int sh9a_lookup_bucket(sh9a_bucket_t * bucket,
                                     uint32_t digest) {
     int i;

     uint32_t * dp = bucket->digest;
     uint32_t leftover[5] = {0};
     for (i = 0 ; i < SH9A_DEPTH; i++) {
          if (digest == (dp[i] & SH9A_DIGEST_MASK)) {
               sh9a_sort_lru_lower(dp, i);
               return 1;
          }
          sh9a_build_leftover(i, leftover, dp[i]);
     }
     for (i = 0; i < 5; i++) {
          if (digest == leftover[i]) {
               sh9a_sort_lru_upper(dp, i+16);
               return 1;
          }
     }
     return 0;
}

// lookup bucket.. count zeros in bucket
int sh9a_lookup_bucket2(sh9a_bucket_t * bucket,
                                      uint32_t digest, uint32_t * zeros) {
     int i;

     *zeros = 0;
     uint32_t * dp = bucket->digest;
     uint32_t leftover[5] = {0};
     uint32_t dcmp;
     for (i = 0 ; i < SH9A_DEPTH; i++) {
          dcmp = dp[i] & SH9A_DIGEST_MASK;
          if (digest == dcmp) {
               sh9a_sort_lru_lower(dp, i);
               return 1;
          }
          *zeros += dcmp ? 0 : 1;
          sh9a_build_leftover(i, leftover, dp[i]);
     }
     for (i = 0; i < 5; i++) {
          if (digest == leftover[i]) {
               sh9a_sort_lru_upper(dp, i+16);
               return 1;
          }
          *zeros += leftover[i] ? 0 : 1;
     }
     return 0;
}

#define SH9A_PERMUTE1 0xed31952d18a569ddULL
#define SH9A_PERMUTE2 0x94e36ad1c8d2654bULL
void sh9a_gethash(stringhash9a_t * sht,
                                uint8_t * key, uint32_t keylen,
                                uint32_t *h1, uint32_t *h2,
                                uint32_t *pd1, uint32_t *pd2) {

     dprint("trying to hash %.*s", keylen, key);
     uint64_t m = evahash64(key, keylen, sht->hash_seed);
     uint64_t p1 = m * SH9A_PERMUTE1;
     uint64_t p2 = m * SH9A_PERMUTE2;
     uint64_t lh1, lh2;
     lh1 = (p1 >> SH9A_DIGEST_BITS) & sht->mask_index;
     lh2 = ((p2 >> SH9A_DIGEST_BITS) & sht->mask_index) | sht->table_bit;
     *h1 = (uint32_t)lh1;
     *h2 = (uint32_t)lh2;

     uint32_t d1, d2;
     d1 = (uint32_t)(p1 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;
     d2 = (uint32_t)(p2 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;

     //make sure digest not zero - if so, set to default
     *pd1 = d1 ? d1 : SH9A_DIGEST_DEFAULT;
     *pd2 = d2 ? d2 : SH9A_DIGEST_DEFAULT;
}

void sh9a_gethash2(stringhash9a_t * sht,
                                 uint8_t * key, uint32_t keylen,
                                 uint32_t *h1, uint32_t *h2,
                                 uint32_t *pd1, uint32_t *pd2,
                                 uint64_t *hash) {

     uint64_t m = evahash64(key, keylen, sht->hash_seed);
     *hash = m;
     uint64_t p1 = m * SH9A_PERMUTE1;
     uint64_t p2 = m * SH9A_PERMUTE2;
     uint64_t lh1, lh2;
     lh1 = (p1 >> SH9A_DIGEST_BITS) & sht->mask_index;
     lh2 = ((p2 >> SH9A_DIGEST_BITS) & sht->mask_index) | sht->table_bit;
     *h1 = (uint32_t)lh1;
     *h2 = (uint32_t)lh2;

     uint32_t d1, d2;
     d1 = (uint32_t)(p1 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;
     d2 = (uint32_t)(p2 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;

     //make sure digest not zero - if so, set to default
     *pd1 = d1 ? d1 : SH9A_DIGEST_DEFAULT;
     *pd2 = d2 ? d2 : SH9A_DIGEST_DEFAULT;
}

void sh9a_gethash3(stringhash9a_t * sht,
                                 uint64_t hash,
                                 uint32_t *h1, uint32_t *h2,
                                 uint32_t *pd1, uint32_t *pd2) {

     uint64_t m = hash;
     uint64_t p1 = m * SH9A_PERMUTE1;
     uint64_t p2 = m * SH9A_PERMUTE2;
     uint64_t lh1, lh2;
     lh1 = (p1 >> SH9A_DIGEST_BITS) & sht->mask_index;
     lh2 = ((p2 >> SH9A_DIGEST_BITS) & sht->mask_index) | sht->table_bit;
     *h1 = (uint32_t)lh1;
     *h2 = (uint32_t)lh2;

     uint32_t d1, d2;
     d1 = (uint32_t)(p1 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;
     d2 = (uint32_t)(p2 & SH9A_DIGEST_MASK2) << SH9A_DIGEST_SHIFT;

     //make sure digest not zero - if so, set to default
     *pd1 = d1 ? d1 : SH9A_DIGEST_DEFAULT;
     *pd2 = d2 ? d2 : SH9A_DIGEST_DEFAULT;
}

void sh9a_shift_new(uint32_t * d, uint32_t a) {

     d[12] &= SH9A_DIGEST_MASK;
     d[12] |= d[9] & SH9A_LEFTOVER_MASK;
     d[13] &= SH9A_DIGEST_MASK;
     d[13] |= d[10] & SH9A_LEFTOVER_MASK;
     d[14] &= SH9A_DIGEST_MASK;
     d[14] |= d[11] & SH9A_LEFTOVER_MASK;

     d[9] &= SH9A_DIGEST_MASK;
     d[9] |= d[6] & SH9A_LEFTOVER_MASK;
     d[10] &= SH9A_DIGEST_MASK;
     d[10] |= d[7] & SH9A_LEFTOVER_MASK;
     d[11] &= SH9A_DIGEST_MASK;
     d[11] |= d[8] & SH9A_LEFTOVER_MASK;

     d[6] &= SH9A_DIGEST_MASK;
     d[6] |= d[3] & SH9A_LEFTOVER_MASK;
     d[7] &= SH9A_DIGEST_MASK;
     d[7] |= d[4] & SH9A_LEFTOVER_MASK;
     d[8] &= SH9A_DIGEST_MASK;
     d[8] |= d[5] & SH9A_LEFTOVER_MASK;

     d[3] &= SH9A_DIGEST_MASK;
     d[3] |= d[0] & SH9A_LEFTOVER_MASK;
     d[4] &= SH9A_DIGEST_MASK;
     d[4] |= d[1] & SH9A_LEFTOVER_MASK;
     d[5] &= SH9A_DIGEST_MASK;
     d[5] |= d[2] & SH9A_LEFTOVER_MASK;

     uint32_t b = d[15];
     uint32_t i;
     for (i = 15; i > 0; i--) {
          d[i] &= SH9A_LEFTOVER_MASK;
          d[i] |= d[i-1] & SH9A_DIGEST_MASK;
     }
     d[0] &= SH9A_DIGEST_MASK;
     d[0] |= (b & 0xFF00)>>8;
     d[1] &= SH9A_DIGEST_MASK;
     d[1] |= (b & 0xFF0000)>>16;
     d[2] &= SH9A_DIGEST_MASK;
     d[2] |= (b & 0xFF000000)>>24;

     d[0] &= SH9A_LEFTOVER_MASK;
     d[0] |= a; 
}

int stringhash9a_check_posthash(stringhash9a_t * sht,
                                              uint32_t h1, uint32_t h2,
                                              uint32_t d1, uint32_t d2) {
     if (sh9a_lookup_bucket(&sht->buckets[h1], d1)) {
          return 1;
     }
     if (sh9a_lookup_bucket(&sht->buckets[h2], d2)) {
          return 1;
     }
     return 0;
     
}

//find records using hashkeys.. return 1 if found
int stringhash9a_check(stringhash9a_t * sht,
                                     void * key, int keylen) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash(sht, (uint8_t*)key, keylen, &h1, &h2, &d1, &d2);

     return stringhash9a_check_posthash(sht, h1, h2, d1, d2);
}

//find records using hashkeys.. return 1 if found
int stringhash9a_check_gethash(stringhash9a_t * sht,
                                             void * key, int keylen,
                                             uint64_t * phash) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash2(sht, (uint8_t*)key, keylen, &h1, &h2, &d1, &d2, phash);

     return stringhash9a_check_posthash(sht, h1, h2, d1, d2);
}

//find records using hashkeys.. return 1 if found
int stringhash9a_check_hash(stringhash9a_t * sht,
                                          uint64_t hash) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash3(sht, hash, &h1, &h2, &d1, &d2);

     return stringhash9a_check_posthash(sht, h1, h2, d1, d2);
}

int sh9a_cmp_epoch(stringhash9a_t * sht, uint32_t h1, uint32_t h2,
                                 uint32_t d1) {
     uint8_t e1, e2;
     uint8_t diff1, diff2;

     e1 = sht->buckets[h1].digest[15] & SH9A_LEFTOVER_MASK;
     diff1 = sht->epoch - e1;
     e2 = sht->buckets[h2].digest[15] & SH9A_LEFTOVER_MASK;
     diff2 = sht->epoch - e2; 
    
     //find oldest table 
     if (diff1 > diff2) {
          return 1;
     }
     else if (diff2 < diff1) {
          return 0;
     }
     //in the case of a tie, try to choose randomly
     else {
          if (d1 & 0x1) {
               return 1;
          }
          else {
               return 0;
          }
     }
     //return (diff1 >= diff2) ? 1 : 0;
}

void sh9a_update_bucket_epoch(stringhash9a_t * sht, sh9a_bucket_t * bucket) {
     sht->insert_cnt++;
     if (sht->insert_cnt > sht->max_insert_cnt) {
          sht->insert_cnt = 0;
          sht->epoch++;
     }
     bucket->digest[15] &= SH9A_DIGEST_MASK;
     bucket->digest[15] |= (uint32_t)sht->epoch;
}


int stringhash9a_set_posthash(stringhash9a_t * sht,
                                            uint32_t h1, uint32_t h2,
                                            uint32_t d1, uint32_t d2) {
     uint32_t zeros1, zeros2;

     if (sh9a_lookup_bucket2(&sht->buckets[h1], d1, &zeros1) ||
         sh9a_lookup_bucket2(&sht->buckets[h2], d2, &zeros2)) {
          dprint("found in bucket");
          return 1;
     }

     sh9a_bucket_t * bucket;

     //if zeros.. do normal d-left balance
     if (zeros1 > zeros2) {
          bucket = &sht->buckets[h1];
          sh9a_shift_new(bucket->digest, d1);
     }
     else if (zeros1 < zeros2) {
          bucket = &sht->buckets[h2];
          sh9a_shift_new(bucket->digest, d2);
     }
     else if (zeros1) { /// its a tie
          bucket = &sht->buckets[h1];
          sh9a_shift_new(bucket->digest, d1);
     }
     else {
          //ok we have to drop an item
          sht->drops++;

          if (sh9a_cmp_epoch(sht, h1, h2, d1)) {
               bucket = &sht->buckets[h1];
               sh9a_shift_new(bucket->digest, d1);
          }
          else {
               bucket = &sht->buckets[h2];
               sh9a_shift_new(bucket->digest, d2);
          }
     }

     sh9a_update_bucket_epoch(sht, bucket); 

     return 0;
}

uint64_t stringhash9a_drop_cnt(stringhash9a_t * sht) {
     return sht->drops;
}

//find records using hashkeys.. return 1 if found
int stringhash9a_set(stringhash9a_t * sht,
                                   void * key, int keylen) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash(sht, (uint8_t*)key, keylen, &h1, &h2, &d1, &d2);

     dprint("%u %u %u %u", h1, h2, d1 ,d2);

     return stringhash9a_set_posthash(sht, h1, h2, d1, d2);

}

//find records using hashkeys.. return 1 if found
int stringhash9a_set_gethash(stringhash9a_t * sht,
                                           void * key, int keylen,
                                           uint64_t * phash) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash2(sht, (uint8_t*)key, keylen, &h1, &h2, &d1, &d2, phash);

     return stringhash9a_set_posthash(sht, h1, h2, d1, d2);

}

//find records using hashkeys.. return 1 if found
int stringhash9a_set_hash(stringhash9a_t * sht,
                                        uint64_t hash) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash3(sht, hash, &h1, &h2, &d1, &d2);

     return stringhash9a_set_posthash(sht, h1, h2, d1, d2);
}



//move mru item to front.. for lower 16 items in a bucket
void sh9a_sort_lru_lower_half(uint32_t * d, uint8_t mru) {
     uint32_t a;
     uint32_t i;
     uint32_t x = (mru > 10) ? (mru - 10) : 0;
     a = d[mru] & SH9A_DIGEST_MASK;
     for (i = mru; i > x; i--) {
          d[i] &= SH9A_LEFTOVER_MASK;
          d[i] |= d[i-1] & SH9A_DIGEST_MASK;
     }
     d[x] &= SH9A_LEFTOVER_MASK;
     d[x] |= a;
}


//move mru item to front..  ridiculous code bloat - but it's supposed
// to perform better than straightforward looping..
void sh9a_sort_lru_upper_half(uint32_t * d, uint8_t mru) {
     uint32_t a;
     uint32_t i;

     switch (mru) {
     case 16:
          a = ((d[0] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[1] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[2] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 17:
          a = ((d[3] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[4] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[5] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 18:
          a = ((d[6] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[7] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[8] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 19:
          a = ((d[9] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[10] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[11] & SH9A_LEFTOVER_MASK)<<24);
          break;
     case 20:
          a = ((d[12] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[13] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[14] & SH9A_LEFTOVER_MASK)<<24);
          break;
     }

     switch (mru) {
     case 20:
          d[12] &= SH9A_DIGEST_MASK;
          d[12] |= d[9] & SH9A_LEFTOVER_MASK;
          d[13] &= SH9A_DIGEST_MASK;
          d[13] |= d[10] & SH9A_LEFTOVER_MASK;
          d[14] &= SH9A_DIGEST_MASK;
          d[14] |= d[11] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 19:
          d[9] &= SH9A_DIGEST_MASK;
          d[9] |= d[6] & SH9A_LEFTOVER_MASK;
          d[10] &= SH9A_DIGEST_MASK;
          d[10] |= d[7] & SH9A_LEFTOVER_MASK;
          d[11] &= SH9A_DIGEST_MASK;
          d[11] |= d[8] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 18:
          d[6] &= SH9A_DIGEST_MASK;
          d[6] |= d[3] & SH9A_LEFTOVER_MASK;
          d[7] &= SH9A_DIGEST_MASK;
          d[7] |= d[4] & SH9A_LEFTOVER_MASK;
          d[8] &= SH9A_DIGEST_MASK;
          d[8] |= d[5] & SH9A_LEFTOVER_MASK;
          //nobreak;
     case 17:
          d[3] &= SH9A_DIGEST_MASK;
          d[3] |= d[0] & SH9A_LEFTOVER_MASK;
          d[4] &= SH9A_DIGEST_MASK;
          d[4] |= d[1] & SH9A_LEFTOVER_MASK;
          d[5] &= SH9A_DIGEST_MASK;
          d[5] |= d[2] & SH9A_LEFTOVER_MASK;
          //no break;
     }

     uint32_t x = mru - 10;
     uint32_t b = d[15];
     for (i = 15; i > x; i--) {
          d[i] &= SH9A_LEFTOVER_MASK;
          d[i] |= d[i-1] & SH9A_DIGEST_MASK;
     }
     d[0] &= SH9A_DIGEST_MASK;
     d[0] |= (b & 0xFF00)>>8;
     d[1] &= SH9A_DIGEST_MASK;
     d[1] |= (b & 0xFF0000)>>16;
     d[2] &= SH9A_DIGEST_MASK;
     d[2] |= (b & 0xFF000000)>>24;

     d[x] &= SH9A_LEFTOVER_MASK;
     d[x] |= a; 
}

//move mru item to front..  ridiculous code bloat - but it's supposed
// to loop unravel
void sh9a_delete_lru(uint32_t * d, uint8_t item) {
     uint32_t i;
     if (item < 16) {
          for (i = item; i < 15; i++) {
               d[i] &= SH9A_LEFTOVER_MASK;
               d[i] = d[i+1] & SH9A_DIGEST_MASK;
          }
          d[15] = ((d[0] & SH9A_LEFTOVER_MASK)<<8) + 
               ((d[1] & SH9A_LEFTOVER_MASK)<<16) +
               ((d[2] & SH9A_LEFTOVER_MASK)<<24);

          item = 16;
     }
     switch (item) {
     case 16:
          d[0] &= SH9A_DIGEST_MASK;
          d[0] |= d[3] & SH9A_LEFTOVER_MASK;
          d[1] &= SH9A_DIGEST_MASK;
          d[1] |= d[4] & SH9A_LEFTOVER_MASK;
          d[2] &= SH9A_DIGEST_MASK;
          d[2] |= d[5] & SH9A_LEFTOVER_MASK;
          //no break;
     case 17:
          d[3] &= SH9A_DIGEST_MASK;
          d[3] |= d[6] & SH9A_LEFTOVER_MASK;
          d[4] &= SH9A_DIGEST_MASK;
          d[4] |= d[7] & SH9A_LEFTOVER_MASK;
          d[5] &= SH9A_DIGEST_MASK;
          d[5] |= d[8] & SH9A_LEFTOVER_MASK;
          //no break;
     case 18:
          d[6] &= SH9A_DIGEST_MASK;
          d[6] |= d[9] & SH9A_LEFTOVER_MASK;
          d[7] &= SH9A_DIGEST_MASK;
          d[7] |= d[10] & SH9A_LEFTOVER_MASK;
          d[8] &= SH9A_DIGEST_MASK;
          d[8] |= d[11] & SH9A_LEFTOVER_MASK;
          //no break;
     case 19:
          d[9] &= SH9A_DIGEST_MASK;
          d[9] |= d[12] & SH9A_LEFTOVER_MASK;
          d[10] &= SH9A_DIGEST_MASK;
          d[10] |= d[13] & SH9A_LEFTOVER_MASK;
          d[11] &= SH9A_DIGEST_MASK;
          d[11] |= d[14] & SH9A_LEFTOVER_MASK;
          //no break;
     }

     //delete last element..
     d[12] &= SH9A_DIGEST_MASK;
     d[13] &= SH9A_DIGEST_MASK;
     d[14] &= SH9A_DIGEST_MASK;
}

//given an index and digest.. find state data...
int sh9a_delete_bucket(sh9a_bucket_t * bucket,
                                    uint32_t digest) {
     uint32_t i;
     
     uint32_t * dp = bucket->digest;
     uint32_t leftover[6] = {0};
     for (i = 0 ; i < SH9A_DEPTH; i++) {
          if (digest == (dp[i] & SH9A_DIGEST_MASK)) {
               sh9a_delete_lru(bucket->digest, i);
               return 1;
          }
          sh9a_build_leftover(i, leftover, digest);
     }
     for (i = 0; i < 5; i++) {
          if (digest == leftover[i]) {
               sh9a_delete_lru(bucket->digest, i+16);
               return 1;
          }
     }
     return 0;

}


int stringhash9a_delete(stringhash9a_t * sht,
                                      void * key, int keylen) {

     uint32_t h1, h2;
     uint32_t d1, d2;

     sh9a_gethash(sht, (uint8_t*)key, keylen, &h1, &h2, &d1, &d2);

     //lookup in digest.. location1
     if (sh9a_delete_bucket(&sht->buckets[h1], d1)) {
          return 1;
     }

     if (sh9a_delete_bucket(&sht->buckets[h2], d2)) {
          return 1;
     }

     return 0;
}

void stringhash9a_flush(stringhash9a_t * sht) {
     memset(sht->buckets, 0, sizeof(sh9a_bucket_t) * (uint64_t)sht->index_size * 2);
     sht->epoch = 1;
}

void stringhash9a_destroy(stringhash9a_t * sht) {

     uint64_t expire_cnt = stringhash9a_drop_cnt(sht);
     if (expire_cnt) {
          dprint("sh9a table expire cnt %"PRIu64, expire_cnt);
     }
     free(sht->buckets);
     free(sht);
}


