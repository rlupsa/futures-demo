#pragma once

#include "Future.h"
#include "ThreadPool.h"

/** A connection socket offering asynchronous operations.
 * 
 * @note This implementation is mostly for demo purposes. A better implementation would use poll() or select()
 * */
class Socket {
public:
    Socket();
    Socket(Socket const&) = delete;
    Socket(Socket&&) = delete;
    Socket* operator=(Socket const&) = delete;
    Socket* operator=(Socket&&) = delete;
    virtual ~Socket();

    /**
     * @brief Launches a receive from the socket. The future will be completed only when at least one byte has been read or the other end has been closed
     * @param data pointer to where the read data is to be stored
     * @param len the maximum number of bytes to read
     * @return a future that will receive the number of bytes actually read, 0 on end-of-file, or a negative number on error
     */
    virtual Future<ssize_t> recv(void* data, size_t len) = 0;
    
    /**
     * @brief Launches sending data to the socket
     * @param data
     * @param len
     * @return a future that will be set to true on success or on false on failure
     */
    virtual Future<bool> send(void const* data, size_t len) = 0;
    virtual Future<bool> send(std::shared_ptr<std::string const> pStr) = 0;
};

/** A server TCP listening socket offering asynchronous operations.
 * 
 * @note This implementation is mostly for demo purposes. A better implementation would use poll() or select()
 * */
class ServerSocket {
public:
    ServerSocket();
    ServerSocket(Socket const&) = delete;
    ServerSocket(Socket&&) = delete;
    ServerSocket* operator=(ServerSocket const&) = delete;
    ServerSocket* operator=(ServerSocket&&) = delete;

    virtual ~ServerSocket();

    /** Starts waiting for a new connection from a client. Returns a future that will complete when a connection is accepted.
     * */
    virtual Future<std::shared_ptr<Socket> > accept() = 0;
};


class TcpSocket;
class TcpServerSocket;

std::unique_ptr<TcpServerSocket> createTcpServer(int port);

/** @brief Asynchronously connects to a remote server. Returns a future that will complete when the connection is established.
 * */
Future<std::unique_ptr<Socket> > tcpConnect(char const* hostname, int port);

class TcpSocket : public Socket {
public:
    ~TcpSocket() override;

    Future<ssize_t> recv(void* data, size_t len) override;
    
    Future<bool> send(void const* data, size_t len) override;
    Future<bool> send(std::shared_ptr<std::string const> pStr) override;

    TcpSocket();

private:
    friend Future<std::unique_ptr<Socket> > tcpConnect(char const* hostname, int port);
    friend class TcpServerSocket;

    
    int m_sd;
    ThreadPool m_executor;
};

class TcpServerSocket : public ServerSocket {
public:
    ~TcpServerSocket() override;
    
    Future<std::shared_ptr<Socket> > accept() override;

private:
    friend std::unique_ptr<TcpServerSocket> createTcpServer(int port);

    TcpServerSocket();
    
    int m_sd;
    ThreadPool m_executor;
};
