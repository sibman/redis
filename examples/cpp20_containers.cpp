/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/co_spawn.hpp>
#include <map>
#include <vector>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
namespace redis = boost::redis;
using redis::request;
using redis::response;
using redis::ignore_t;
using redis::ignore;
using redis::config;
using connection = net::deferred_t::as_default_on_t<boost::redis::connection>;

void print(std::map<std::string, std::string> const& cont)
{
   for (auto const& e: cont)
      std::cout << e.first << ": " << e.second << "\n";
}

void print(std::vector<int> const& cont)
{
   for (auto const& e: cont) std::cout << e << " ";
   std::cout << "\n";
}

// Stores the content of some STL containers in Redis.
auto store(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   std::vector<int> vec
      {1, 2, 3, 4, 5, 6};

   std::map<std::string, std::string> map
      {{"key1", "value1"}, {"key2", "value2"}, {"key3", "value3"}};

   request req;
   req.push_range("RPUSH", "rpush-key", vec);
   req.push_range("HSET", "hset-key", map);

   co_await conn->async_exec(req);
}

auto hgetall(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   // A request contains multiple commands.
   request req;
   req.push("HGETALL", "hset-key");

   // Responses as tuple elements.
   response<std::map<std::string, std::string>> resp;

   // Executes the request and reads the response.
   co_await conn->async_exec(req, resp);

   print(std::get<0>(resp).value());
}

// Retrieves in a transaction.
auto transaction(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.push("MULTI");
   req.push("LRANGE", "rpush-key", 0, -1); // Retrieves
   req.push("HGETALL", "hset-key"); // Retrieves
   req.push("EXEC");

   response<
      ignore_t, // multi
      ignore_t, // lrange
      ignore_t, // hgetall
      response<std::optional<std::vector<int>>, std::optional<std::map<std::string, std::string>>> // exec
   > resp;

   co_await conn->async_exec(req, resp);

   print(std::get<0>(std::get<3>(resp).value()).value().value());
   print(std::get<1>(std::get<3>(resp).value()).value().value());
}

// Called from the main function (see main.cpp)
net::awaitable<void> co_main(config cfg)
{
   auto ctx = std::make_shared<net::ssl::context>(net::ssl::context::tls_client);
   auto conn = std::make_shared<connection>(co_await net::this_coro::executor, *ctx);
   conn->async_run(cfg, {}, net::consign(net::detached, conn, ctx));

   co_await store(conn);
   co_await transaction(conn);
   co_await hgetall(conn);
   conn->cancel();
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
