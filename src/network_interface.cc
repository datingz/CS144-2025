#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
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
void NetworkInterface::send_datagram( InternetDatagram dgram, const Address& next_hop )
{
  // encapsulation the InternetDatagram and transmit frame
  EthernetFrame frame;
  // dsr MAC addr (dont'n know now)
  EthernetAddress MAC_dst;

  uint32_t IP_dst =  next_hop.ipv4_numeric();
  // hit
  if(this->arp_cache_.find(IP_dst) != this->arp_cache_.end()){
    // set header
    MAC_dst = this->arp_cache_[IP_dst].MAC_address_;
    frame.header.type = EthernetHeader::TYPE_IPv4;
    frame.header.src = this->ethernet_address_;
    frame.header.dst = MAC_dst;
    // set payload
    InternetDatagram d = dgram;
    frame.payload = serialize( d );
    this->transmit( frame );
  } else{
    // miss
    // if(this->datagrams_queued_.find(IP_dst) != this->datagrams_queued_.end()){
    //   // do not resend but push in queue
    //   DatagramState d;
    //   d.dgram_ = dgram;
    //   d.timestamp_ = 0;
    //   this->datagrams_queued_.emplace( IP_dst, d);
    // }
    
    // should resend?
    auto range = this->datagrams_queued_.equal_range(IP_dst);
    bool do_not_resend = false;
    for(auto it = range.first; it != range.second; ++it){
      if(it->second.had_resent_){
        do_not_resend = true;
      }
    }

    // push in queue
    DatagramState d;
    d.dgram_ = dgram;
    d.timestamp_ = 0;
    if(do_not_resend){
      this->datagrams_queued_.emplace( IP_dst, d );
      return;
    }
    d.had_resent_ = true;
    this->datagrams_queued_.emplace( IP_dst, d );
    // send arp request
    ARPMessage arp_msg;
    // set arp message
    arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
    arp_msg.sender_ethernet_address =this->ethernet_address_;
    arp_msg.sender_ip_address = this->ip_address_.ipv4_numeric();
    // arp_msg.target_ethernet_address = ETHERNET_BROADCAST;
    arp_msg.target_ip_address = IP_dst;
    // // push in queue
    // DatagramState d;
    // d.dgram_ = dgram;
    // d.timestamp_ = 0;
    // this->datagrams_queued_.emplace( IP_dst, d );
    // set frame header
    frame.header.type = EthernetHeader::TYPE_ARP;
    frame.header.src = this->ethernet_address_;
    frame.header.dst = ETHERNET_BROADCAST;
    // set frame payload
    frame.payload = serialize( arp_msg );
    this->transmit( frame );
  }
  
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  // unrelated situlation
  EthernetAddress target = frame.header.dst;
  if(target != ETHERNET_BROADCAST && target != this->ethernet_address_){
    return;
  }

  uint16_t type = frame.header.type;
  if(type == EthernetHeader::TYPE_IPv4){
    InternetDatagram dgram;
    if(parse( dgram, frame.payload ) != true){
      return;
    }
    this->datagrams_received_.push( dgram );
  } else if(type == EthernetHeader::TYPE_ARP){
    ARPMessage arp_msg;
    if(parse( arp_msg, frame.payload ) != true){
      return;
    }
    // learn new cast
    uint32_t IP_learn = arp_msg.sender_ip_address;
    EthernetAddress MAC_learn = arp_msg.sender_ethernet_address;
    ArpCacheState a;
    a.MAC_address_ = MAC_learn;
    a.timestamp_ = 0;
    this->arp_cache_[IP_learn] = a;

    // send: resend dgram or send reply
    EthernetFrame send;
    // if find in queue, resend the dgram
    auto range = this->datagrams_queued_.equal_range( IP_learn );
    // if(this->datagrams_queued_.find(IP_learn) != this->datagrams_queued_.end()){
    //   send.header.type = EthernetHeader::TYPE_IPv4;
    //   send.header.src = this->ethernet_address_;
    //   send.header.dst = MAC_learn;
    //   InternetDatagram d = this->datagrams_queued_[IP_learn].dgram_;
    //   send.payload = serialize( d );
    //   this->datagrams_queued_.erase(IP_learn);
    //   this->transmit( send );
    // }  
    for(auto it = range.first; it != range.second; ++it){
      send.header.type = EthernetHeader::TYPE_IPv4;
      send.header.src = this->ethernet_address_;
      send.header.dst = MAC_learn;
      InternetDatagram d = it->second.dgram_;
      send.payload = serialize( d );
      this->transmit( send );
    }
    this->datagrams_queued_.erase( IP_learn );
    if(arp_msg.opcode == ARPMessage::OPCODE_REQUEST){
      if(arp_msg.target_ip_address != this->ip_address_.ipv4_numeric()){
        return;
      }
      // if receive arp request, relpy it
      ARPMessage new_arp_msg;
      new_arp_msg.opcode = ARPMessage::OPCODE_REPLY;
      new_arp_msg.sender_ethernet_address = this->ethernet_address_;
      new_arp_msg.sender_ip_address = this->ip_address_.ipv4_numeric();
      new_arp_msg.target_ethernet_address = MAC_learn;
      new_arp_msg.target_ip_address = IP_learn;
      // set header
      send.header.type = EthernetHeader::TYPE_ARP;
      send.header.src = this->ethernet_address_;
      send.header.dst = MAC_learn;
      // set payload
      send.payload = serialize( new_arp_msg );
      this->transmit( send );
    }
    
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for(auto it = this->arp_cache_.begin(); it != this->arp_cache_.end();){
    it->second.timestamp_ += ms_since_last_tick;
    if(it->second.timestamp_ > 30000){
      it = this->arp_cache_.erase(it);
    } else{
      it++;
    }
    
  }
  for(auto it = this->datagrams_queued_.begin(); it != this->datagrams_queued_.end();){
    it->second.timestamp_ += ms_since_last_tick;
    if(it->second.timestamp_ > 5000){
      it = this->datagrams_queued_.erase(it);
    } else{
      it++;
    }
    
  }
}



