#include "stream_reassembler.hh"

#include <vector>
#include <iterator>
#include <map>
#include <iostream>
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _assemble_idx(0),\
 _output(capacity), _capacity(capacity), _eof_idx(-1), _segments({}) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        _eof_idx = data.size() + index;
    }
    // not expect segement, cache it
    if (index > _assemble_idx) {
        _merge_segment(index, data);
        return;
    } 

    // expect segment, write it to ByteStream
    int start_pos = _assemble_idx - index;
    int write_cnt = data.size() - start_pos;
    // not enough space
    if (write_cnt < 0) {
        return;
    }
    _assemble_idx += _output.write(data.substr(start_pos, write_cnt));

    // search the next segment
    std::vector<size_t> pop_list;
    for (auto segment : _segments) {
        // already process or empty string
        if (segment.first + segment.second.size() <= _assemble_idx || segment.second.size() == 0) {
            pop_list.push_back(segment.first);
            continue;
        } 

        // not yet
        if (_assemble_idx < segment.first) {
            continue;
        }

        start_pos = _assemble_idx - segment.first;
        write_cnt = segment.second.size() - start_pos;
        _assemble_idx += _output.write(segment.second.substr(start_pos, write_cnt));
        pop_list.push_back(segment.first);
    }
    // remove the useless segment
    for (auto segment_id : pop_list) {
        _segments.erase(segment_id);
    }

    if (empty() && _assemble_idx == _eof_idx) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t total_size = 0;
    for (const auto& segment : _segments) {
        total_size += segment.second.size();
    }
    return total_size;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }


void StreamReassembler::_merge_segment(size_t index, const std::string& data) {
    size_t data_left = index;
    size_t data_right = index + data.size();
    std::string data_copy = data;
    std::vector<size_t> remove_list;
    bool should_cache = true;
    for (auto segment : _segments)
    {
        size_t seg_left = segment.first;
        size_t seg_right = segment.first + segment.second.size();
        //|new_index     |segment.first            |segment.second.size()        |merge_segment.size
        if (data_left <= seg_left && data_right >= seg_left) {
            if (data_right >= seg_right) {
                remove_list.push_back(segment.first);
                continue;
            }
            
            if (data_right < seg_right) {
                data_copy = data_copy.substr(0, seg_left - data_left) + segment.second;
                data_right = data_left + data_copy.size();
                remove_list.push_back(segment.first);
            }
        } 

        if (data_left > seg_left && data_left <= seg_right) {
            if (data_right <= seg_right) {
                should_cache = false;
            }

            if (data_right > seg_right) {
                data_copy = segment.second.substr(0, data_left - seg_left) + data_copy;
                data_left = seg_left;
                data_right = data_left + data_copy.size();
                remove_list.push_back(segment.first);
            }
        }
    }
    
    // remove overlap data
    for (auto remove_idx : remove_list) {
        _segments.erase(remove_idx);
    }
    if (should_cache)
        _segments[data_left] = data_copy;
}