#include "router.hh"
#include "address.hh"
#include "debug.hh"

#include <cstdint>
#include <iostream>
#include <iterator>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  if ( prefix_length > 32 ) {
    return;
  }

  // 高 prefix_length 位为 1。用 64 位是因为长度为 32 时要左移 32 位，在 uint32_t 上是未定义行为。
  // 长度为 0 时掩码是 0，表示默认路由。
  uint32_t mask = ( static_cast<uint64_t>( 1 ) << 32 ) - ( static_cast<uint64_t>( 1 ) << ( 32 - prefix_length ) );
  // 只把网络号当 key。同一网络号、不同前缀长度会进同一个内层表。
  uint32_t network_address = route_prefix & mask;
  routing_table_[network_address][prefix_length] = { next_hop, interface_num };
}

std::optional<Router::PrefixTable::iterator> Router::find_longest_prefix( PrefixTable& prefix_table,
                                                                          uint32_t network_address,
                                                                          uint32_t dst )
{
  // 内层 map 按前缀长度升序，从尾部往前才是从长到短。先命中的就是这一项里的最长匹配。
  auto prefix_table_iter = prefix_table.end();
  while ( prefix_table_iter != prefix_table.begin() ) {
    prefix_table_iter = prev( prefix_table_iter );
    uint8_t prefix_length = prefix_table_iter->first;
    uint32_t mask = ( static_cast<uint64_t>( 1 ) << 32 ) - ( static_cast<uint64_t>( 1 ) << ( 32 - prefix_length ) );
    // 再用这一档自己的长度验一次。network_address 入库时已经掩过，低位是 0。
    if ( ( network_address & mask ) == ( dst & mask ) ) {
      return prefix_table_iter;
    }
  }
  return nullopt;
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto interface : interfaces_ ) {
    auto& datagrams = interface->datagrams_received();
    while ( !datagrams.empty() ) {
      InternetDatagram datagram = datagrams.front();
      datagrams.pop();
      // 绑定的是报头里的 ttl，--ttl 才会写回去。进来已是 0，或减完变成 0，都丢弃。
      uint8_t& ttl = datagram.header.ttl;
      if ( ttl == 0 || --ttl == 0 ) {
        continue;
      }
      // 校验和覆盖 TTL，不重算的话对端对不上。
      datagram.header.compute_checksum();

      uint32_t dst = datagram.header.dst;
      // 比 dst 更大的网络号不可能是它的前缀：前缀是把 dst 的低位清 0，结果不会比 dst 大。
      // 从第一个大于 dst 的位置往前走，就是按网络号从大到小试。匹配得上的路由里，前缀越长、
      // 清掉的低位越少，网络号越大，所以会先被看到。第一处真正匹配的就是最长前缀。
      auto routing_table_iter = routing_table_.upper_bound( dst );
      while ( routing_table_iter != routing_table_.begin() ) {
        routing_table_iter = prev( routing_table_iter );
        uint32_t network_address = routing_table_iter->first;
        // x & -x 取出最低的那个 1，它及其左边的位构成一个更短的掩码。网络号低位已是 0，
        // 这个 1 落在前缀内部，所以掩码比真实前缀松。这些位都对不上，这一项里每一档前缀
        // 长度都不可能匹配，就不必再扫。0.0.0.0 没有置位比特，掩码算出来是 0，比较恒为真，
        // 默认路由不会在这里被跳过。
        uint32_t mask = ~( ( network_address & -network_address ) - 1 );
        if ( ( network_address & mask ) != ( dst & mask ) ) {
          continue;
        }

        auto& prefix_table = routing_table_iter->second;
        auto prefix_table_iter = find_longest_prefix( prefix_table, network_address, dst );
        if ( prefix_table_iter.has_value() ) {
          auto [next_hop, interface_num] = prefix_table_iter.value()->second;
          // 空的下一跳表示目的网络直连在该接口上，下一跳就是目的地址本身。
          if ( next_hop.has_value() ) {
            interfaces_[interface_num]->send_datagram( datagram, next_hop.value() );
          } else {
            interfaces_[interface_num]->send_datagram( datagram, Address::from_ipv4_numeric( dst ) );
          }
          break;
        }
      }
    }
  }
}
