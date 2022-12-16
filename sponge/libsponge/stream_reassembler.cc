#include "stream_reassembler.hh"

#include <vector>
#include <iterator>
#include <map>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _assemble_idx(0), _output(capacity), _capacity(capacity), _segments({}) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _output.end_input();
    }
    // not enough space
    if (_output.buffer_size() + data.size() + unassembled_bytes() > _capacity) {
        return;
    }
    // already process
    else if (index + data.size() <= _assemble_idx) {
        return;
    }
    // not expect segement, cache it
    else if (index > _assemble_idx) {
        _segments[index] = data;
        return;
    } 
    
    // expect segment, write it to ByteStream
    int start_pos = _assemble_idx - index;
    int write_cnt = data.size() - start_pos;
    _output.write(data.substr(start_pos, write_cnt));
    _assemble_idx += write_cnt;
    std::vector<size_t> pop_list;
    // already sorted
    for (auto segment : _segments) {
        if (segment.first + segment.second.size() <= _assemble_idx) {
            pop_list.push_back(segment.first);
        } else {
            start_pos = _assemble_idx - segment.first;
            write_cnt = data.size() - start_pos;
            _output.write(segment.second.substr(start_pos, write_cnt));
            _assemble_idx += write_cnt;
            pop_list.push_back(segment.first);
        }
    }
    for (auto segment_id : pop_list) {
        _segments.erase(segment_id);
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t total_size = 0;
    for (const auto& segment : _segments) {
        total_size += segment.second.size();
    }
    return total_size;
}

bool StreamReassembler::empty() const { return _output.buffer_size() + unassembled_bytes() == 0; }
