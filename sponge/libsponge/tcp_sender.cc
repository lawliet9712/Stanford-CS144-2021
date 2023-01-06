#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
#include <vector>
#include <iterator>
// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
{
    _default_initial_retransmission_timeout = retx_timeout;
}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    // CLOSED
    if (next_seqno_absolute() == 0) {
        // need to send the syn
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = _isn;
        _segments_out.push(seg);
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        // cache the segments
        _cache_segment(next_seqno_absolute() , seg);
    }

    // SYN_ACKED
    if (not stream_in().eof() && next_seqno_absolute() > bytes_in_flight()) {
        // if windows size equal zero , the fill window method should act like the window size is one
        size_t max_seg_size = TCPConfig::MAX_PAYLOAD_SIZE;
        size_t remaining_wsz = _peer_windows_size ? _peer_windows_size : 1;
        size_t flight_size = bytes_in_flight();
        if (remaining_wsz < flight_size) {
            return;
        }
        remaining_wsz -= flight_size;
        string read_stream = _stream.read(min(remaining_wsz, max_seg_size));
        
        while (!read_stream.empty()) {
            TCPSegment seg;
            seg.header().seqno = next_seqno();
            seg.payload() = Buffer(std::move(read_stream));
            remaining_wsz -= seg.length_in_sequence_space();
            if (stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2 && remaining_wsz >= 1) {
                seg.header().fin = true;
                remaining_wsz -= 1;
            }

            _next_seqno += seg.length_in_sequence_space();
            _bytes_in_flight += seg.length_in_sequence_space();
            _segments_out.push(seg);

            // cache the seg
            _cache_segment(next_seqno_absolute(), seg);
            read_stream = _stream.read(min(remaining_wsz, max_seg_size));
        }
    }

    // SYN_ACKED
    if (stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2) {
        // stream has reached EOF, but FIN flag hasn't been sent yet. so send the fin
        size_t remaining_wsz = _peer_windows_size ? _peer_windows_size : 1;
        size_t flight_size = bytes_in_flight();
        if (remaining_wsz <= flight_size) {
            return;
        }

        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.header().fin = true;
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _windows_size -= seg.length_in_sequence_space();
        _segments_out.push(seg);
        // cache the segments
        _cache_segment(next_seqno_absolute(), seg);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    int left_need_ack_bytes = next_seqno() - ackno;
    if (left_need_ack_bytes < 0) {
        return;
    }

    // pop the cache segments;
    vector<uint64_t> remove_acknos;
    for (auto & seg : _segments_cache) {
        uint64_t abs_ackno = unwrap(ackno, _isn, seg.first);
        if (seg.first <= abs_ackno) {
            remove_acknos.push_back(seg.first);
        }
    }

    for (auto & remove_ackno : remove_acknos) {
        _segments_cache.erase(remove_ackno);
    }

    if (window_size > _peer_windows_size) {
        // extend windows size
        _windows_size += window_size - _peer_windows_size;
    }
    _peer_windows_size = window_size;

    // valid ackno
    if (!remove_acknos.empty()) {
        //cerr << "ack received window_size=" << window_size << endl;
        _windows_size = max(window_size, uint16_t(1));
        _bytes_in_flight = left_need_ack_bytes;
        _time = 0;
        _retx_times = 0;
        _initial_retransmission_timeout = _default_initial_retransmission_timeout;
        fill_window();
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_segments_cache.empty()) {
        return;
    }

    _time += ms_since_last_tick;
     if (_time >= _initial_retransmission_timeout && _retx_times <= TCPConfig::MAX_RETX_ATTEMPTS) {
        _segments_out.push(_segments_cache.begin()->second);
        _time = 0;
        if (_peer_windows_size != 0) {
            _initial_retransmission_timeout *= 2;
            _retx_times += 1;
        }
     }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retx_times; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}

void TCPSender::_cache_segment(uint64_t ackno, TCPSegment& seg) {
    _segments_cache[ackno] = seg;
}