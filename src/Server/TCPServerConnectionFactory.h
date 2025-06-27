#pragma once

#include <Poco/SharedPtr.h>
#include <Server/TCPProtocolStackData.h>

namespace Poco
{
namespace Net
{
    class StreamSocket;
    class TCPServerConnection;
}
}
namespace DB
{
class TCPServer;

// 一个用于创建 TCP 连接的工厂类
class TCPServerConnectionFactory
{
public:
    using Ptr = Poco::SharedPtr<TCPServerConnectionFactory>;

    virtual ~TCPServerConnectionFactory() = default;

    /// Same as Poco::Net::TCPServerConnectionFactory except we can pass the TCPServer
    // 与 Poco::Net::TCPServerConnectionFactory 相同，除了我们可以传递 TCPServer
    virtual Poco::Net::TCPServerConnection * createConnection(const Poco::Net::StreamSocket & socket, TCPServer & tcp_server) = 0;
    virtual Poco::Net::TCPServerConnection * createConnection(const Poco::Net::StreamSocket & socket, TCPServer & tcp_server, TCPProtocolStackData &/* stack_data */)
    {
        return createConnection(socket, tcp_server);
    }
};
}
