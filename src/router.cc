#include "router.hh"
#include "debug.hh"

#include <iostream>

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
  Route_info a;
  a.route_prefix_ = route_prefix;
  a.prefix_length_ = prefix_length;
  a.next_hop_ = next_hop;
  a.interface_num_ = interface_num;
  if(route_prefix == 0 && prefix_length == 0){
    this->has_default_router_ = true;
    this->default_router_.interface_num_ = interface_num;
    this->default_router_.next_hop_ = next_hop;
  }
  this->route_ip_.push_back(std::move(a));
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{

    for(size_t i = 0; i < this->interfaces_.size(); ++i){
        // get interface
        NetworkInterface& interface = *this->interface(i);
        while(!interface.datagrams_received().empty()){
            InternetDatagram dgram = interface.datagrams_received().front();
            uint32_t IP_dst = dgram.header.dst;
            uint8_t max_length = 0;
            // get longest prefix match
            Route_info longest_prefix_match;
            bool no_match = true;
            // check ip route
            for(auto it = this->route_ip_.begin(); it != this->route_ip_.end(); ++it){
                // size_t fuck = interface.datagrams_received().size();
                uint32_t cur_prefix = it->route_prefix_;
                uint8_t cur_length = it->prefix_length_;
                // get mask;
                uint32_t mask = 0;
                if(cur_length >= 32){
                    mask = 0xffffffff;
                } else if(cur_length == 0){
                    mask = 0;
                } else{
                    mask = ~0U << (32 - cur_length);
                }
                cur_prefix &= mask;
                uint32_t cmp = IP_dst & mask;
                if(cmp == cur_prefix && cur_length > max_length){
                    no_match = false;
                    max_length = cur_length;
                    longest_prefix_match.interface_num_ = it->interface_num_;
                    longest_prefix_match.next_hop_ = it->next_hop_;
                }
            }
            if(!no_match && dgram.header.ttl > 1){
                // send the dgram througth the interface
                dgram.header.ttl--;
                dgram.header.compute_checksum();
                if(longest_prefix_match.next_hop_.has_value()){
                    NetworkInterface& send = *this->interface(longest_prefix_match.interface_num_);
                    send.send_datagram(dgram, *longest_prefix_match.next_hop_);
                } else{
                    Address add = Address::from_ipv4_numeric(IP_dst);
                    NetworkInterface& send = *this->interface(longest_prefix_match.interface_num_);
                    send.send_datagram(dgram, std::move(add));
                }
            }else if(no_match && dgram.header.ttl > 1 && this->has_default_router_){
                dgram.header.ttl--;
                dgram.header.compute_checksum();
                // send to default router
                NetworkInterface& send = *this->interface(this->default_router_.interface_num_);
                send.send_datagram(dgram, *this->default_router_.next_hop_);
            }

            interface.datagrams_received().pop();
        }
    }
    
}
