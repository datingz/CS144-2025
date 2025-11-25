#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // error
  if(message.RST){
    this->reassembler_.reader().set_error();
  }
  if(message.SYN){
    this->zero_point_ = message.seqno;
    this->is_first_ = false;
  }
  uint64_t check_point = this->writer().bytes_pushed();
  uint64_t first_index = message.seqno.unwrap(this->zero_point_, check_point) - !message.SYN;
  bool is_last_substring = message.FIN;
  this->reassembler_.insert(first_index, std::move(message.payload), is_last_substring);

}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  TCPReceiverMessage res;
  if(!this->is_first_){
    res.ackno = Wrap32::wrap(this->reassembler_.writer().bytes_pushed() + 1 + this->writer().is_closed(), this->zero_point_);
  }
  uint64_t c = this->writer().available_capacity();
  if(c > UINT16_MAX){
    c = UINT16_MAX;
  }
  res.window_size = c;
  res.RST = false;
  if(this->reader().has_error())
    res.RST = true;
  return res;
}
