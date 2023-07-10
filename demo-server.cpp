#include "Continuations.h"
#include "Socket.h"
#include "FutureWaiter.h"

#include <algorithm>
#include <string>
#include <string.h>

#include <iostream>

/* A demo application for the "Future" mechanism.
 * It represents a server that reads pairs numbers, in text format, and responds with their sums.
 * */

class BufferedReader {
private:
    enum class ReadIntState {
        beforeFirstDigit, readingNumber, atEnd, error
    };
    struct ReadIntData {
        int tmpVal = 0;
        ReadIntState state = ReadIntState::beforeFirstDigit;
    };
public:
    BufferedReader(Executor* pExecutor, Socket* pSocket)
        :m_pExecutor(pExecutor),
        m_pSocket(pSocket),
        m_buf(new char[5]),
        m_bufPos(m_buf.get()),
        m_bufEndData(m_bufPos),
        m_bufEndAlloc(m_buf.get()+5),
        m_eof(false)
        {}

    /**
     * @brief Reads an integer
     * @return the read number, or -1 on error
     */
    Future<int> readInt() {
        std::shared_ptr<ReadIntData> pData = std::make_shared<ReadIntData>();
        Future<bool> loopResult = executeAsyncLoop<bool>(*m_pExecutor,
            [](bool cont){return cont;},
            [this,pData](bool)->Future<bool> {
                while(m_bufPos < m_bufEndData) {
                    char c = *m_bufPos;
                    if(c >= '0' && c <= '9') {
                        pData->state = ReadIntState::readingNumber;
                        pData->tmpVal = 10*pData->tmpVal + (c-'0');
                    } else if(c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                        if(pData->state == ReadIntState::readingNumber) {
                            pData->state = ReadIntState::atEnd;
                            return completedFuture<bool>(false);
                        }
                    } else {
                        pData->state = ReadIntState::error;
                        return completedFuture<bool>(false);
                    }
                    ++m_bufPos;
                }
                if(m_eof) {
                    if(pData->state == ReadIntState::readingNumber) {
                        pData->state = ReadIntState::atEnd;
                    } else {
                        pData->state = ReadIntState::error;
                    }
                    return completedFuture<bool>(false);
                }
                return readMore();
            },
            true);
        return addContinuation<int>(*m_pExecutor, [this, pData](bool)->int {
                if(pData->state == ReadIntState::atEnd) {
                    return pData->tmpVal;
                } else {
                    return -1;
                }
            }, loopResult);
    }

private:
    /**
     * @brief Launches a read from the underlying socket and into the buffer
     * @return A future that will be set to true on success or false on error
     */
    Future<bool> readMore() {
        if(m_bufPos < m_bufEndData) {
            ::memmove(m_buf.get(), m_bufPos, m_bufEndData-m_bufPos);
            m_bufEndData = m_buf.get() + (m_bufEndData-m_bufPos);
        } else {
            m_bufEndData = m_buf.get();
        }
        m_bufPos = m_buf.get();
        Future<ssize_t> recvBytesFuture = m_pSocket->recv(m_bufEndData, m_bufEndAlloc-m_bufEndData);
        return addContinuation<bool>(*m_pExecutor, [this](ssize_t recvBytes)->bool {
            if(recvBytes < 0) {
                ::perror("recv()");
                return false;
            }
            m_bufEndData += recvBytes;
            if(recvBytes == 0) {
                m_eof = true;
            }
            return true;
        }, recvBytesFuture);
    }

    
    

    Executor* m_pExecutor;
    Socket* m_pSocket;
    std::unique_ptr<char[]> m_buf;
    char* m_bufPos;
    char* m_bufEndData;
    char* m_bufEndAlloc;
    bool m_eof;
};

class ClientHandler {
public:
    ClientHandler(Executor* pExecutor, std::shared_ptr<Socket> pSocket)
        :m_pExecutor(pExecutor),
        m_pSocket(std::move(pSocket)),
        m_reader(m_pExecutor, m_pSocket.get())
        {}

    Future<bool> executeOneRequest() {
        Future<int> fa = m_reader.readInt();
        Future<int> fb = addAsyncContinuation<int>(*m_pExecutor,
            [this](int a)->Future<int> {
                if(a > 0) {
                    return m_reader.readInt();
                } else {
                    throw -1;
                }
            }, fa);
        Future<bool> result = addAsyncContinuation<bool>(*m_pExecutor,
            [this,fa](int b) -> Future<bool> {
                if(b > 0) {
                    int sum = fa.get() + b;
                    std::shared_ptr<std::string> pSumStr = std::make_shared<std::string>(std::to_string(sum) + "\n");
                    return m_pSocket->send(pSumStr);
                } else {
                    throw -2;
                }
            }, fb);
        return result;
    }

    Future<bool> run() {
        Future<bool> loopF = executeAsyncLoop<bool>(*m_pExecutor,
            [](bool b){return b;},
            [this](bool b){return executeOneRequest();},
            true);
        Future<bool> finish = addContinuation<bool>(*m_pExecutor, [this](bool)->bool{m_pSocket = nullptr;return false;}, loopF);
        return catchAsync<bool>(*m_pExecutor, [this](std::exception_ptr pEx) -> Future<bool> {
            try {
                m_pSocket = nullptr;
                std::rethrow_exception(pEx);
            } catch(int i) {
                if(i == -1) {
                    std::cout << "Normal ending\n";
                    return completedFuture<bool>(false);
                }
                throw;
            }
        }, loopF);
    }

private:
    Executor* m_pExecutor;
    std::shared_ptr<Socket> m_pSocket; // try to change to unique_ptr
    BufferedReader m_reader;
};

class Server {
public:
    Server()
        :m_executor(1)
        {}
    void run() {
        m_pServerSocket = createTcpServer(5000);
        Future<bool> loopF = executeAsyncLoop(m_executor, [](bool){return true;},
            [this](bool) {
                Future<std::shared_ptr<Socket> > socketF = startProcessOneClient();
                return addContinuation<bool>(m_executor, [](std::shared_ptr<Socket> pS)->bool {return pS != nullptr;}, socketF);
            }, true);
        m_waiter.addToWaitList(loopF);
        m_waiter.waitForAll();
    }

private:
    Future<std::shared_ptr<Socket> > startProcessOneClient() {
        Future<std::shared_ptr<Socket> > socketF = m_pServerSocket->accept();
        Future<std::shared_ptr<ClientHandler> > clientHandlerF = addContinuation<std::shared_ptr<ClientHandler> >(m_executor, 
            [this](std::shared_ptr<Socket> const& pSocket) -> std::shared_ptr<ClientHandler> {
                return std::make_shared<ClientHandler>(&m_executor, pSocket);
        }, socketF);
        Future<bool> finishF = addAsyncContinuation<bool>(m_executor, [this](std::shared_ptr<ClientHandler> clientHandler)->Future<bool>{
            return clientHandler->run();
        }, clientHandlerF);
        Future<bool> clientHolderF = addContinuation<bool>(m_executor, [clientHandlerF](bool val){return val;}, finishF);
        m_waiter.addToWaitList(clientHolderF);
        return socketF;
    }

    FutureWaiter m_waiter;
    ThreadPool m_executor;
    std::unique_ptr<ServerSocket> m_pServerSocket;
};

void demo_server() {
    Server server;
    server.run();
}
