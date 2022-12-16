#include "byte_stream.hh"
#include <iostream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity): 
     _input_ended(false),
     _begin(0),
     _end(capacity),
     _total_written(0),
     _total_read(0),
     _capacity(capacity),
     _data({})
{
    _data.reserve(capacity);
}

size_t ByteStream::write(const string &data) {
    int write_size = min(data.size(), remaining_capacity());
    for (int i = _begin, j = 0; i < _begin + write_size; i++, j++) {
        int real_index = i % _capacity;
        _data[real_index] = data[j];   
    }
    _begin += write_size;
    _total_written += write_size;
    return write_size;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string peek_result;
    int peek_size = min(buffer_size(), len);
    for (int i = _end; i < _end + peek_size; i++) {
        int real_index = i % _capacity;
        peek_result += _data[real_index];
    }
    return peek_result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    int pop_size = min(buffer_size(), len);
    _end += pop_size;
    _total_read += pop_size;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read_result;
    int read_size = min(buffer_size(), len);
    for (int i = _end; i < _end + read_size; i++) {
        int real_index = i % _capacity;
        read_result += _data[real_index];
    }
    _total_read += read_size;
    _end += read_size;
    return read_result;
}

void ByteStream::end_input() { _input_ended = true; }

bool ByteStream::input_ended() const { return _input_ended; }

size_t ByteStream::buffer_size() const { return _begin - (_end - _capacity); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _input_ended && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _total_written; }

size_t ByteStream::bytes_read() const { return _total_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
