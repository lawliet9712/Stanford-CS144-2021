#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
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
    , _stream(capacity) {}

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
        _cache_segment(seg);
    }

    // SYN_SENT
    if (next_seqno_absolute() > 0 && next_seqno_absolute() == bytes_in_flight()) {
        // nothing need to do
        return;
    }

    // SYN_ACKED
    if (not stream_in().eof() && next_seqno_absolute() > bytes_in_flight()) {
        // if windows size equal zero , the fill window method should act like the window size is one
        if (_windows_size == 0) {
            // empty segment no need to cache
            _windows_size = 1;
        }

        string read_stream = _stream.read(_windows_size);
        if (read_stream.empty()) {
            return;
        }
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.payload() = Buffer(std::move(read_stream));
        _windows_size -= seg.length_in_sequence_space();
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _segments_out.push(seg);
        // cache the seg
        _cache_segment(seg);
    }

    // SYN_ACKED
    if (stream_in().eof() && next_seqno_absolute() < stream_in().bytes_written() + 2) {
        // stream has reached EOF, but FIN flag hasn't been sent yet. so send the fin
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.header().fin = true;
        _next_seqno += seg.length_in_sequence_space();
        _bytes_in_flight += seg.length_in_sequence_space();
        _segments_out.push(seg);
        // cache the segments
        _cache_segment(seg);
    }

    // FIN_SENT
    if (stream_in().eof() && next_seqno_absolute() ==  stream_in().bytes_written() + 2\
        && bytes_in_flight() > 0)
    {
        // nothing need to do
        return;
    }

    // FIN_ACKED
    if (stream_in().eof() && next_seqno_absolute() ==  stream_in().bytes_written() + 2\
        && bytes_in_flight() == 0)
    {
        // nothing need to do
        return;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    int left_need_ack_bytes = next_seqno() - ackno;
    cerr << "next_seqno=" << next_seqno().raw_value() << " left_need_ack_bytes=" << left_need_ack_bytes << endl;
    if (left_need_ack_bytes != 0) {
        return;
    }

    _time = 0;
    _windows_size = window_size;
    _bytes_in_flight = left_need_ack_bytes;
    // pop the cache segments;
    cerr << "pop cache segments" << endl;
    _segments_cache.pop();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_segments_cache.empty()) {
        return;
    }

    _time += ms_since_last_tick;
     if (_time >= _initial_retransmission_timeout && _retx_times <= TCPConfig::MAX_RETX_ATTEMPTS) {
         _segments_out.push(_segments_cache.front());
         _initial_retransmission_timeout *= 2;
         _retx_times += 1;
         _time = 0;
     }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retx_times; }

void TCPSender::send_empty_segment() {
    _segments_out.push(TCPSegment());
}

void TCPSender::_cache_segment(TCPSegment& seg) {
    cerr << "_cache_segment cache segments" << endl;
    _segments_cache.push(seg);
}