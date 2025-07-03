#pragma once

#include "config.h"

#if USE_GRPC

#include "clickhouse_grpc.grpc.pb.h"
#include <Poco/Net/SocketAddress.h>
#include <base/types.h>
#include <Common/Logger.h>

namespace Poco { class Logger; }

namespace grpc
{
class Server;
class ServerCompletionQueue;
}

namespace DB
{
class IServer;

class GRPCServer
{
public:
    /// 构造函数
    GRPCServer(IServer & iserver_, const Poco::Net::SocketAddress & address_to_listen_);
    ~GRPCServer();

    /// Starts the server. A new thread will be created that waits for and accepts incoming connections.
    /// 启动服务器。将创建一个新线程，等待并接受传入的连接。
    void start();

    /// Stops the server. No new connections will be accepted.
    /// 停止服务器。不接受新的连接。
    void stop();

    /// Returns the port this server is listening to.
    /// 返回此服务器监听的端口。
    UInt16 portNumber() const { return address_to_listen.port(); }

    /// Returns the number of currently handled connections.
    /// 返回当前处理的连接数。
    size_t currentConnections() const;

    /// Returns the number of current threads.
    /// 返回当前线程数。
    size_t currentThreads() const { return currentConnections(); }

private:
    using GRPCService = clickhouse::grpc::ClickHouse::AsyncService;
    class Runner;

    IServer & iserver;
    const Poco::Net::SocketAddress address_to_listen;
    LoggerRawPtr log;
    GRPCService grpc_service;
    std::unique_ptr<grpc::Server> grpc_server;
    std::unique_ptr<grpc::ServerCompletionQueue> queue;
    std::unique_ptr<Runner> runner;
};
}
#endif
