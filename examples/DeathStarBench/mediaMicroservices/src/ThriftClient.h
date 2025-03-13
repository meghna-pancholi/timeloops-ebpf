#ifndef SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H
#define SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H

#include <string>
#include <thread>
#include <iostream>
#include <boost/log/trivial.hpp>
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <thrift/stdcxx.h>
#include "logger.h"
#include "GenericClient.h"

namespace media_service
{

  using apache::thrift::TException;
  using apache::thrift::protocol::TBinaryProtocol;
  using apache::thrift::protocol::TProtocol;
  using apache::thrift::transport::TFramedTransport;
  using apache::thrift::transport::TSocket;
  using apache::thrift::transport::TTransport;

  template <class TThriftClient>
  class ThriftClient : public GenericClient
  {
  public:
    ThriftClient(const std::string &addr, int port);

    ThriftClient(const ThriftClient &) = delete;
    ThriftClient &operator=(const ThriftClient &) = delete;
    ThriftClient(ThriftClient<TThriftClient> &&) = default;
    ThriftClient &operator=(ThriftClient &&) = default;

    ~ThriftClient() override;

    TThriftClient *GetClient() const;

    void Connect() override;
    void Disconnect() override;
    void KeepAlive() override;
    void KeepAlive(int timeout_ms) override;
    bool IsConnected() override;

  private:
    TThriftClient *_client;
    std::shared_ptr<TSocket> _socket;
    std::shared_ptr<TTransport> _transport;
    std::shared_ptr<TProtocol> _protocol;

    void handleException(const TException &tx);
  };

  template <class TThriftClient>
  ThriftClient<TThriftClient>::ThriftClient(const std::string &addr, int port)
  {
    _addr = addr;
    _port = port;
    _socket = std::shared_ptr<TSocket>(new TSocket(addr, port));
    _socket->setKeepAlive(true);
    _socket->setRecvTimeout(30000); // Set receive timeout to 10 seconds
    _socket->setSendTimeout(30000); // Set send timeout to 10 seconds
    _transport = std::shared_ptr<TTransport>(new TFramedTransport(_socket));
    _protocol = std::shared_ptr<TProtocol>(new TBinaryProtocol(_transport));
    _client = new TThriftClient(_protocol);
  }

  template <class TThriftClient>
  ThriftClient<TThriftClient>::~ThriftClient()
  {
    Disconnect();
    delete _client;
  }

  template <class TThriftClient>
  TThriftClient *ThriftClient<TThriftClient>::GetClient() const
  {
    return _client;
  }

  template <class TThriftClient>
  bool ThriftClient<TThriftClient>::IsConnected()
  {
    return _transport->isOpen();
  }

  template <class TThriftClient>
  void ThriftClient<TThriftClient>::Connect()
  {
    if (!IsConnected())
    {
      try
      {
        _transport->open();
      }
      catch (TException &tx)
      {
        handleException(tx);
      }
    }
  }

  template <class TThriftClient>
  void ThriftClient<TThriftClient>::Disconnect()
  {
    if (IsConnected())
    {
      try
      {
        _transport->close();
      }
      catch (TException &tx)
      {
        handleException(tx);
      }
    }
  }

  template <class TThriftClient>
  void ThriftClient<TThriftClient>::KeepAlive()
  {
    try
    {
      if (!IsConnected())
      {
        Connect();
      }
      // Optionally, you could send a lightweight request to ensure the connection is alive
      // Example: _client->ping(); assuming there's a ping method in TThriftClient
    }
    catch (const TException &tx)
    {
      handleException(tx);
    }
  }

  template <class TThriftClient>
  void ThriftClient<TThriftClient>::KeepAlive(int timeout_ms)
  {
    auto start = std::chrono::steady_clock::now();
    while (true)
    {
      try
      {
        if (!IsConnected())
        {
          Connect();
        }
        // Optionally, you could send a lightweight request to ensure the connection is alive
        // Example: _client->ping(); assuming there's a ping method in TThriftClient
        break; // Successfully connected
      }
      catch (const TException &tx)
      {
        handleException(tx);
        // Check if timeout has been reached
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        if (elapsed.count() >= timeout_ms)
        {
          LOG(error) << "KeepAlive timeout reached";
          throw tx;
        }
        // Sleep for a short period before retrying
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
    }
  }

  template <class TThriftClient>
  void ThriftClient<TThriftClient>::handleException(const TException &tx)
  {
    LOG(error) << "Exception: " << tx.what();
    Disconnect();
    // Optional: Implement more sophisticated retry or recovery logic here
  }

} // namespace media_service

#endif // SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H

// #ifndef SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H
// #define SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H

// #include <string>
// #include <thread>
// #include <iostream>
// #include <boost/log/trivial.hpp>

// #include <thrift/protocol/TBinaryProtocol.h>
// #include <thrift/transport/TSocket.h>
// #include <thrift/transport/TTransportUtils.h>
// #include <thrift/stdcxx.h>
// #include "logger.h"
// #include "GenericClient.h"

// namespace media_service
// {

//   using apache::thrift::TException;
//   using apache::thrift::protocol::TBinaryProtocol;
//   using apache::thrift::protocol::TProtocol;
//   using apache::thrift::transport::TFramedTransport;
//   using apache::thrift::transport::TSocket;
//   using apache::thrift::transport::TTransport;

//   template <class TThriftClient>
//   class ThriftClient : public GenericClient
//   {
//   public:
//     ThriftClient(const std::string &addr, int port);

//     ThriftClient(const ThriftClient &) = delete;
//     ThriftClient &operator=(const ThriftClient &) = delete;
//     ThriftClient(ThriftClient<TThriftClient> &&) = default;
//     ThriftClient &operator=(ThriftClient &&) = default;

//     ~ThriftClient() override;

//     TThriftClient *GetClient() const;

//     void Connect() override;
//     void Disconnect() override;
//     void KeepAlive() override;
//     void KeepAlive(int timeout_ms) override;
//     bool IsConnected() override;

//   private:
//     TThriftClient *_client;

//     // std::shared_ptr<TTransport> _socket;
//     std::shared_ptr<TSocket> _socket;
//     std::shared_ptr<TTransport> _transport;
//     std::shared_ptr<TProtocol> _protocol;
//   };

//   template <class TThriftClient>
//   ThriftClient<TThriftClient>::ThriftClient(
//       const std::string &addr, int port)
//   {
//     _addr = addr;
//     _port = port;
//     // _socket = std::shared_ptr<TTransport>(new TSocket(addr, port));
//     _socket = std::shared_ptr<TSocket>(new TSocket(addr, port));
//     _socket->setKeepAlive(true);
//     _transport = std::shared_ptr<TTransport>(new TFramedTransport(_socket));
//     _protocol = std::shared_ptr<TProtocol>(new TBinaryProtocol(_transport));
//     _client = new TThriftClient(_protocol);
//   }

//   template <class TThriftClient>
//   ThriftClient<TThriftClient>::~ThriftClient()
//   {
//     Disconnect();
//     delete _client;
//   }

//   template <class TThriftClient>
//   TThriftClient *ThriftClient<TThriftClient>::GetClient() const
//   {
//     return _client;
//   }

//   template <class TThriftClient>
//   bool ThriftClient<TThriftClient>::IsConnected()
//   {
//     return _transport->isOpen();
//   }

//   template <class TThriftClient>
//   void ThriftClient<TThriftClient>::Connect()
//   {
//     if (!IsConnected())
//     {
//       try
//       {
//         _transport->open();
//       }
//       catch (TException &tx)
//       {
//         throw tx;
//       }
//     }
//   }

//   template <class TThriftClient>
//   void ThriftClient<TThriftClient>::Disconnect()
//   {
//     if (IsConnected())
//     {
//       try
//       {
//         _transport->close();
//       }
//       catch (TException &tx)
//       {
//         throw tx;
//       }
//     }
//   }

//   template <class TThriftClient>
//   void ThriftClient<TThriftClient>::KeepAlive()
//   {
//   }

//   // TODO: Implement KeepAlive Timeout
//   template <class TThriftClient>
//   void ThriftClient<TThriftClient>::KeepAlive(int timeout_ms)
//   {
//   }

// } // namespace media_service

// #endif // SOCIAL_NETWORK_MICROSERVICES_THRIFTCLIENT_H
