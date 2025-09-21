#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

// Print everything to stdout (as required by assignment)
static void out(const string &s) {
    cout << s << flush;
}

// Read one line from socket (until '\n')
static string recv_line(int fd) {
    string buf;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return {}; // connection closed or error
        buf.push_back(c);
        if (c == '\n') break;
    }
    return buf;
}

// Split "host:port" or "[ipv6]:port"
static bool split_host_port(const string &input, string &host, string &port) {
    if (!input.empty() && input.front() == '[') {
        auto pos = input.find("]:");
        if (pos == string::npos) return false;
        host = input.substr(1, pos - 1);
        port = input.substr(pos + 2);
        return true;
    }
    auto pos = input.rfind(':');
    if (pos == string::npos) return false;
    host = input.substr(0, pos);
    port = input.substr(pos + 1);
    return true;
}

// Connect to server (IPv4 or IPv6)
static int connect_server(const string &host, const string &port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    if (getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0)
        return -1;

    int sock = -1;
    for (auto rp = res; rp != nullptr; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    return sock;
}

// Check banner for "TEXT TCP"
static bool check_banner(int sock) {
    string banner;
    char buf[512];
    while (true) {
        ssize_t n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break;
        banner.append(buf, buf + n);
        if (banner.find("\n\n") != string::npos || banner.find("\r\n\r\n") != string::npos)
            break;
        if (banner.size() > 4096) break;
    }

    istringstream ss(banner);
    string line;
    while (getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("TEXT TCP", 0) == 0) {
            send(sock, "OK\n", 3, 0);
            return true;
        }
        if (line.empty()) break;
    }
    return false;
}

// Perform calculation
static string calculate(const string &task) {
    istringstream iss(task);
    string op, a_str, b_str;
    if (!(iss >> op >> a_str >> b_str)) return {};

    try {
        if (op == "add" || op == "sub" || op == "mul" || op == "div") {
            long long a = stoll(a_str), b = stoll(b_str), r = 0;
            if (op == "add") r = a + b;
            else if (op == "sub") r = a - b;
            else if (op == "mul") r = a * b;
            else if (op == "div") r = (b == 0 ? 0 : a / b);
            return to_string(r);
        }
        if (op == "fadd" || op == "fsub" || op == "fmul" || op == "fdiv") {
            double a = stod(a_str), b = stod(b_str), r = 0;
            if (op == "fadd") r = a + b;
            else if (op == "fsub") r = a - b;
            else if (op == "fmul") r = a * b;
            else if (op == "fdiv") r = (b == 0 ? 0.0 : a / b);
            char buf[64];
            snprintf(buf, sizeof(buf), "%8.8g", r);
            return string(buf);
        }
    } catch (...) {
        return {};
    }
    return {};
}

int main(int argc, char **argv) {
    if (argc != 2) {
        out("Usage: ./client <host:port>\n");
        return 1;
    }

    string host, port;
    if (!split_host_port(argv[1], host, port)) {
        out("ERROR: RESOLVE ISSUE\n");
        return 1;
    }

    cout << "Host " << host << ", and port " << port << "." << endl;

    int sock = connect_server(host, port);
    if (sock < 0) {
        out("ERROR: CANT CONNECT TO " + host + "\n");
        return 1;
    }

    if (!check_banner(sock)) {
        cout << "ERROR" << endl;
        close(sock);
        return 0;
    }

    string task_line = recv_line(sock);
    if (task_line.empty()) {
        out("ERROR: RESOLVE ISSUE\n");
        close(sock);
        return 1;
    }
    if (!task_line.empty() && (task_line.back() == '\n' || task_line.back() == '\r'))
        task_line.erase(task_line.find_last_not_of("\r\n") + 1);

    cout << "ASSIGNMENT: " << task_line << endl;

    string result = calculate(task_line);
    if (result.empty()) {
        out("ERROR: RESOLVE ISSUE\n");
        close(sock);
        return 1;
    }

    string msg = result + "\n";
    send(sock, msg.c_str(), msg.size(), 0);

    string reply = recv_line(sock);
    if (!reply.empty() && (reply.back() == '\n' || reply.back() == '\r'))
        reply.erase(reply.find_last_not_of("\r\n") + 1);

    if (reply == "OK")
        cout << "OK (myresult=" << result << ")" << endl;
    else
        cout << "ERROR (myresult=" << result << ")" << endl;

    close(sock);
    return 0;
}