#include <cstdint>
#include <iostream>
#include <string>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "exception.hh"
#include "helpers.hh"
#include "ipv4_datagram.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // requested_ 的值（->second）是上次发出 ARP 请求的时刻。
  auto requested_iter = requested_.find( next_hop.ipv4_numeric() );
  if ( requested_iter != requested_.end() ) {
    if ( now_ - requested_iter->second > ARP_REQUEST_COOLDOWN_MS ) {
      // 这次请求过期：等它的数据报全部失效，下面会为当前这个数据报重新发请求。
      requested_.erase( requested_iter );
      datagrams_to_send_.erase( next_hop.ipv4_numeric() );
    } else {
      // 仍在 5 秒内：只排队，不重发，也不把请求时间改成 now_。
      datagrams_to_send_[next_hop.ipv4_numeric()].push( dgram );
      return;
    }
  }

  auto cache_iter = cache_.find( next_hop.ipv4_numeric() );
  EthernetFrame frame;
  // cache_ 的值：->second.first 是时间戳，->second.second 是下一跳的 MAC。
  // 以太网头按 dst、src、type 的顺序初始化。
  if ( cache_iter != cache_.end() && now_ - cache_iter->second.first <= ARP_MAPPING_TTL_MS ) {
    frame.header = { cache_iter->second.second, ethernet_address_, EthernetHeader::TYPE_IPv4 };
    frame.payload = serialize( dgram );
  } else {
    if ( cache_iter != cache_.end() ) {
      cache_.erase( cache_iter );
    }
    frame.header = { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP };
    ARPMessage message;
    message.opcode = ARPMessage::OPCODE_REQUEST;
    message.sender_ethernet_address = ethernet_address_;
    message.sender_ip_address = ip_address_.ipv4_numeric();
    message.target_ip_address = next_hop.ipv4_numeric();
    frame.payload = serialize( message );
    // 时间戳记的是这次请求发出的时刻。随后 5 秒内的数据报都排在这一次请求后面。
    requested_[next_hop.ipv4_numeric()] = now_;
    datagrams_to_send_[next_hop.ipv4_numeric()].push( dgram );
  }
  transmit( frame );
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // 先看以太网类型再 parse。parse 会把 payload 移走，同一份缓冲区只能解析一次。
  // 目的地址必须是本接口或广播，别人的单播帧直接忽略。
  if ( InternetDatagram datagram;
       frame.header.type == EthernetHeader::TYPE_IPv4
       && ( frame.header.dst == ethernet_address_ || frame.header.dst == ETHERNET_BROADCAST )
       && parse( datagram, frame.payload ) ) {
    datagrams_received_.push( datagram );
    return;
  }
  if ( ARPMessage message; frame.header.type == EthernetHeader::TYPE_ARP && parse( message, frame.payload ) ) {
    // 请求和回复都从 sender 学习映射。已有记录只刷新 pair.first（时间戳）。
    auto it = cache_.find( message.sender_ip_address );
    if ( it != cache_.end() ) {
      it->second.first = now_;
    } else {
      cache_[message.sender_ip_address] = { now_, message.sender_ethernet_address };
    }
    // 只有对方在问我们自己的 IP 时才回复。
    if ( message.opcode == ARPMessage::OPCODE_REQUEST && message.target_ip_address == ip_address_.ipv4_numeric() ) {
      EthernetFrame reply_frame;
      reply_frame.header = { message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP };
      ARPMessage reply_message;
      reply_message.opcode = ARPMessage::OPCODE_REPLY;
      reply_message.sender_ethernet_address = ethernet_address_;
      reply_message.sender_ip_address = ip_address_.ipv4_numeric();
      reply_message.target_ethernet_address = message.sender_ethernet_address;
      reply_message.target_ip_address = message.sender_ip_address;
      reply_frame.payload = serialize( reply_message );
      transmit( reply_frame );
    } else if ( message.opcode == ARPMessage::OPCODE_REPLY
                && message.target_ip_address == ip_address_.ipv4_numeric()
                && message.target_ethernet_address == ethernet_address_ ) {
      // 这是对我们自己请求的回复：把该下一跳上等待的数据报按对方 MAC 发出去。
      requested_.erase( message.sender_ip_address );
      auto& datagrams = datagrams_to_send_[message.sender_ip_address];
      while ( !datagrams.empty() ) {
        auto send_datagram = datagrams.front();
        datagrams.pop();
        EthernetFrame send_frame;
        send_frame.header = { message.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 };
        send_frame.payload = serialize( send_datagram );
        transmit( send_frame );
      }
      datagrams_to_send_.erase( message.sender_ip_address );
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  now_ += ms_since_last_tick;

  // map::erase 返回下一个迭代器。删除的分支不能再 ++，否则会跳过下一项；
  // 删掉最后一个时还会对 end() 自增。
  for ( auto it = cache_.begin(); it != cache_.end(); ) {
    if ( now_ - it->second.first > ARP_MAPPING_TTL_MS ) {
      it = cache_.erase( it );
    } else {
      it++;
    }
  }
  for ( auto it = requested_.begin(); it != requested_.end(); ) {
    if ( now_ - it->second > ARP_REQUEST_COOLDOWN_MS ) {
      datagrams_to_send_.erase( it->first );
      it = requested_.erase( it );
    } else {
      it++;
    }
  }
}
