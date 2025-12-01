#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// How many sequence numbers are outstanding?
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  uint64_t res = 0;
  for ( auto it = this->buffer_.begin(); it != this->buffer_.end(); ++it ) {
    res += it->sequence_length();
  }
  return res;
}

// How many consecutive retransmissions have happened?
uint64_t TCPSender::consecutive_retransmissions() const
{
  return this->retransmission_times_;
}

void TCPSender::push( const TransmitFunction& transmit )
{

  uint64_t index = this->next_seqno_;
  string payload = "";
  size_t peek_size;
  uint16_t window_size;
  // check before sub
  if(this->unsent_index_ + (this->cur_window_size_ == 0 ? 1 : this->cur_window_size_) > this->next_seqno_){
    window_size = this->unsent_index_ + (this->cur_window_size_ == 0 ? 1 : this->cur_window_size_) - this->next_seqno_;
  } else{
    window_size = 0;
  }

  while ( window_size > 0 ) {
    // alreadly sent all bytes
    if(this->is_sent_FIN_){
      return;
    }
    TCPSenderMessage res;
    // set SYN & start the timer
    if ( index == 0 ) {
      res.SYN = true;
      this->is_timer_running_ = true;
      window_size--;
    }
    // get payload
    string_view sv = this->reader().peek();
    peek_size = sv.size();
    uint64_t max_size = window_size < TCPConfig::MAX_PAYLOAD_SIZE ? window_size : TCPConfig::MAX_PAYLOAD_SIZE;
    if ( peek_size >= max_size ) {
      payload = sv.substr( 0, max_size );
      this->reader().pop( max_size );
    } else if ( peek_size != 0 ) {
      // get long enougth payload
      while ( payload.size() < max_size ) {
        if ( payload.size() + peek_size <= max_size ) {
          payload += sv;
          this->reader().pop( peek_size );
        } else {
          size_t t = max_size - payload.size() - peek_size;
          payload += sv.substr( 0, t );
          this->reader().pop( t );
        }
        // update
        sv = this->reader().peek();
        peek_size = sv.size();
        if ( peek_size == 0 ) {
          break;
        }
      }
    }
    
    
    // set res message
    res.payload = std::move( payload );
    if(window_size > res.sequence_length()){
      // set FIN
      if ( this->writer().is_closed() &&  this->reader().peek().size() == 0 && !this->is_sent_FIN_) {
        res.FIN = true;
        this->is_sent_FIN_ = true;
      }
    }
    window_size -= res.payload.size() + res.FIN;
    res.seqno = std::move( Wrap32::wrap( index, this->isn_ ) );
    index += res.sequence_length();
    // set RST
    if ( this->reader().has_error() ) {
      res.RST = true;
      transmit( std::move( res ) );
      return;
    }
    // no SYN, no payload, no FIN
    if ( res.sequence_length() == 0 ) {
      break;
    }else if( res.FIN ){
      this->buffer_.push_back( res );
      // send res
      transmit( std::move( res ) );
      this->is_sent_FIN_ = true;
      this->is_timer_running_ = true;
      break;
    }

    this->buffer_.push_back( res );
    // send res
    transmit( std::move( res ) );
    // start timer
    this->is_timer_running_ = true;
  }

  this->next_seqno_ = index;
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage res;
  if(this->reader().has_error()){
    res.RST = true;
  }
  res.seqno = Wrap32::wrap( this->next_seqno_, this->isn_ );
  return res;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  this->cur_window_size_ = msg.window_size;
  // set status
  if ( msg.RST ) {
    this->reader().set_error();
  }
  if ( !msg.ackno.has_value() ) {
    return;
  }
  Wrap32 ack = *msg.ackno;

  uint64_t new_index = ack.unwrap( this->isn_, this->unsent_index_ );
  if ( new_index > this->unsent_index_ && new_index <= this->next_seqno_) {
    // is new
    this->unsent_index_ = new_index;
    this->RTO_ms_ = this->initial_RTO_ms_;
    this->time_since_timer_start_ms_ = 0;
    this->retransmission_times_ = 0;
  }
  
  // delete sent successfully
  while ( !this->buffer_.empty() ) {
    uint64_t index = this->buffer_.front().seqno.unwrap( this->isn_, this->unsent_index_ );
    size_t len = this->buffer_.front().sequence_length();
    index += len - 1;
    if ( index < this->unsent_index_ ) {
      this->buffer_.pop_front();
    } else {
      break;
    }
  }
  // close timer when the buffer is empty
  if(this->buffer_.empty()){
    this->is_timer_running_ = false;
    this->RTO_ms_ = this->initial_RTO_ms_;
    this->time_since_timer_start_ms_ = 0;
    this->retransmission_times_ = 0;
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // is timer start?
  if(this->is_timer_running_){
    this->time_since_timer_start_ms_ += ms_since_last_tick;
  } else{
    return;
  }

  if(this->time_since_timer_start_ms_ < this->RTO_ms_){
    return;
  }
  // time out
  if(!this->buffer_.empty()){
    transmit(this->buffer_.front());
    
    if(this->cur_window_size_ != 0){
      this->RTO_ms_ *= 2;
      this->retransmission_times_++;
    }

    this->time_since_timer_start_ms_ = 0;
  }

}
