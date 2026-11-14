#pragma once

#include "exception.hh"
#include "network_interface.hh"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    interfaces_.push_back( notnull( "add_interface", std::move( interface ) ) );
    return interfaces_.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return interfaces_.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( uint32_t route_prefix,
                  uint8_t prefix_length,
                  std::optional<Address> next_hop,
                  size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> interfaces_ {};

  // 外层 key 是按前缀掩完的网络地址，内层 key 是前缀长度。
  // 掩完相同的几条路由落在同一个外层项里，例如 10.0.0.0/8 和 10.0.0.0/16。
  // 值是下一跳（空表示直连）和出接口编号。
  using PrefixTable = std::map<uint8_t, std::pair<std::optional<Address>, size_t>>;
  std::map<uint32_t, PrefixTable> routing_table_ {};

  // 在同一个网络地址下，从长到短找第一条能匹配 dst 的前缀。
  std::optional<PrefixTable::iterator> find_longest_prefix( PrefixTable& prefix_table,
                                                            uint32_t network_address,
                                                            uint32_t dst );
};
