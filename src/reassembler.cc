#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // debug( "unimplemented insert({}, {}, {}) called", first_index, data, is_last_substring );
  if(data.empty()){
    if(is_last_substring){
      this->received_last_substring_ = true;
    }else{
      return;
    }
  }

  // insert everything except the index out of range
  uint64_t data_len = data.size();
  uint64_t exp_index = this->expected_index_;
  uint64_t max_index = exp_index + this->writer().available_capacity() - 1;

  bool is_inserted = false;
  if(first_index + data_len - 1 <= max_index){
    this->buffer_.emplace(first_index, data);
    is_inserted = true;
  } else if(first_index < max_index){
    // cut
    std::string temp = data.substr(0, max_index - first_index + 1);
    if(!temp.empty()){
      this->buffer_.emplace(first_index, temp);
      is_inserted = true;
    }
  }

  if(is_inserted && is_last_substring){
    this->received_last_substring_ = true;
  }
  
  // find the minimun index
  while(!this->buffer_.empty()){
    uint64_t min_index = this->buffer_.begin()->first;
    uint64_t len = this->buffer_.begin()->second.size();
    if(min_index + len - 1 >= exp_index && min_index <= exp_index){
      string sub = this->buffer_.begin()->second.substr(exp_index);
      // push and update cur_index
      if(!sub.empty()){
        this->output_.writer().push(sub);
        exp_index = min_index + len; 
        this->buffer_.erase(this->buffer_.begin());
      }
    } else {
      break;
    } 
  }

  this->expected_index_ = exp_index;
   
  // if received last sub && output all the data
  if(this->received_last_substring_){
    this->output_.writer().close();
  }
  
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
// that mean u have to count the bytes but not return a state for counting
uint64_t Reassembler::count_bytes_pending() const
{
  // debug( "unimplemented count_bytes_pending() called" );
  if(this->buffer_.empty()){
    return {};
  }
  uint64_t head = this->buffer_.begin()->first;
  uint64_t tail = this->buffer_.rbegin()->first;
  tail += this->buffer_.rbegin()->second.size() - 1;
  return tail - head + 1;
}
