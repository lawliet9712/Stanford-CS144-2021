#include "tcp_sponge_socket.hh"
#include "util.hh"
#include "address.hh"

#include <cstdlib>
#include <iostream>
#include <arpa/inet.h>

using namespace std;

/* c style implement
void get_URL(const string &host, const string &path) {
    // Your code here.
    // 1. init socket
    struct hostent* h = gethostbyname(host.c_str());
    if (h == nullptr)
    {
        cerr << "Function called: get_URL(" << host << ", " << path << ") Failed, not found hostname\n";
        return;
    }

    cout << "Host Addr =" << h->h_name << " Ip=" << inet_ntoa(*((struct in_addr *)h->h_addr)) << "\n";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = *(struct in_addr*)h->h_addr;
    server_addr.sin_port = htons(80);
    
    // 2. connect to host
    if (connect(sock, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == -1)
    {
        cerr << "Function called: get_URL(" << host << ", " << path << ") Failed, conncet failed\n";
        return;
    }
    cout << "Connect finish ..." << "\n";

    // 3. send request path
    // extern ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);
    string buf = "GET " + path + " HTTP/1.1\r\n";
    ssize_t send_size = send(sock, buf.c_str(), buf.size(), 0);
    buf = "HOST: " + host + "\r\n";
    send_size = send(sock, buf.c_str(), buf.size(), 0);
    buf = "Connection: close\r\n";
    send_size = send(sock, buf.c_str(), buf.size(), 0);
    buf = "\r\n";
    send_size = send(sock, buf.c_str(), buf.size(), 0);

    // 4. read response data
    char ch;
    while (recv(sock, &ch, sizeof(ch), 0) != 0)
    {
        cout << ch;
    }
    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    //cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    //cerr << "Warning: get_URL() has not been implemented yet.\n";
}
*/

// should use TCPClient
void get_URL(const string &host, const string &path) {
    // Your code here.
    // 1. init socket
    FullStackSocket socket;
    
    // 2. connect to host
    socket.connect(Address(host, "http"));

    // 3. send request path
    // extern ssize_t send (int __fd, const void *__buf, size_t __n, int __flags);
    socket.write("GET " + path + " HTTP/1.1\r\n");
    socket.write("HOST: " + host + "\r\n");
    socket.write("Connection: close\r\n");
    socket.write("\r\n");

    // 4. read response data
    while(!socket.eof())
        cout << socket.read();
    // You will need to connect to the "http" service on
    // the computer whose name is in the "host" string,
    // then request the URL path given in the "path" string.

    // Then you'll need to print out everything the server sends back,
    // (not just one call to read() -- everything) until you reach
    // the "eof" (end of file).

    socket.wait_until_closed();
    //cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    //cerr << "Warning: get_URL() has not been implemented yet.\n";
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            cerr << "Usage: " << argv[0] << " HOST PATH\n";
            cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const string host = argv[1];
        const string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const exception &e) {
        cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
