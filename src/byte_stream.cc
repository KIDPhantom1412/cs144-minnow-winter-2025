#include "byte_stream.hh"
#include <algorithm>
#include <cstdint>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : buffer( capacity + 1, '\0' ), capacity_( capacity ) {}

uint64_t ByteStream::buffered_size() const
{
  if ( tail >= head ) {
    return tail - head;
  }
  return static_cast<uint64_t>( buffer.size() ) - head + tail;
}

void Writer::push( string data )
{
  const uint64_t length = min( static_cast<uint64_t>( data.size() ), available_capacity() );
  bytes_pushed_ += length;
  const uint64_t bytes_to_end = static_cast<uint64_t>( buffer.size() ) - tail;
  if ( length <= bytes_to_end ) {
    copy( data.begin(), data.begin() + length, buffer.begin() + tail );
  } else {
    copy( data.begin(), data.begin() + bytes_to_end, buffer.begin() + tail );
    copy( data.begin() + bytes_to_end, data.begin() + length, buffer.begin() );
  }
  tail = ( tail + length ) % buffer.size();
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
  const uint64_t length = min( static_cast<uint64_t>( buffer.size() ) - head, bytes_buffered() );
  return string_view( buffer.begin() + head, buffer.begin() + head + length );
}

void Reader::pop( uint64_t len )
{
  head = ( head + min( len, bytes_buffered() ) ) % buffer.size();
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
