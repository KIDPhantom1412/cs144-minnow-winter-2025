#include "wrapping_integers.hh"
#include "debug.hh"
#include <cstdint>
#include <limits>
#include <sys/types.h>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>( n );
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t low_part = static_cast<uint64_t>( ( this->raw_value_ - zero_point.raw_value_ ) );
  uint64_t low_mask = static_cast<uint64_t>( numeric_limits<uint32_t>::max() );
  uint64_t high_mask = numeric_limits<uint64_t>::max() ^ low_mask;
  if ( low_part <= ( checkpoint & low_mask ) ) {
    if ( ( checkpoint & low_mask ) - low_part <= low_part + ( low_mask + 1 - ( checkpoint & low_mask ) ) ) {
      return ( checkpoint & high_mask ) + low_part;
    }
    return ( ( checkpoint + ( static_cast<uint64_t>( 1 ) << 32 ) ) & high_mask ) + low_part;
  }
  if ( ( checkpoint & high_mask ) == 0
       || low_part - ( checkpoint & low_mask ) <= low_mask + 1 - low_part + ( checkpoint & low_mask ) ) {
    return ( checkpoint & high_mask ) + low_part;
  }
  return ( ( checkpoint - ( static_cast<uint64_t>( 1 ) << 32 ) ) & high_mask ) + low_part;
}
