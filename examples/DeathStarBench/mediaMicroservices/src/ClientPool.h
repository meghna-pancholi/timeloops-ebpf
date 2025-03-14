#ifndef MEDIA_MICROSERVICES_CLIENTPOOL_H
#define MEDIA_MICROSERVICES_CLIENTPOOL_H

#include <vector>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <string>
#include <thread> // Include this header for std::this_thread

#include "logger.h"

namespace media_service
{

  template <class TClient>
  class ClientPool
  {
  public:
    ClientPool(const std::string &client_type, const std::string &addr,
               int port, int min_size, int max_size, int timeout_ms);
    ~ClientPool();

    ClientPool(const ClientPool &) = delete;
    ClientPool &operator=(const ClientPool &) = delete;
    ClientPool(ClientPool &&) = default;
    ClientPool &operator=(ClientPool &&) = default;

    TClient *Pop();
    void Push(TClient *);
    void Push(TClient *, int);
    void Remove(TClient *);
    void Keepalive(TClient *);

  private:
    std::deque<TClient *> _pool;
    std::string _addr;
    std::string _client_type;
    int _port;
    int _min_pool_size{};
    int _max_pool_size{};
    int _curr_pool_size{};
    int _timeout_ms;
    std::mutex _mtx;
    std::condition_variable _cv;
  };

  template <class TClient>
  ClientPool<TClient>::ClientPool(const std::string &client_type,
                                  const std::string &addr, int port, int min_pool_size,
                                  int max_pool_size, int timeout_ms)
  {
    _addr = addr;
    _port = port;
    _min_pool_size = min_pool_size;
    _max_pool_size = max_pool_size;
    _timeout_ms = timeout_ms;
    _client_type = client_type;

    for (int i = 0; i < min_pool_size; ++i)
    {
      TClient *client = new TClient(addr, port);
      _pool.emplace_back(client);
    }
    _curr_pool_size = min_pool_size;
  }

  template <class TClient>
  ClientPool<TClient>::~ClientPool()
  {
    while (!_pool.empty())
    {
      delete _pool.front();
      _pool.pop_front();
    }
  }

  template <class TClient>
  TClient *ClientPool<TClient>::Pop()
  {
    TClient *client = nullptr;
    std::unique_lock<std::mutex> cv_lock(_mtx);
    {
      while (_pool.size() == 0)
      {
        // Create a new client if current pool size is less than the max pool size.
        if (_curr_pool_size < _max_pool_size)
        {
          try
          {
            client = new TClient(_addr, _port);
            _curr_pool_size++;
            break;
          }
          catch (...)
          {
            cv_lock.unlock();
            return nullptr;
          }
        }
        else
        {
          auto wait_time = std::chrono::system_clock::now() +
                           std::chrono::milliseconds(_timeout_ms);
          bool wait_success = _cv.wait_until(cv_lock, wait_time,
                                             [this]
                                             { return _pool.size() > 0; });
          if (!wait_success)
          {
            LOG(warning) << "ClientPool pop timeout";
            cv_lock.unlock();
            return nullptr;
          }
        }
      }
      if (!client)
      {
        client = _pool.front();
        _pool.pop_front();
      }

    } // cv_lock(_mtx)
    cv_lock.unlock();

    if (client)
    {
      int retries = 3; // Number of retries
      while (retries > 0)
      {
        try
        {
          client->Connect();
          break;
        }
        catch (const apache::thrift::transport::TTransportException &e)
        {
          LOG(error) << "Failed to connect " + _client_type + ". Retries left: " << retries;
          std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
          retries--;
          if (retries == 0)
          {
            LOG(error) << "Max retries reached for " + _client_type;
            // Remove the problematic client and try to create a new one
            Remove(client);
            client = new TClient(_addr, _port);
            try
            {
              client->Connect();
              break;
            }
            catch (...)
            {
              delete client;
              cv_lock.lock();
              _curr_pool_size--;
              cv_lock.unlock();
              return nullptr;
            }
          }
        }
      }
    }
    return client;
  }

  template <class TClient>
  void ClientPool<TClient>::Push(TClient *client)
  {
    std::unique_lock<std::mutex> cv_lock(_mtx);
    client->KeepAlive();
    _pool.push_back(client);
    cv_lock.unlock();
    _cv.notify_one();
  }

  template <class TClient>
  void ClientPool<TClient>::Push(TClient *client, int timeout_ms)
  {
    std::unique_lock<std::mutex> cv_lock(_mtx);
    client->KeepAlive(timeout_ms);
    _pool.push_back(client);
    cv_lock.unlock();
    _cv.notify_one();
  }

  template <class TClient>
  void ClientPool<TClient>::Remove(TClient *client)
  {
    std::unique_lock<std::mutex> lock(_mtx);
    delete client;
    _curr_pool_size--;
    lock.unlock();
  }

  template <class TClient>
  void ClientPool<TClient>::Keepalive(TClient *client)
  {
    long curr_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now().time_since_epoch())
                              .count();
    if (curr_timestamp - client->_connect_timestamp > client->_keepalive_ms)
    {
      Remove(client);
    }
    else
    {
      Push(client);
    }
  }

} // namespace media_service

#endif // MEDIA_MICROSERVICES_CLIENTPOOL_H

// #ifndef MEDIA_MICROSERVICES_CLIENTPOOL_H
// #define MEDIA_MICROSERVICES_CLIENTPOOL_H

// #include <vector>
// #include <mutex>
// #include <condition_variable>
// #include <deque>
// #include <chrono>
// #include <string>
// #include <thread>

// #include "logger.h"

// namespace media_service
// {

//   template <class TClient>
//   class ClientPool
//   {
//   public:
//     ClientPool(const std::string &client_type, const std::string &addr,
//                int port, int min_size, int max_size, int timeout_ms);
//     ~ClientPool();

//     ClientPool(const ClientPool &) = delete;
//     ClientPool &operator=(const ClientPool &) = delete;
//     ClientPool(ClientPool &&) = default;
//     ClientPool &operator=(ClientPool &&) = default;

//     TClient *Pop();
//     void Push(TClient *);
//     void Push(TClient *, int);
//     void Remove(TClient *);
//     void Keepalive(TClient *);

//   private:
//     std::deque<TClient *> _pool;
//     std::string _addr;
//     std::string _client_type;
//     int _port;
//     int _min_pool_size{};
//     int _max_pool_size{};
//     int _curr_pool_size{};
//     int _timeout_ms;
//     std::mutex _mtx;
//     std::condition_variable _cv;
//   };

//   template <class TClient>
//   ClientPool<TClient>::ClientPool(const std::string &client_type,
//                                   const std::string &addr, int port, int min_pool_size,
//                                   int max_pool_size, int timeout_ms)
//   {
//     _addr = addr;
//     _port = port;
//     _min_pool_size = min_pool_size;
//     _max_pool_size = max_pool_size;
//     _timeout_ms = timeout_ms;
//     _client_type = client_type;

//     for (int i = 0; i < min_pool_size; ++i)
//     {
//       TClient *client = new TClient(addr, port);
//       _pool.emplace_back(client);
//     }
//     _curr_pool_size = min_pool_size;
//   }

//   template <class TClient>
//   ClientPool<TClient>::~ClientPool()
//   {
//     while (!_pool.empty())
//     {
//       delete _pool.front();
//       _pool.pop_front();
//     }
//   }

//   template <class TClient>
//   TClient *ClientPool<TClient>::Pop()
//   {
//     TClient *client = nullptr;
//     std::unique_lock<std::mutex> cv_lock(_mtx);
//     {
//       while (_pool.size() == 0)
//       {
//         // Create a new client if current pool size is less than the max pool size.
//         if (_curr_pool_size < _max_pool_size)
//         {
//           try
//           {
//             client = new TClient(_addr, _port);
//             _curr_pool_size++;
//             break;
//           }
//           catch (...)
//           {
//             cv_lock.unlock();
//             return nullptr;
//           }
//         }
//         else
//         {
//           auto wait_time = std::chrono::system_clock::now() +
//                            std::chrono::milliseconds(_timeout_ms);
//           bool wait_success = _cv.wait_until(cv_lock, wait_time,
//                                              [this]
//                                              { return _pool.size() > 0; });
//           if (!wait_success)
//           {
//             LOG(warning) << "ClientPool pop timeout";
//             cv_lock.unlock();
//             return nullptr;
//           }
//         }
//       }
//       if (!client)
//       {
//         client = _pool.front();
//         _pool.pop_front();
//       }
//     }
//     cv_lock.unlock();

//     if (client)
//     {
//       int retries = 3; // Number of retries
//       while (retries > 0)
//       {
//         try
//         {
//           client->Connect();
//           break;
//         }
//         catch (const apache::thrift::transport::TTransportException &e)
//         {
//           LOG(error) << "Failed to connect " + _client_type + ". Retries left: " << retries;
//           std::this_thread::sleep_for(std::chrono::seconds(1)); // Wait before retrying
//           retries--;
//           if (retries == 0)
//           {
//             LOG(error) << "Max retries reached for " + _client_type;
//             // Remove the problematic client and try to create a new one
//             Remove(client);
//             client = new TClient(_addr, _port);
//             try
//             {
//               client->Connect();
//               break;
//             }
//             catch (...)
//             {
//               delete client;
//               cv_lock.lock();
//               _curr_pool_size--;
//               cv_lock.unlock();
//               return nullptr;
//             }
//           }
//         }
//       }
//     }
//     return client;
//   }

//   // template <class TClient>
//   // TClient *ClientPool<TClient>::Pop()
//   // {
//   //   TClient *client = nullptr;
//   //   std::unique_lock<std::mutex> cv_lock(_mtx);
//   //   {
//   //     while (_pool.size() == 0)
//   //     {
//   //       // Create a new a client if current pool size is less than
//   //       // the max pool size.
//   //       if (_curr_pool_size < _max_pool_size)
//   //       {
//   //         try
//   //         {
//   //           client = new TClient(_addr, _port);
//   //           _curr_pool_size++;
//   //           break;
//   //         }
//   //         catch (...)
//   //         {
//   //           cv_lock.unlock();
//   //           return nullptr;
//   //         }
//   //       }
//   //       else
//   //       {
//   //         auto wait_time = std::chrono::system_clock::now() +
//   //                          std::chrono::milliseconds(_timeout_ms);
//   //         bool wait_success = _cv.wait_until(cv_lock, wait_time,
//   //                                            [this]
//   //                                            { return _pool.size() > 0; });
//   //         if (!wait_success)
//   //         {
//   //           LOG(warning) << "ClientPool pop timeout";
//   //           cv_lock.unlock();
//   //           return nullptr;
//   //         }
//   //       }
//   //     }
//   //     if (!client)
//   //     {
//   //       client = _pool.front();
//   //       _pool.pop_front();
//   //     }

//   //   } // cv_lock(_mtx)
//   //   cv_lock.unlock();

//   //   if (client)
//   //   {
//   //     try
//   //     {
//   //       client->Connect();
//   //     }
//   //     catch (...)
//   //     {
//   //       LOG(error) << "Failed to connect " + _client_type;
//   //       _pool.push_back(client);
//   //       throw;
//   //     }
//   //   }
//   //   return client;
//   // }

//   template <class TClient>
//   void ClientPool<TClient>::Push(TClient *client)
//   {
//     std::unique_lock<std::mutex> cv_lock(_mtx);
//     client->KeepAlive();
//     _pool.push_back(client);
//     cv_lock.unlock();
//     _cv.notify_one();
//   }

//   template <class TClient>
//   void ClientPool<TClient>::Push(TClient *client, int timeout_ms)
//   {
//     std::unique_lock<std::mutex> cv_lock(_mtx);
//     client->KeepAlive(timeout_ms);
//     _pool.push_back(client);
//     cv_lock.unlock();
//     _cv.notify_one();
//   }

//   template <class TClient>
//   void ClientPool<TClient>::Remove(TClient *client)
//   {
//     std::unique_lock<std::mutex> lock(_mtx);
//     delete client;
//     _curr_pool_size--;
//     lock.unlock();
//   }

//   template <class TClient>
//   void ClientPool<TClient>::Keepalive(TClient *client)
//   {
//     long curr_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
//                               std::chrono::system_clock::now().time_since_epoch())
//                               .count();
//     if (curr_timestamp - client->_connect_timestamp > client->_keepalive_ms)
//     {
//       Remove(client);
//     }
//     else
//     {
//       Push(client);
//     }
//   }

// } // namespace media_service

// #endif // MEDIA_MICROSERVICES_CLIENTPOOL_H