#include "Socket.h"

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

Socket::Socket() = default;
Socket::~Socket() = default;

ServerSocket::ServerSocket() = default;
ServerSocket::~ServerSocket() = default;

std::unique_ptr<TcpServerSocket> createTcpServer(int port) {
    std::unique_ptr<TcpServerSocket> ret(new TcpServerSocket());
    ret->m_sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if(ret->m_sd < 0) {
        perror("socket()");
        return nullptr;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(uint16_t(port));
    addr.sin_addr.s_addr = INADDR_ANY;
    if(0 > ::bind(ret->m_sd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
        perror("bind()");
        return nullptr;
    }
    if(0 > ::listen(ret->m_sd, 10)) {
        perror("listen()");
        return nullptr;
    }
    
    return ret;
}

TcpServerSocket::~TcpServerSocket() {
    ::close(m_sd);
}

Future<std::shared_ptr<Socket> > TcpServerSocket::accept() {
    std::shared_ptr<PromiseFuturePair<std::shared_ptr<Socket> > > pf = std::make_shared<PromiseFuturePair<std::shared_ptr<Socket> > >();
    m_executor.enqueue([this,pf](){
        int sd = ::accept(m_sd, nullptr, nullptr);
        if(sd < 0) {
            perror("accept()");
            pf->set(nullptr);
            return;
        }
        std::shared_ptr<TcpSocket> ps =std::make_shared<TcpSocket>();
        ps->m_sd = sd;
        pf->set(std::move(ps));
    });
    return Future<std::shared_ptr<Socket> >(pf);
}

TcpServerSocket::TcpServerSocket()
    :m_sd(-1),
    m_executor(1)
{
    // empty
}

TcpSocket::TcpSocket()
    :m_sd(-1),
    m_executor(1)
{
    // empty
}

TcpSocket::~TcpSocket()
{
    ::close(m_sd);
}

Future<ssize_t> TcpSocket::recv(void* data, size_t len) {
    std::shared_ptr<PromiseFuturePair<ssize_t> > pf = std::make_shared<PromiseFuturePair<ssize_t> >();
    m_executor.enqueue([this,pf,data,len](){
        ssize_t ret = ::recv(m_sd, data, len, 0);
        if(ret < 0) {
            perror("recv()");
        }
        pf->set(ret);
    });
    return Future<ssize_t>(pf);
}

Future<bool> TcpSocket::send(void const* data, size_t len) {
    std::shared_ptr<PromiseFuturePair<bool> > pf = std::make_shared<PromiseFuturePair<bool> >();
    m_executor.enqueue([this,pf,data,len](){
        ssize_t ret = ::send(m_sd, data, len, 0);
        if(ret != len) {
            perror("send()");
            pf->set(false);
            return;
        }
        pf->set(true);
    });
    return Future<bool>(pf);
}

Future<bool> TcpSocket::send(std::shared_ptr<std::string const> pStr) {
    std::shared_ptr<PromiseFuturePair<bool> > pf = std::make_shared<PromiseFuturePair<bool> >();
    m_executor.enqueue([this,pf,pStr](){
        ssize_t ret = ::send(m_sd, pStr->data(), pStr->size(), 0);
        if(ret != pStr->size()) {
            perror("send()");
            pf->set(false);
            return;
        }
        pf->set(true);
    });
    return Future<bool>(pf);
}

//Future<std::unique_ptr<Socket> > tcpConnect(char const* hostname, int port);
