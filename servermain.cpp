// servermain.cpp
// Lab 2a - Simple TCP Server (TEXT TCP 1.0)
// Build: g++ -std=c++17 -O2 -Wall -o server servermain.cpp

#include <iostream>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using std::string;
using std::cout;
using std::cerr;
using std::endl;

// ------------------ Utilities ------------------

static bool send_all(int fd, const string &msg) {
    const char *ptr = msg.c_str();
    size_t left = msg.size();
    while (left > 0) {
        ssize_t n = send(fd, ptr, left, 0);
        if (n <= 0) return false;
        ptr += n;
        left -= (size_t)n;
    }
    return true;
}

static string recv_line(int fd) {
    string line;
    char c;
    while (true) {
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return {};
        line.push_back(c);
        if (c == '\n') break;
    }
    return line;
}

static string trim_newline(string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    return s;
}

static bool parse_hostport(const string &arg, string &host, string &port) {
    if (!arg.empty() && arg.front() == '[') {
        auto pos = arg.find("]:");
        if (pos == string::npos) return false;
        host = arg.substr(1, pos - 1);
        port = arg.substr(pos + 2);
        return true;
    }
    auto pos = arg.rfind(':');
    if (pos == string::npos) return false;
    host = arg.substr(0, pos);
    port = arg.substr(pos + 1);
    return true;
}

// ------------------ Random helpers ------------------

static string pick_int_op() {
    static const char *ops[] = {"add","sub","mul","div"};
    return ops[rand() % 4];
}
static string pick_float_op() {
    static const char *ops[] = {"fadd","fsub","fmul","fdiv"};
    return ops[rand() % 4];
}

static std::pair<int,int> random_ints() {
    int a = (rand() % 20001) - 10000;
    int b = (rand() % 20001) - 10000;
    if (b == 0) b = 1;
    return {a,b};
}
static std::pair<double,double> random_floats() {
    double a = ((rand() % 1000000) / 1000000.0) * 200.0 - 100.0;
    double b = ((rand() % 1000000) / 1000000.0) * 200.0 - 100.0;
    if (fabs(b) < 1e-9) b = 1e-6;
    return {a,b};
}

static string fmt_double(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%8.8g", v);
    return string(buf);
}

// ------------------ Main ------------------

int main(int argc, char **argv) {
    if (argc != 2) {
        cerr << "Usage: ./server <IP|DNS>:<PORT>\n";
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    srand((unsigned)time(nullptr) ^ getpid());

    string host, port;
    if (!parse_hostport(argv[1], host, port)) {
        cerr << "Bad address format\n";
        return 1;
    }

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int rc = getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rc) << endl;
        return 1;
    }

    int listenfd = -1;
    for (auto *ai = res; ai; ai = ai->ai_next) {
        listenfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (listenfd < 0) continue;
        int yes = 1;
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
        if (bind(listenfd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(listenfd);
        listenfd = -1;
    }
    freeaddrinfo(res);

    if (listenfd < 0) {
        cerr << "bind failed\n";
        return 1;
    }
    if (listen(listenfd, 5) != 0) {
        cerr << "listen failed\n";
        close(listenfd);
        return 1;
    }

    cout << "server ready waiting for connections, on " << port << endl;

    while (true) {
        struct sockaddr_storage cli_addr{};
        socklen_t cli_len = sizeof(cli_addr);
        int clientfd = accept(listenfd, (struct sockaddr*)&cli_addr, &cli_len);
        if (clientfd < 0) {
            if (errno == EINTR) continue;
            cerr << "accept failed\n";
            break;
        }

        struct timeval tv{5,0};
        setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        // Step 1: Send protocol
        if (!send_all(clientfd, "TEXT TCP 1.0\n\n")) {
            close(clientfd);
            continue;
        }

        // Step 2: Wait for OK
        string line = recv_line(clientfd);
        if (trim_newline(line) != "OK") {
            close(clientfd);
            continue;
        }

        // Step 3: Generate assignment
        bool useFloat = (rand() % 2 == 0);
        string assignment;
        string op;
        int ia=0, ib=0;
        double fa=0, fb=0;

        if (useFloat) {
            op = pick_float_op();
            auto pr = random_floats();
            fa = pr.first; fb = pr.second;
            assignment = op + " " + fmt_double(fa) + " " + fmt_double(fb) + "\n";
        } else {
            op = pick_int_op();
            auto pr = random_ints();
            ia = pr.first; ib = pr.second;
            std::ostringstream oss;
            oss << op << " " << ia << " " << ib << "\n";
            assignment = oss.str();
        }

        if (!send_all(clientfd, assignment)) {
            close(clientfd);
            continue;
        }

        // Step 4: Client answer
        string ans = trim_newline(recv_line(clientfd));
        if (ans.empty()) {
            send_all(clientfd, "ERROR TO\n");
            close(clientfd);
            continue;
        }

        // Step 5: Verify
        bool ok = false;
        if (useFloat) {
            double exp=0;
            if (op=="fadd") exp=fa+fb;
            else if (op=="fsub") exp=fa-fb;
            else if (op=="fmul") exp=fa*fb;
            else if (op=="fdiv") exp=(fabs(fb)<1e-12?0:fa/fb);
            try {
                double client = std::stod(ans);
                if (fabs(exp-client) < 0.0001) ok = true;
            } catch(...) {}
        } else {
            long long exp=0;
            if (op=="add") exp=ia+ib;
            else if (op=="sub") exp=ia-ib;
            else if (op=="mul") exp=ia*ib;
            else if (op=="div") exp=(ib==0?0:ia/ib);
            try {
                size_t idx=0;
                long long client=std::stoll(ans,&idx);
                if (idx==ans.size() && client==exp) ok = true;
            } catch(...) {}
        }

        if (ok) send_all(clientfd, "OK\n");
        else send_all(clientfd, "ERROR\n");

        close(clientfd);
    }

    close(listenfd);
    return 0;
}