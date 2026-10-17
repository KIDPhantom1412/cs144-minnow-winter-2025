#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer_( capacity + 1, '\0' ), capacity_( capacity ) {}

uint64_t ByteStream::buffered_size() const
{
  if ( tail_ >= head_ ) {
    return tail_ - head_;
  }
  return static_cast<uint64_t>( buffer_.size() ) - head_ + tail_;
}

void Writer::push( string data )
{
  const uint64_t length = min( static_cast<uint64_t>( data.size() ), available_capacity() );
  bytes_pushed_ += length;
  const uint64_t bytes_to_end = static_cast<uint64_t>( buffer_.size() ) - tail_;
  if ( length <= bytes_to_end ) {
    copy( data.begin(), data.begin() + length, buffer_.begin() + tail_ );
  } else {
    copy( data.begin(), data.begin() + bytes_to_end, buffer_.begin() + tail_ );
    copy( data.begin() + bytes_to_end, data.begin() + length, buffer_.begin() );
  }
  tail_ = ( tail_ + length ) % buffer_.size();
}

void Writer::close()
{
  is_closed_ = true;
}

bool Writer::is_closed() const
{
  return is_closed_;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffered_size();
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

string_view Reader::peek() const
{
  const uint64_t length = min( static_cast<uint64_t>( buffer_.size() ) - head_, bytes_buffered() );
  return string_view( buffer_.begin() + head_, buffer_.begin() + head_ + length );
}

void Reader::pop( uint64_t len )
{
  head_ = ( head_ + min( len, bytes_buffered() ) ) % buffer_.size();
}

bool Reader::is_finished() const
{
  return is_closed_ && bytes_buffered() == 0;
}

uint64_t Reader::bytes_buffered() const
{
  return buffered_size();
}

uint64_t Reader::bytes_popped() const
{
  return bytes_pushed_ - bytes_buffered();
}
