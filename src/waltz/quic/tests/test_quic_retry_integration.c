#include "../fd_quic.h"
#include "fd_quic_test_helpers.h"
#include "../../../util/net/fd_ip4.h"

#include <stdio.h>
#include <stdlib.h>

int
my_stream_rx_cb( fd_quic_conn_t * conn,
                 ulong            stream_id,
                 ulong            offset,
                 uchar const *    data,
                 ulong            data_sz,
                 int              fin ) {
  (void)conn;
  FD_LOG_DEBUG(( "server rx stream data stream=%lu size=%lu offset=%lu",
                 stream_id, data_sz, offset ));
  FD_TEST( fd_ulong_is_aligned( offset, 512UL ) );
  FD_LOG_HEXDUMP_DEBUG(( "received data", data, data_sz ));

  FD_TEST( data_sz==512UL );
  FD_TEST( !fin );
  FD_TEST( 0==memcmp( data, "Hello world", 11u ) );
  return FD_QUIC_SUCCESS;
}

int server_complete = 0;
int client_complete = 0;

/* server connection received in callback */
fd_quic_conn_t * server_conn = NULL;

void
my_connection_new( fd_quic_conn_t * conn,
                   void *           vp_context ) {
  (void)vp_context;

  FD_LOG_NOTICE(( "server handshake complete" ));

  server_complete = 1;
  server_conn = conn;
}

void
my_handshake_complete( fd_quic_conn_t * conn,
                       void *           vp_context ) {
  (void)conn;
  (void)vp_context;

  FD_LOG_NOTICE(( "client handshake complete" ));

  client_complete = 1;
}


/* global "clock" */
ulong now = 123;

ulong test_clock( void * ctx ) {
  (void)ctx;
  return now;
}

int
main( int argc, char ** argv ) {
  fd_boot          ( &argc, &argv );
  fd_quic_test_boot( &argc, &argv );

  fd_rng_t _rng[1]; fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, 0U, 0UL ) );

  ulong cpu_idx = fd_tile_cpu_id( fd_tile_idx() );
  if( cpu_idx>fd_shmem_cpu_cnt() ) cpu_idx = 0UL;

  char const * _page_sz  = fd_env_strip_cmdline_cstr ( &argc, &argv, "--page-sz",   NULL, "gigantic"                   );
  ulong        page_cnt  = fd_env_strip_cmdline_ulong( &argc, &argv, "--page-cnt",  NULL, 1UL                          );
  ulong        numa_idx  = fd_env_strip_cmdline_ulong( &argc, &argv, "--numa-idx",  NULL, fd_shmem_numa_idx( cpu_idx ) );

  ulong page_sz = fd_cstr_to_shmem_page_sz( _page_sz );
  if( FD_UNLIKELY( !page_sz ) ) FD_LOG_ERR(( "unsupported --page-sz" ));

  FD_LOG_NOTICE(( "Creating workspace (--page-cnt %lu, --page-sz %s, --numa-idx %lu)", page_cnt, _page_sz, numa_idx ));
  fd_wksp_t * wksp = fd_wksp_new_anonymous( page_sz, page_cnt, fd_shmem_cpu_idx( numa_idx ), "wksp", 0UL );
  FD_TEST( wksp );

  fd_quic_limits_t const quic_limits = {
    .conn_cnt           = 10,
    .conn_id_cnt        = 10,
    .handshake_cnt      = 10,
    .stream_id_cnt      = 10,
    .stream_pool_cnt    = 400,
    .inflight_frame_cnt = 1024 * 10,
    .tx_buf_sz          = 1<<14
  };

  ulong quic_footprint = fd_quic_footprint( &quic_limits );
  FD_TEST( quic_footprint );
  FD_LOG_NOTICE(( "QUIC footprint: %lu bytes", quic_footprint ));

  FD_LOG_NOTICE(( "Creating server QUIC" ));
  fd_quic_t * server_quic = fd_quic_new_anonymous( wksp, &quic_limits, FD_QUIC_ROLE_SERVER, rng );
  FD_TEST( server_quic );

  FD_LOG_NOTICE(( "Creating client QUIC" ));
  fd_quic_t * client_quic = fd_quic_new_anonymous( wksp, &quic_limits, FD_QUIC_ROLE_CLIENT, rng );
  FD_TEST( client_quic );

  server_quic->cb.now              = test_clock;
  server_quic->cb.conn_new         = my_connection_new;
  server_quic->cb.stream_rx        = my_stream_rx_cb;
  server_quic->config.retry = 1;

  client_quic->cb.now              = test_clock;
  client_quic->cb.conn_hs_complete = my_handshake_complete;

  server_quic->config.initial_rx_max_stream_data = 1<<16;
  client_quic->config.initial_rx_max_stream_data = 1<<16;

  FD_LOG_NOTICE(( "Creating virtual pair" ));
  fd_quic_virtual_pair_t vp;
  fd_quic_virtual_pair_init( &vp, server_quic, client_quic );

  FD_LOG_NOTICE(( "Initializing QUICs" ));
  FD_TEST( fd_quic_init( server_quic ) );
  FD_TEST( fd_quic_init( client_quic ) );

  FD_LOG_NOTICE(( "Creating connection" ));
  fd_quic_conn_t * client_conn = fd_quic_connect( client_quic, 0U, 0, 0U, 0 );
  FD_TEST( client_conn );

  /* do general processing */
  for( ulong j = 0; j < 20; j++ ) {
    FD_LOG_INFO(( "running services" ));
    fd_quic_service( client_quic );
    fd_quic_service( server_quic );

    if( server_complete && client_complete ) {
      FD_LOG_INFO(( "***** both handshakes complete *****" ));
      break;
    }
  }

  FD_TEST( client_quic->metrics.conn_created_cnt== 1 );
  FD_TEST( client_quic->metrics.conn_retry_cnt  == 0 );
  FD_TEST( server_quic->metrics.conn_created_cnt== 1 );
  FD_TEST( server_quic->metrics.conn_retry_cnt  == 1 );
  /* Server: Retry, Initial, Handshake
     Client: Initial, Initial, Handshake */
  FD_TEST( client_quic->metrics.net_rx_pkt_cnt  == 3 );
  FD_TEST( client_quic->metrics.net_tx_pkt_cnt  == 3 );
  FD_TEST( server_quic->metrics.net_rx_pkt_cnt  == 3 );
  FD_TEST( server_quic->metrics.net_tx_pkt_cnt  == 3 );
  FD_TEST( client_quic->metrics.net_tx_byte_cnt > server_quic->metrics.net_tx_byte_cnt );

  /* TODO we get callback before the call to fd_quic_conn_new_stream can complete
     must delay until the conn->state is ACTIVE */

  FD_LOG_NOTICE(( "Creating streams" ));

  fd_quic_stream_t * client_stream   = fd_quic_conn_new_stream( client_conn );
  FD_TEST( client_stream );

  fd_quic_stream_t * client_stream_0 = fd_quic_conn_new_stream( client_conn );
  FD_TEST( client_stream_0 );

  FD_LOG_NOTICE(( "Sending data over streams" ));

  char buf[512] = "Hello world!\x00-   ";

  for( unsigned j = 0; j < 16; ++j ) {
    FD_LOG_INFO(( "running services" ));

    fd_quic_service( client_quic );
    fd_quic_service( server_quic );

    buf[12] = ' ';
    //buf[15] = (char)( ( j / 10 ) + '0' );
    buf[16] = (char)( ( j % 10 ) + '0' );
    int rc = 0;
    if( j&1 ) {
      rc = fd_quic_stream_send( client_stream,   buf, sizeof(buf), 0 );
    } else {
      rc = fd_quic_stream_send( client_stream_0, buf, sizeof(buf), 0 );
    }

    FD_LOG_INFO(( "fd_quic_stream_send returned %d", rc ));
  }

  FD_LOG_NOTICE(( "Closing connections" ));

  fd_quic_conn_close( client_conn, 0 );
  fd_quic_conn_close( server_conn, 0 );

  FD_LOG_NOTICE(( "Waiting for ACKs" ));

  for( uint j=0; j<10U; ++j ) {
    FD_LOG_INFO(( "running services" ));
    fd_quic_service( client_quic );
    fd_quic_service( server_quic );
  }

  FD_TEST( client_quic->metrics.conn_created_cnt== 1 );

  FD_LOG_NOTICE(( "Cleaning up" ));
  fd_quic_virtual_pair_fini( &vp );
  fd_wksp_free_laddr( fd_quic_delete( fd_quic_leave( fd_quic_fini ( server_quic ) ) ) );
  fd_wksp_free_laddr( fd_quic_delete( fd_quic_leave( fd_quic_fini ( client_quic ) ) ) );
  fd_wksp_delete_anonymous( wksp );
  fd_rng_delete( fd_rng_leave( rng ) );

  FD_LOG_NOTICE(( "pass" ));
  fd_quic_test_halt();
  fd_halt();
  return 0;
}
