#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`
#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

bool address_equal(const EthernetAddress& a, const EthernetAddress& b) {
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    // find and send ip datagram
    EthernetFrame frame;
    auto& address = _learn_address[next_hop_ip];
    if (address.is_valid) {
        //cerr << "[send_datagram] send data gram" << endl;
        _send_datagram(dgram, address.learn_address);
    }
    // send arp message request
    else {
        /*
        don’t want to flood the network with ARP requests. If the network interface already sent 
        an ARP request about the same IP address in the last five seconds, don’t send a second 
        request—just wait for a reply to the first one. Again, queue the datagram until you 
        learn the destination Ethernet address.
        */
        //cerr << " address query time=" << address.query_pass_time << endl;
        if (!address.can_query()) {
            address.datagrams.push(dgram);
            return;
        }

        //cerr << "[send_datagram] no found address map, send datagram" << endl;
        // can not query again in 5s, reset query pass time
        address.query_pass_time -= LearnAddress::QUERY_VAILD_TIMEOUT;

        // setup header
        EthernetHeader& header = frame.header();
        header.dst = ETHERNET_BROADCAST;
        header.src = _ethernet_address;
        header.type = EthernetHeader::TYPE_ARP;

        // setup arp payload
        ARPMessage arp;
        arp.opcode = ARPMessage::OPCODE_REQUEST;
        arp.sender_ethernet_address = _ethernet_address;
        arp.sender_ip_address = _ip_address.ipv4_numeric();
        arp.target_ip_address = next_hop_ip;
        frame.payload() = arp.serialize();

        // cache datagram
        address.datagrams.push(dgram);
        _frames_out.push(frame);
    }

}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    uint32_t local_ip = _ip_address.ipv4_numeric();
    //cerr << "recv_frame " << frame.header().to_string() << " local_ip=" << local_ip << endl;
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram datagram;
        ParseResult result = datagram.parse(frame.payload());
        if (result != ParseResult::NoError || frame.header().dst != _ethernet_address) {
            //cerr << "recv_frame " << frame.header().to_string() << " local_ip=" << local_ip << " dst=" << datagram.header().dst << endl;
            return {};
        }
        return datagram;
    }

    if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp;
        ParseResult result = arp.parse(frame.payload());
        if (result != ParseResult::NoError) {
            return {};
        }
        /* 
        If the inbound frame is ARP, parse the payload as an ARPMessage and, if successful, 
        remember the mapping between the sender’s IP address and Ethernet address for 30 seconds.
        */
        auto& address = _learn_address[arp.sender_ip_address];
        address.is_valid = true;
        address.learn_address = arp.sender_ethernet_address;
        address.learn_pass_time = 0;
        address.query_pass_time = LearnAddress::QUERY_VAILD_TIMEOUT;

        //cerr << "recv arp " << arp.to_string() << endl;
        // received arp request, reply host ip and mac
        if (arp.opcode == ARPMessage::OPCODE_REQUEST && arp.target_ip_address == local_ip) {
            // setup header
            EthernetFrame reply_frame;
            EthernetHeader& header = reply_frame.header();
            header.dst = frame.header().src;
            header.src = _ethernet_address;
            header.type = EthernetHeader::TYPE_ARP;

            // setup arp payload
            ARPMessage reply_arp;
            reply_arp.opcode = ARPMessage::OPCODE_REPLY;
            reply_arp.sender_ethernet_address = _ethernet_address;
            reply_arp.sender_ip_address = _ip_address.ipv4_numeric();
            reply_arp.target_ip_address = arp.sender_ip_address;
            reply_arp.target_ethernet_address = arp.sender_ethernet_address;
            reply_frame.payload() = reply_arp.serialize();

            // send it
            _frames_out.push(reply_frame);
        }

        // received arp reply
        if (arp.opcode == ARPMessage::OPCODE_REPLY) {
            // re-sent datagrams
            while (!address.datagrams.empty()) {
                InternetDatagram& dgram = address.datagrams.front();
                _send_datagram(dgram, address.learn_address);
                address.datagrams.pop();
            }
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    for (auto& address : _learn_address) {
        LearnAddress& learn_address = address.second;
        learn_address.tick(ms_since_last_tick);
        if (learn_address.learn_timeout()) {
            learn_address.reset();      
        }
    }
 }

void NetworkInterface::_send_datagram(const InternetDatagram &dgram, EthernetAddress ethernet_address) {
    EthernetFrame frame;
    EthernetHeader& header = frame.header();
    header.dst = ethernet_address;
    header.src = _ethernet_address;
    header.type = EthernetHeader::TYPE_IPv4;
    frame.payload() = dgram.serialize();
    _frames_out.push(frame);
}