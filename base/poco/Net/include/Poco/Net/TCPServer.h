//
// TCPServer.h
//
// Library: Net
// Package: TCPServer
// Module:  TCPServer
//
// Definition of the TCPServer class.
//
// Copyright (c) 2005-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef Net_TCPServer_INCLUDED
#define Net_TCPServer_INCLUDED


#include "Poco/AutoPtr.h"
#include "Poco/Net/Net.h"
#include "Poco/Net/ServerSocket.h"
#include "Poco/Net/TCPServerConnectionFactory.h"
#include "Poco/Net/TCPServerParams.h"
#include "Poco/RefCountedObject.h"
#include "Poco/Runnable.h"
#include "Poco/Thread.h"
#include "Poco/ThreadPool.h"

#include <atomic>


namespace Poco
{
namespace Net
{


    class TCPServerDispatcher;
    class StreamSocket;


    class Net_API TCPServerConnectionFilter : public Poco::RefCountedObject
    /// A TCPServerConnectionFilter can be used to reject incoming connections
    /// before passing them on to the TCPServerDispatcher and
    /// starting a thread to handle them.
    ///
    /// An example use case is white-list or black-list IP address filtering.
    ///
    /// Subclasses must override the accept() method.
    // 一个 TCPServerConnectionFilter 可以用于拒绝传入的连接
    // 在传递给 TCPServerDispatcher 之前
    // 并启动一个线程来处理它们。
    // 一个例子是白名单或黑名单 IP 地址过滤。
    // 子类必须重写 accept() 方法。
    {
    public:
        // 自动指针
        typedef Poco::AutoPtr<TCPServerConnectionFilter> Ptr;

        // 接受传入的连接
        virtual bool accept(const StreamSocket & socket) = 0;
        /// Returns true if the given StreamSocket connection should
        /// be handled, and passed on to the TCPServerDispatcher.
        // 如果返回 true，则可以处理传入的连接
        // 并将其传递给 TCPServerDispatcher。

        ///
        /// Returns false if the socket should be closed immediately.
        // 如果套接字应该被关闭，则返回 false。
        ///
        /// The socket can be prevented from being closed immediately
        /// if false is returned by creating a copy of the socket.
        // 如果返回 false，则可以创建套接字的副本。
        /// This can be used to handle certain socket connections in
        /// a special way, outside the TCPServer framework.
        // 这可以用于在 TCPServer 框架之外以特殊方式处理某些套接字连接。

    protected:
        virtual ~TCPServerConnectionFilter();
    };


    class Net_API TCPServer : public Poco::Runnable
    /// This class implements a multithreaded TCP server.
    // 这个类实现了一个多线程的 TCP 服务器。
    ///
    /// The server uses a ServerSocket to listen for incoming
    /// connections. The ServerSocket must have been bound to
    /// an address before it is passed to the TCPServer constructor.
    // 服务器使用 ServerSocket 来监听传入的连接。
    // ServerSocket 必须在传递给 TCPServer 构造函数之前被绑定到一个地址。
    /// Additionally, the ServerSocket must be put into listening
    /// state before the TCPServer is started by calling the start()
    /// method.
    // 在调用 start() 方法之前，ServerSocket 必须被置于监听状态。
    ///
    /// The server uses a thread pool to assign threads to incoming
    /// connections. Before incoming connections are assigned to
    /// a connection thread, they are put into a queue.
    // 服务器使用线程池来分配线程给传入的连接。
    // 在传入的连接被分配给一个连接线程之前，它们会被放入一个队列。
    /// Connection threads fetch new connections from the queue as soon
    /// as they become free. Thus, a connection thread may serve more
    /// than one connection.
    // 连接线程在它们变得可用时从队列中获取新的连接。
    // 因此，一个连接线程可能会服务多个连接。
    ///
    /// As soon as a connection thread fetches the next connection from
    /// the queue, it creates a TCPServerConnection object for it
    /// (using the TCPServerConnectionFactory passed to the constructor)
    /// and calls the TCPServerConnection's start() method. When the
    /// start() method returns, the connection object is deleted.
    // 一旦一个连接线程从队列中获取下一个连接，它就会创建一个 TCPServerConnection 对象
    // （使用传递给构造函数的 TCPServerConnectionFactory）
    // 并调用 TCPServerConnection 的 start() 方法。
    // 当 start() 方法返回时，连接对象被删除。
    ///
    /// The number of connection threads is adjusted dynamically, depending
    /// on the number of connections waiting to be served.
    // 连接线程的数量是动态调整的，取决于等待服务的连接数量。
    ///
    /// It is possible to specify a maximum number of queued connections.
    // 可以指定一个最大数量的队列连接。
    /// This prevents the connection queue from overflowing in the
    /// case of an extreme server load. In such a case, connections that
    /// cannot be queued are silently and immediately closed.
    // 在极端服务器负载的情况下，连接队列可能会溢出。
    // 在这种情况下，无法入队的连接会被静默且立即关闭。
    ///
    /// TCPServer uses a separate thread to accept incoming connections.
    /// Thus, the call to start() returns immediately, and the server
    /// continues to run in the background.
    // TCPServer 使用一个单独的线程来接受传入的连接。
    // 因此，调用 start() 方法会立即返回，服务器会在后台继续运行。
    ///
    /// To stop the server from accepting new connections, call stop().
    // 要停止服务器接受新的连接，请调用 stop()。
    ///
    /// After calling stop(), no new connections will be accepted and
    /// all queued connections will be discarded.
    // 在调用 stop() 之后，不会接受新的连接，并且所有队列中的连接将被丢弃。
    /// Already served connections, however, will continue being served.
    // 然而，已经服务的连接将继续被服务。
    {
    public:
        // 创建 TCPServer，使用给定的 ServerSocket。
        TCPServer(TCPServerConnectionFactory::Ptr pFactory, Poco::UInt16 portNumber = 0, TCPServerParams::Ptr pParams = 0);

        /// Creates the TCPServer, with ServerSocket listening on the given port.
        /// Default port is zero, allowing any available port. The port number
        /// can be queried through TCPServer::port() member.
        // 创建 TCPServer，使用 ServerSocket 监听给定的端口。
        // 默认端口为零，允许任何可用的端口。
        // 可以通过 TCPServer::port() 成员查询端口号。
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        // 服务器拥有 TCPServerConnectionFactory 的所有权，
        // 并在不再需要时删除它。
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        // 服务器拥有 TCPServerParams 对象的所有权，
        // 如果未提供 TCPServerParams 对象，服务器会创建自己的一个。
        ///
        /// New threads are taken from the default thread pool.
        // 新线程从默认线程池中获取。

        TCPServer(TCPServerConnectionFactory::Ptr pFactory, const ServerSocket & socket, TCPServerParams::Ptr pParams = 0);
        /// Creates the TCPServer, using the given ServerSocket.
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        ///
        /// New threads are taken from the default thread pool.

        TCPServer(
            TCPServerConnectionFactory::Ptr pFactory,
            Poco::ThreadPool & threadPool,
            const ServerSocket & socket,
            TCPServerParams::Ptr pParams = 0);
        /// Creates the TCPServer, using the given ServerSocket.
        ///
        /// The server takes ownership of the TCPServerConnectionFactory
        /// and deletes it when it's no longer needed.
        ///
        /// The server also takes ownership of the TCPServerParams object.
        /// If no TCPServerParams object is given, the server's TCPServerDispatcher
        /// creates its own one.
        ///
        /// New threads are taken from the given thread pool.

        virtual ~TCPServer();
        /// Destroys the TCPServer and its TCPServerConnectionFactory.

        const TCPServerParams & params() const;
        /// Returns a const reference to the TCPServerParam object
        /// used by the server's TCPServerDispatcher.

        void start();
        /// Starts the server. A new thread will be
        /// created that waits for and accepts incoming
        /// connections.
        ///
        /// Before start() is called, the ServerSocket passed to
        /// TCPServer must have been bound and put into listening state.

        void stop();
        /// Stops the server.
        ///
        /// No new connections will be accepted.
        /// Already handled connections will continue to be served.
        ///
        /// Once the server has been stopped, it cannot be restarted.

        int currentThreads() const;
        /// Returns the number of currently used connection threads.

        int maxThreads() const;
        /// Returns the maximum number of threads available.

        int totalConnections() const;
        /// Returns the total number of handled connections.

        int currentConnections() const;
        /// Returns the number of currently handled connections.

        int maxConcurrentConnections() const;
        /// Returns the maximum number of concurrently handled connections.

        int queuedConnections() const;
        /// Returns the number of queued connections.

        int refusedConnections() const;
        /// Returns the number of refused connections.

        const ServerSocket & socket() const;
        /// Returns the underlying server socket.

        Poco::UInt16 port() const;
        /// Returns the port the server socket listens on.

        void setConnectionFilter(const TCPServerConnectionFilter::Ptr & pFilter);
        /// Sets a TCPServerConnectionFilter. Can also be used to remove
        /// a filter by passing a null pointer.
        ///
        /// To avoid a potential race condition, the filter must
        /// be set before the TCPServer is started. Trying to set
        /// the filter after start() has been called will trigger
        /// an assertion.

        TCPServerConnectionFilter::Ptr getConnectionFilter() const;
        /// Returns the TCPServerConnectionFilter set with setConnectionFilter(),
        /// or null pointer if no filter has been set.

    protected:
        void run();
        /// Runs the server. The server will run until
        /// the stop() method is called, or the server
        /// object is destroyed, which implicitly calls
        /// the stop() method.

        static std::string threadName(const ServerSocket & socket);
        /// Returns a thread name for the server thread.

    private:
        TCPServer();
        TCPServer(const TCPServer &);
        TCPServer & operator=(const TCPServer &);

        ServerSocket _socket;
        TCPServerDispatcher * _pDispatcher;
        TCPServerConnectionFilter::Ptr _pConnectionFilter;
        Poco::Thread _thread;
        std::atomic<bool> _stopped;
    };


    //
    // inlines
    //
    inline const ServerSocket & TCPServer::socket() const
    {
        return _socket;
    }


    inline Poco::UInt16 TCPServer::port() const
    {
        return _socket.address().port();
    }


    inline TCPServerConnectionFilter::Ptr TCPServer::getConnectionFilter() const
    {
        return _pConnectionFilter;
    }


}
} // namespace Poco::Net


#endif // Net_TCPServer_INCLUDED
