#include "tcp_receiver.hh"

#include <iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    // 1. process syn flag
    if (seg.header().syn) {
        if (!_received_syn) {
            _received_syn = true;
            _isn = seg.header().seqno;
        }
    }
    
    // 2. push payload
    if (ackno().has_value() && !_reassembler.stream_out().input_ended()) {
        // relative seqno to stream index
        uint64_t stream_index = unwrap(seg.header().seqno, _isn, _checkpoint);
        // stream index should in the window
        uint64_t abs_ackno = unwrap(ackno().value(), _isn, _checkpoint);
        if (stream_index + seg.length_in_sequence_space() <= abs_ackno || stream_index >= abs_ackno + window_size()) {
            return;
        }

        if (!seg.header().syn) {
            stream_index -= 1; // ignore syn flag;
        }
        _reassembler.push_substring(seg.payload().copy(), stream_index, seg.header().fin);
        _checkpoint = _reassembler.stream_out().bytes_written();
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    // next_write + 1 ,because syn flag will not push in stream
    size_t next_write = _reassembler.stream_out().bytes_written() + 1;
    next_write = _reassembler.stream_out().input_ended() ? next_write + 1 : next_write;
    return !_received_syn ? optional<WrappingInt32>() : wrap(next_write, _isn);
}

size_t TCPReceiver::window_size() const {
    return _reassembler.stream_out().remaining_capacity();
}
