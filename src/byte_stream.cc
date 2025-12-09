#include "byte_stream.hh"
#include "debug.hh"
#include <iostream>

#define BLOCK_MAX_SIZE 4096

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) 
: capacity_( capacity )
, buffer_{}, is_closed_( false )
, available_capacity_( capacity )
, bytes_buffered_( 0 )
, bytes_popped_( 0 )
, bytes_pushed_( 0 ) 
, push_block_{}
, BLOCK_SIZE_( BLOCK_MAX_SIZE <= capacity ? BLOCK_MAX_SIZE : capacity )
, pop_block_{}
, pop_block_size_{}
{}

void ByteStream::write_block(const char* data, uint64_t len){
  if(this->push_block_.size() >= BLOCK_SIZE_){
    this->buffer_.push_back(this->push_block_);
    this->push_block_ = "";
  }
  this->push_block_.append(data, len);
}


// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)
  uint64_t a = this->available_capacity();
  uint64_t s = data.size();
  if(s == 0){
    return;
  }

  if(s > a){
    data = data.substr(0, a);
    s = a;
  }
  this->write_block(data.data(), s);
  this->bytes_pushed_ += s;
  this->bytes_buffered_ += s;
  this->available_capacity_ -= s;

}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  this->is_closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  return this->is_closed_; 
}

// How many bytes can be pushed to the stream right now?
uint64_t Writer::available_capacity() const
{
  //debug( "Writer::available_capacity() not yet implemented" );
  return this->available_capacity_; // Your code here.
}

// Total number of bytes cumulatively pushed to the stream
uint64_t Writer::bytes_pushed() const
{
  // debug( "Writer::bytes_pushed() not yet implemented" );
  return this->bytes_pushed_; // Your code here.
}

// Peek at the next bytes in the buffer -- ideally as many as possible.
// It's not required to return a string_view of the *whole* buffer, but
// if the peeked string_view is only one byte at a time, it will probably force
// the caller to do a lot of extra work.
string_view Reader::peek() const
{
  // if(!this->buffer_.empty()){
  //   return this->buffer_.front();
  // } 
  // return this->push_block_;
  
  if(!this->pop_block_.empty()){
    if(this->pop_block_size_ < this->pop_block_.size()){
      return std::string_view(this->pop_block_.data() + this->pop_block_size_, this->pop_block_.size() - this->pop_block_size_);
    }
  }
  if(!this->buffer_.empty()){
    return this->buffer_.front();
  } 
  return this->push_block_;


}


// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  if(len == 0){
    return;
  }
  uint64_t b = this->bytes_buffered_;
  if(len >= b){
    len = b;
    this->buffer_.clear();
    this->push_block_ = "";
    this->pop_block_ = "";
  }
  
  // update state
  this->bytes_buffered_ -= len;
  this->available_capacity_ += len;
  this->bytes_popped_ += len;

  // if both empty, the bytes_buffered in the push_block
  while(!this->buffer_.empty() || !this->pop_block_.empty()){
    // if empty, get from buffer
    if(this->pop_block_.empty()){
      this->pop_block_ = std::move(this->buffer_.front());
      this->buffer_.pop_front();
    }
    this->pop_block_size_ += len;
    len = 0;
    if(this->pop_block_size_ >= this->pop_block_.size()){
      this->pop_block_size_ -= this->pop_block_.size();
      this->pop_block_ = "";
    }
    if(this->pop_block_size_ < this->pop_block_.size()){
      break;
    }
  }
  if(len != 0 && !this->push_block_.empty()){
    len = len > this->push_block_.size() ? this->push_block_.size() : len;
    this->push_block_ = this->push_block_.substr(len);
  }
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  bool res = this->is_closed_;
  res &= (this->bytes_buffered_ == 0)? true : false;
  res &= this->push_block_.empty();
  return res;
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  return this->bytes_buffered_; // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  return this->bytes_popped_; // Your code here.
}
