#if !FD_HAS_HOSTED
#error "This target requires FD_HAS_HOSTED"
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../util/fd_util.h"
#include "../../util/sanitize/fd_fuzz.h"
#include "fd_blake3.h"

int
LLVMFuzzerInitialize( int  *   argc,
                      char *** argv ) {
  /* Set up shell without signal handlers */
  putenv( "FD_LOG_BACKTRACE=0" );
  fd_boot( argc, argv );
  fd_log_level_core_set(3); /* crash on warning log */
  atexit( fd_halt );
  return 0;
}

int
LLVMFuzzerTestOneInput( uchar const * data,
                        ulong         size ) {
  // hash single message
  char const * msg = ( char const * ) data;

  uchar hash1[ 32 ] __attribute__((aligned(32)));
  uchar hash2[ 32 ] __attribute__((aligned(32)));

  fd_blake3_t sha[1];
  assert( fd_blake3_init( sha ) == sha );
  assert( fd_blake3_append( sha, msg, size ) == sha );
  assert( fd_blake3_fini( sha, hash1 ) == hash1 );
  assert( fd_blake3_init( sha ) == sha );
  assert( fd_blake3_append( sha, msg, size ) == sha );
  assert( fd_blake3_fini( sha, hash2 ) == hash2 );
  assert( !memcmp( hash1, hash2, 32UL ) );

  FD_FUZZ_MUST_BE_COVERED;
  return 0;
}
