#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/file.h>
#include <string.h>
#include <inttypes.h>

#include <fcntl.h>

#include "dbstructs.h"

#define MODULE_NAME "dbpciaddr"
#define MODULE_DIR_NAME "/sys/kernel/debug/"
#define MODULE_CONTROLLER_NAME "ctrlargs"

#define code_address_space 'a'
#define code_pci_dev 'p'
#define code_unknown 'u'

int fileno( FILE *stream );

enum struct_type {
  ST_ADDRESS_SPACE,
  ST_PCI_DEV,
  ST_UNKNOWN
};

static const enum struct_type struct_codes[] = {
  [ code_address_space ] = ST_ADDRESS_SPACE,
  [ code_pci_dev ]       = ST_PCI_DEV,
  [ code_unknown ]       = ST_UNKNOWN
};

static size_t get_length( u32 number ) {
  return snprintf( NULL, 0, "%u", number );
}

typedef void (struct_printer)( void* ptr_struct );

static void print_address_space( void* addr_space );
static void print_pci_dev( void* dev );

static struct_printer* struct_printers[] = {
  [ ST_ADDRESS_SPACE ] = print_address_space,
  [ ST_PCI_DEV ] = print_pci_dev
};
 
int main( int argc, char** argv ) {

  if ( access( MODULE_DIR_NAME MODULE_NAME, F_OK ) != 0 ) {
    fprintf( stderr, "module " MODULE_NAME " not loaded, please check it status with [ lsmod | grep " MODULE_NAME " ]\n" );
    return -1;
  }

  if ( argc < 2 ) {
    fprintf( stderr, "incompatible parameters count\n" );
    return -1;
  }

  const char raw_struct = argv[ 1 ][ 0 ];
  const enum struct_type stype = struct_codes[ raw_struct ];
  char* srcblob_path = NULL;

  size_t buffer_length = 0;
  u32 pid = 1;
  u32 fd = 0;
  u32 vendor = PCI_ANY_ID;
  u32 device = PCI_ANY_ID;
  size_t read_params = 0;

  if ( stype == ST_ADDRESS_SPACE ) {
    read_params = sscanf( argv[ 2 ], "%u:%u", &pid, &fd );
    srcblob_path = MODULE_DIR_NAME MODULE_NAME "/address_space";
    fprintf( stderr, "read %zu params -- [ pid=%u; l=%zu, fd=%u; l=%zu ]\n", read_params, pid, get_length( pid ), fd, get_length( fd ) );
    buffer_length += 3 + get_length( pid ) + get_length( fd );
  } else if ( stype == ST_PCI_DEV ) {
    read_params = sscanf( argv[ 2 ], "%x:%x", &vendor, &device );
    srcblob_path = MODULE_DIR_NAME MODULE_NAME "/pci_dev";
    fprintf( stderr, "read %zu params -- [ vendor=%u, device=%u ]\n", read_params, vendor, device );
    buffer_length += 3 + get_length( vendor ) + get_length( device );
  } else {
    fprintf( stderr, "incompatible struct type\n" );
    return -1;
  }

  fprintf( stderr, "blob will be read from \"%s\"\n", srcblob_path );

  /* write args start */
  struct flock lk_ctrlargs;
  FILE* ctrlargs = fopen( MODULE_DIR_NAME MODULE_NAME "/" MODULE_CONTROLLER_NAME, "w" );
  if ( ctrlargs == NULL ) {
    fprintf( stderr, "cannot open controller file\n" );
    return -1;
  }
  int fd_ctrlargs = fileno( ctrlargs );
  fcntl( fd_ctrlargs, LOCK_EX, &lk_ctrlargs );

  char buffer[ buffer_length ];
  size_t check_sum = 0;

  switch ( stype ) {
    case ST_ADDRESS_SPACE:
      check_sum += snprintf( buffer, buffer_length, "a%u:%u\n", pid, fd );
      break;
    case ST_PCI_DEV:
      check_sum += snprintf( buffer, buffer_length, "p%u:%u\n", vendor, device );
      break;
    default:
      break;
  }
  fprintf( stderr, "data successfully formed [ buffer=\"%s\", length=%zu ]\n", buffer, buffer_length );

  if ( check_sum == buffer_length ) {
    size_t check_count = fwrite( buffer, buffer_length, 1, ctrlargs );
    fprintf( stderr, "written %zu objects\n", check_count );
  }

  // fcntl( fd_ctrlargs, LOCK_UN, &lk_ctrlargs );
  fclose( ctrlargs );
  /* write args finish */

  /* read blob start */
  struct flock lk_blob;
  FILE* blob_source = fopen( srcblob_path, "rb" );
  if ( blob_source == NULL ) {
    fprintf( stderr, "cannot open blob source\n" );
    // fcntl( fd_ctrlargs, LOCK_UN, &lk_ctrlargs );
    fclose( ctrlargs );
    return -1; 
  }
  int fd_blob_source = fileno( blob_source );
  fcntl( fd_blob_source, LOCK_EX, &lk_blob );

  struct pci_dev dev;
  struct address_space addr_space;
  size_t check_blob_count = 0;
  void* on_print = NULL;
  switch ( stype ) {
    case ST_ADDRESS_SPACE:
      check_blob_count += fread( &addr_space, sizeof( struct address_space ), 1, blob_source );
      on_print = &addr_space;
      break;
    case ST_PCI_DEV:
      check_blob_count += fread( &dev, sizeof( struct pci_dev ), 1, blob_source );
      on_print = &dev;
      break;
    default:
      break;
  }

  fcntl( fd_blob_source, LOCK_UN, &lk_blob );
  fclose( blob_source );
  /* read blob finish */

  /* output results */
  struct_printers[ stype ]( on_print );
  return 0;  
}

static void print_address_space( void* addr_space ) {
  if ( addr_space == NULL ) return;
  struct address_space address_space = *( ( struct address_space* ) addr_space );
  printf( "address_space.a_ops:\t%p\n", address_space.a_ops );
  printf( "address_space.flags:\t%lx\n", address_space.flags );
  printf( "address_space.gfp_mask:\t%" PRIu32 "\n", address_space.gfp_mask );
  printf( "address_space.host:\t%p\n", address_space.host );
  printf( "address_space.i_mmap.rb_root.rb_node:\t%p\n", ( void* ) address_space.i_mmap.rb_root.rb_node );
  printf( "address_space.i_mmap.rb_leftmost:\t%p\n", ( void* ) address_space.i_mmap.rb_leftmost );
  printf( "address_space.i_mmap_writable.counter:\t%i\n", address_space.i_mmap_writable.counter );
  printf( "address_space.i_mmap_writable.nrpages:\t%" PRIu64 "\n", address_space.nrpages );
  printf( "address_space.i_mmap_writable.private_data:\t%p\n", address_space.private_data );
  printf( "address_space.i_mmap_writable.private_list.prev:\t%p\n", ( void* ) address_space.private_list.prev );
  printf( "address_space.i_mmap_writable.private_list.next:\t%p\n", ( void* ) address_space.private_list.next );
  printf( "address_space.i_mmap_writable.wb_err:\t%" PRIu32 "\n", address_space.wb_err );
  printf( "address_space.i_mmap_writable.writeback_index:\t%" PRIu64 "\n", address_space.writeback_index );
}

static void print_pci_dev( void* dev ) {
  if ( dev == NULL ) return;
  struct pci_dev pcid = *( ( struct pci_dev* ) dev );
  printf( "pcid.vendor:\t%" PRIu32 "\n", pcid.vendor );
  printf( "pcid.device:\t%" PRIu32 "\n", pcid.device );
  printf( "pcid.acs_cap:\t%" PRIu16 "\n", pcid.acs_cap );
  printf( "pcid.ari_enabled:\t%" PRIu32 "\n", pcid.ari_enabled );
  printf( "pcid.block_cfg_access:\t%" PRIu32 "\n", pcid.block_cfg_access );
  printf( "pcid.bridge_d3:\t%" PRIu32 "\n", pcid.bridge_d3 );
  printf( "pcid.broken_intx_masking:\t%" PRIu32 "\n", pcid.broken_intx_masking );
  printf( "pcid.broken_parity_status:\t%" PRIu32 "\n", pcid.broken_parity_status );
  printf( "pcid.bus:\t%p\n", pcid.bus );
  printf( "pcid.bus_list.next:\t%p\n", ( void* ) pcid.bus_list.next );
  printf( "pcid.bus_list.prev:\t%p\n", ( void* ) pcid.bus_list.prev );
  printf( "pcid.cfg_size:\t%" PRIi32 "\n", pcid.cfg_size );
  printf( "pcid.class:\t%" PRIu32 "\n", pcid.class );
  printf( "pcid.clear_retrain_link:\t%" PRIu32 "\n", pcid.clear_retrain_link );
  printf( "pcid.current_state:\t%" PRIi32 "\n", pcid.current_state );
  printf( "pcid.d1_support:\t%" PRIu32 "\n", pcid.d1_support );
  printf( "pcid.d2_support:\t%" PRIu32 "\n", pcid.d2_support );
  printf( "pcid.d3cold_allowed:\t%" PRIu32 "\n", pcid.d3cold_allowed );
  printf( "pcid.d3cold_delay:\t%" PRIu32 "\n", pcid.d3cold_delay );
  printf( "pcid.d3hot_delay:\t%" PRIu32 "\n", pcid.d3hot_delay );
  printf( "pcid.dev_flags:\t%x\n", pcid.dev_flags );
  printf( "pcid.devcap:\t%" PRIu32 "\n", pcid.devcap );
  printf( "pcid.devfn:\t%" PRIu32 "\n", pcid.devfn );
  printf( "pcid.dma_alias_mask:\t%p\n", ( void* ) pcid.dma_alias_mask );
  printf( "pcid.dma_mask:\t%" PRIu64 "\n", pcid.dma_mask );
  printf( "pcid.driver:\t%p\n", pcid.driver );
  if ( pcid.driver_override != NULL )
    printf( "pcid.driver_override:\t%s\n", pcid.driver_override );
  printf( "pcid.irq:\t%" PRIu32 "\n", pcid.irq );
}