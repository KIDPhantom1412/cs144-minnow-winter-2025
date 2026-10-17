#include "reassembler.hh"
#include "debug.hh"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <sys/types.h>

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  if ( is_last_substring ) {
    last_byte_ = first_index + data.size();
    if ( start_ == last_byte_ ) {
      output_.writer().close();
    }
  }
  uint64_t end_ = start_ + output_.writer().available_capacity();
  if ( first_index >= end_ ) {
    return;
  }
  if ( first_index < start_ ) {
    if ( first_index + data.size() < start_ ) {
      return;
    }

    data.erase( 0, start_ - first_index ); // 一个性能优化点，不过这里我嫌烦，先用erase吧
    first_index = start_;
  }
  if ( data.size() > end_ - first_index ) {
    data.resize( end_ - first_index ); // 一个性能优化点，不过这里我嫌烦，先用resize吧
  }

  if ( first_index == start_ ) {
    output_.writer().push( data );
    uint64_t bytes_to_end = buffer_.size() - first_index % buffer_.size();
    if ( data.size() <= bytes_to_end ) {
      fill_n( bitmap_.begin() + first_index % bitmap_.size(), data.size(), false );
    } else {
      fill_n( bitmap_.begin() + first_index % bitmap_.size(), bytes_to_end, false );
      fill_n( bitmap_.begin(), data.size() - bytes_to_end, false );
    }
    uint64_t length = 0;
    while ( first_index + data.size() + length < end_
            && bitmap_[( first_index + data.size() + length ) % buffer_.size()] ) {
      length++;
    }
    start_ += data.size();
    bytes_to_end = buffer_.size() - start_ % buffer_.size();
    if ( length <= bytes_to_end ) {
      output_.writer().push( buffer_.substr( start_ % buffer_.size(), length ) );
      fill_n( bitmap_.begin() + start_ % bitmap_.size(), length, false );
    } else {
      output_.writer().push( buffer_.substr( start_ % buffer_.size(), bytes_to_end ) );
      fill_n( bitmap_.begin() + start_ % bitmap_.size(), bytes_to_end, false );
      output_.writer().push( buffer_.substr( 0, length - bytes_to_end ) );
      fill_n( bitmap_.begin(), length - bytes_to_end, false );
    }
    start_ += length;
  } else {
    uint64_t bytes_to_end = buffer_.size() - first_index % buffer_.size();
    if ( data.size() <= bytes_to_end ) {
      copy( data.begin(), data.end(), buffer_.begin() + first_index % buffer_.size() );
      fill_n( bitmap_.begin() + first_index % buffer_.size(), data.size(), true );
    } else {
      copy( data.begin(), data.begin() + bytes_to_end, buffer_.begin() + first_index % buffer_.size() );
      fill_n( bitmap_.begin() + first_index % bitmap_.size(), bytes_to_end, true );
      copy( data.begin() + bytes_to_end, data.end(), buffer_.begin() );
      fill_n( bitmap_.begin(), data.size() - bytes_to_end, true );
    }
  }

  if ( start_ == last_byte_ ) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t end_ = start_ + output_.writer().available_capacity();
  uint64_t cnt = 0;
  for ( uint64_t i = start_; i < end_; i++ ) {
    cnt += bitmap_[i % bitmap_.size()];
  }
  return cnt;
}
