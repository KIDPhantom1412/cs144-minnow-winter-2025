#include "tcp_sender.hh"
#include "byte_stream.hh"
#include "debug.hh"
#include "tcp_config.hh"
#include "wrapping_integers.hh"
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <pthread.h>
#include <sys/types.h>

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return nextno_ - ackno_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  uint64_t x = RTO_ms_ / initial_RTO_ms_;
  uint64_t cnt = 0;
  while ( x ) {
    x >>= 1;
    cnt++;
  }
  return --cnt;
}

void TCPSender::transmit_and_add_timer( const TCPSenderMessage& msg, const TransmitFunction& transmit )
{
  if ( ackno_ == nextno_ ) { // 每新发一个段，都需要重置deadline_
    deadline_ = now_ + RTO_ms_;
  }
  transmit( msg );
  nextno_ += msg.sequence_length();
  queue_.push( msg );
}

void TCPSender::push( const TransmitFunction& transmit )
{
  if ( window_size_ > 0 && ackno_ + window_size_ <= nextno_ ) { // 目前发送窗口的数据都处在in flight当中
    return;
  }
  if ( input_.has_error() ) { // 发送RST
    transmit_and_add_timer( make_empty_message(), transmit );
    return;
  }

  uint16_t effective_window = window_size_ == 0 ? 1 : window_size_;
  while ( !FIN_ && nextno_ < ackno_ + effective_window ) {
    uint64_t max_push_length = min( TCPConfig::MAX_PAYLOAD_SIZE, ackno_ + effective_window - nextno_ );
    string push_bytes;
    bool SYN = 0 == nextno_;
    read( input_.reader(), max_push_length - SYN, push_bytes );
    FIN_ = input_.reader().is_finished();
    if ( !SYN && push_bytes.empty() && !FIN_ ) { // 没有任何有效信息的情况下直接退出
      return;
    }
    if ( SYN + push_bytes.size() + FIN_ > ackno_ + effective_window - nextno_ ) {
      FIN_ = false;
    }
    transmit_and_add_timer( { Wrap32::wrap( nextno_, isn_ ), SYN, push_bytes, FIN_, input_.has_error() },
                            transmit );
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return { Wrap32::wrap( nextno_, isn_ ), false, {}, false, input_.has_error() };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if ( msg.RST ) {
    input_.set_error();
    return;
  }

  window_size_ = msg.window_size;
  if ( !msg.ackno.has_value() ) { // 没有ackno也可以更新window_size_
    return;
  }

  bool accept_ack = false;
  while ( !queue_.empty()
          && queue_.front().seqno.unwrap( isn_, ackno_ ) + queue_.front().sequence_length()
               <= msg.ackno.value().unwrap( isn_, nextno_ ) ) {
    if ( msg.ackno.value().unwrap( isn_, ackno_ ) <= nextno_ ) { // 对ACK的合法性校验
      ackno_ = msg.ackno.value().unwrap( isn_, ackno_ );
      queue_.pop();

      accept_ack = true;
    } else {
      break;
    }
  }

  if ( accept_ack ) { // 收到了有效的ack，必须要更新RTO和deadline_
    RTO_ms_ = initial_RTO_ms_;
    deadline_ = now_ + initial_RTO_ms_;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  now_ += ms_since_last_tick;
  if ( !queue_.empty() && now_ >= deadline_ ) {
    if ( window_size_ ) { // 如果window_size_=0，RTO不会倍增
      RTO_ms_ *= 2;
    }
    deadline_ = now_ + RTO_ms_;
    transmit( queue_.front() );
  }
}
