#pragma once

#include "config.h"

#include <base/types.h>
#include <memory>
#include <string>


namespace DB
{

class GRPCServer;
class TCPServer;

/// Provides an unified interface to access a protocol implementing server
/// no matter what type it has (HTTPServer, TCPServer, MySQLServer, GRPCServer, ...).
// 提供一个统一的接口来访问实现协议的服务器
// 无论服务器类型是什么（HTTPServer, TCPServer, MySQLServer, GRPCServer, ...）
class ProtocolServerAdapter
{
    friend class ProtocolServers;
public:
    ProtocolServerAdapter(ProtocolServerAdapter && src) = default;
    ProtocolServerAdapter & operator =(ProtocolServerAdapter && src) = default;

    // 构造函数
    ProtocolServerAdapter(
        const std::string & listen_host_,
        const char * port_name_,
        const std::string & description_,
        std::unique_ptr<TCPServer> tcp_server_,
        bool supports_runtime_reconfiguration_ = true);

    // 构造函数
    // 使用 GRPC 服务器
#if USE_GRPC
    ProtocolServerAdapter(
        const std::string & listen_host_,
        const char * port_name_,
        const std::string & description_,
        std::unique_ptr<GRPCServer> grpc_server_,
        bool supports_runtime_reconfiguration_ = true);
#endif

    /// Starts the server. A new thread will be created that waits for and accepts incoming connections.
    // 启动服务器。一个新线程将被创建，等待并接受传入的连接。
    void start() { impl->start(); }

    /// Stops the server. No new connections will be accepted.
    // 停止服务器。不会接受新的连接。
    void stop() { impl->stop(); }

    // 返回是否正在停止
    bool isStopping() const { return impl->isStopping(); }

    /// Returns the number of currently handled connections.
    // 返回当前正在处理的连接数。
    size_t currentConnections() const { return impl->currentConnections(); }

    // 返回拒绝的连接数
    size_t refusedConnections() const { return impl->refusedConnections(); }

    /// Returns the number of current threads.
    size_t currentThreads() const { return impl->currentThreads(); }

    /// Returns the port this server is listening to.
    // 返回服务器正在监听的端口号
    UInt16 portNumber() const { return impl->portNumber(); }

    // 返回是否支持运行时重新配置
    bool supportsRuntimeReconfiguration() const { return supports_runtime_reconfiguration; }

    // 返回监听主机
    const std::string & getListenHost() const { return listen_host; }

    const std::string & getPortName() const { return port_name; }

    const std::string & getDescription() const { return description; }

private:
    class Impl
    {
    public:
        virtual ~Impl() = default;
        virtual void start() = 0;
        virtual void stop() = 0;
        virtual bool isStopping() const = 0;
        virtual UInt16 portNumber() const = 0;
        virtual size_t currentConnections() const = 0;
        virtual size_t currentThreads() const = 0;
        virtual size_t refusedConnections() const = 0;
    };
    class TCPServerAdapterImpl;
    class GRPCServerAdapterImpl;

    std::string listen_host;
    std::string port_name;
    std::string description;
    std::unique_ptr<Impl> impl;
    bool supports_runtime_reconfiguration = true;
};

}
