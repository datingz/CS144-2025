#include "byte_stream.hh"
#include "debug.hh"
#include <iostream>


using namespace std;

  // uint64_t capacity_;
  // bool error_ {};
  // std::deque<std::string> buffer_;
  // bool is_closed_;
  // uint64_t available_capacity_;
  // uint64_t bytes_buffered_;
  // uint64_t bytes_popped_;
  // uint64_t bytes_pushed_;
ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ), buffer_( 0 ), is_closed_( false ), available_capacity_( capacity ), bytes_buffered_( 0 ), bytes_popped_( 0 ), bytes_pushed_( 0 ) {}

// Push data to stream, but only as much as available capacity allows.
void Writer::push( string data )
{
  // Your code here (and in each method below)
  // debug( "Writer::push({}) not yet implemented", data );
  uint64_t a = this->available_capacity();
  uint64_t s = data.size();
  if(s == 0){
    return;
  }

  if(s > a){
    string sub = data.substr(0, a);
    if(!sub.empty()){
      this->buffer_.push_back(sub);
    }
    s = a;
  } else{
    this->buffer_.push_back(data);   
  }
  // updata state 
  this->bytes_pushed_ += s;
  this->bytes_buffered_ += s;
  this->available_capacity_ -= s;
}

// Signal that the stream has reached its ending. Nothing more will be written.
void Writer::close()
{
  // debug( "Writer::close() not yet implemented" );
  this->is_closed_ = true;
}

// Has the stream been closed?
bool Writer::is_closed() const
{
  // debug( "Writer::is_closed() not yet implemented" );
  return this->is_closed_; // Your code here.
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
  return this->buffer_.front(); // Your code here.
}

// Remove `len` bytes from the buffer.
void Reader::pop( uint64_t len )
{
  // debug( "Reader::pop({}) not yet implemented", len );
  uint64_t b = this->bytes_buffered_;
  if(len >= b){
    len = b;
    this->buffer_.clear();
  }
  
  // update state
  this->bytes_buffered_ -= len;
  this->available_capacity_ += len;
  this->bytes_popped_ += len;

  while(len != 0 && !this->buffer_.empty()){
    uint64_t s = this->buffer_.front().size();
    if(s > len){ 
      string sub = this->buffer_.front().substr(len);
      len = 0;
      this->buffer_.pop_front();
      if(!sub.empty()){
        this->buffer_.push_front(sub);
      }
    } else{
      len -= s;
      this->buffer_.pop_front();
    }
  }
}

// Is the stream finished (closed and fully popped)?
bool Reader::is_finished() const
{
  bool res = this->is_closed_;
  res &= (this->bytes_buffered_ == 0)? true : false;
  return res; // Your code here.
}

// Number of bytes currently buffered (pushed and not popped)
uint64_t Reader::bytes_buffered() const
{
  //debug( "Reader::bytes_buffered() not yet implemented" );
  return this->bytes_buffered_; // Your code here.
}

// Total number of bytes cumulatively popped from stream
uint64_t Reader::bytes_popped() const
{
  // debug( "Reader::bytes_popped() not yet implemented" );
  return this->bytes_popped_; // Your code here.
}
