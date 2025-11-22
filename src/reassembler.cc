#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{

  if ( is_last_substring ) {
    this->received_last_substring_ = true;
    if ( data.empty() ) {
      this->EOF_index_ = first_index;
      if ( this->expected_index_ == first_index ) {
        this->output_.writer().close();
        this->buffer_.clear();
      }
      return;
    } else {
      this->EOF_index_ = first_index + data.size();
    }
  } else {
    if ( data.empty() )
      return;
  }

  // insert except the index out of range
  uint64_t exp_index = this->expected_index_;
  uint64_t tail_index = first_index + data.size();
  uint64_t bound_index = exp_index + this->output_.writer().available_capacity() - 1;
  // cut the overflow part
  if(first_index > bound_index || (this->received_last_substring_ && first_index >= this->EOF_index_)){
    return;
  }
  if ( tail_index > bound_index) {
    data = data.substr( 0, bound_index - first_index + 1 );
    if ( data.empty() ) {
      return;
    }
  }

  if ( tail_index < exp_index + 1 && !is_last_substring ) {
    return;
  } else if ( first_index < exp_index ) {
    data = data.substr( exp_index - first_index );
    first_index = exp_index;
  }

  auto it = this->buffer_.upper_bound( first_index );
  // cut right
  if ( it != this->buffer_.end() ) {
    uint64_t r_index = it->first;
    while(first_index + data.size() > r_index + it->second.size()){
      auto temp_it = it;
      it++;
      this->buffer_.erase(temp_it);
      if(it != this->buffer_.end()){
        r_index = it->first;
      } else{
        break;
      }
    }
    it = this->buffer_.upper_bound( first_index );
    if( it != this->buffer_.end()){
      r_index = it->first;
      data = data.substr( 0, r_index - first_index );
    }
  }
  
  if ( it != this->buffer_.begin() ) {
    it = std::prev( it );
    uint64_t l_index = it->first;
    uint64_t len = it->second.size();
    // cut left
    if ( first_index >= l_index && first_index + data.size() <= l_index + len ) {
      return;
    } else if( first_index < l_index + len ){
      data = data.substr( l_index + len - first_index);
      first_index = l_index + len;
    }
  }

  // insert
  this->buffer_.emplace( first_index, data );

  // find the minimun index
  // decide push or not
  while ( !this->buffer_.empty() ) {
    if ( this->received_last_substring_ && this->EOF_index_ <= exp_index ) {
      this->output_.writer().close();
      this->buffer_.clear();
      return;
    }

    uint64_t min_index = this->buffer_.begin()->first;
    uint64_t len = this->buffer_.begin()->second.size();
    if ( exp_index == min_index ) {
      if(this->received_last_substring_ && min_index + len > this->EOF_index_){
        this->buffer_.begin()->second = this->buffer_.begin()->second.substr(0, this->EOF_index_ - min_index);
        len = this->buffer_.begin()->second.size();
      }
      uint64_t a = this->output_.writer().available_capacity();
      if ( len <= a ) {
        this->output_.writer().push( this->buffer_.begin()->second );
        this->buffer_.erase( this->buffer_.begin() );
        exp_index += len;
      } else {
        std::string temp = this->buffer_.begin()->second.substr( a );
        this->output_.writer().push( this->buffer_.begin()->second.substr( 0, a ) );
        this->buffer_.erase( this->buffer_.begin() );
        this->buffer_.emplace( min_index + a, temp );
        exp_index += a;
        break;
      }
    } else {
      break;
    }
  }

  this->expected_index_ = exp_index;

  // delete garbage
  if (received_last_substring_) {
      auto it_garbage = buffer_.lower_bound(EOF_index_);
      buffer_.erase(it_garbage, buffer_.end());
  }

  // EOF
  if ( this->received_last_substring_ && this->expected_index_ == this->EOF_index_ ) {
    this->output_.writer().close();
    this->buffer_.clear();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
// that mean u have to count the bytes but not return a state for counting
uint64_t Reassembler::count_bytes_pending() const
{
  // debug( "unimplemented count_bytes_pending() called" );
  if ( this->buffer_.empty() ) {
    return {};
  }
  uint64_t res = 0;
  for ( auto it = this->buffer_.begin(); it != this->buffer_.end(); it++ ) {
    res += it->second.size();
  }
  return res;
}
