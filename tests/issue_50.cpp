/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

// Must come before any asio header, otherwise build fails on msvc.

#include <boost/redis/connection.hpp>
#include <boost/redis/logger.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/consign.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <tuple>
#include <iostream>
#include "../examples/start.hpp"

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
using steady_timer = net::use_awaitable_t<>::as_default_on_t<net::steady_timer>;
using boost::redis::request;
using boost::redis::response;
using boost::redis::ignore;
using boost::redis::logger;
using boost::redis::config;
using boost::redis::operation;
using boost::system::error_code;
using boost::asio::use_awaitable;
using boost::asio::redirect_error;
using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;
using namespace std::chrono_literals;

// Push consumer
auto
receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   std::cout << "uuu" << std::endl;
   while (conn->will_reconnect()) {
      std::cout << "dddd" << std::endl;
      // Loop reading Redis pushs messages.
      for (;;) {
         std::cout << "aaaa" << std::endl;
         error_code ec;
         co_await conn->async_receive(ignore, redirect_error(use_awaitable, ec));
         if (ec)
            break;
      }
   }
}

auto
periodic_task(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
  net::steady_timer timer{co_await net::this_coro::executor};
  for (int i = 0; i < 10; ++i) {
    std::cout << "In the loop: " << i << std::endl;
    timer.expires_after(std::chrono::milliseconds(50));
    co_await timer.async_wait(net::use_awaitable);

    // Key is not set so it will cause an error since we are passing
    // an adapter that does not accept null, this will cause an error
    // that result in the connection being closed.
    request req;
    req.push("GET", "mykey");
    auto [ec, u] = co_await conn->async_exec(req, ignore, net::as_tuple(net::use_awaitable));
    if (ec) {
      std::cout << "(1)Error: " << ec << std::endl;
    } else {
      std::cout << "no error: " << std::endl;
    }
  }

  std::cout << "Periodic task done!" << std::endl;
  conn->cancel(operation::run);
  conn->cancel(operation::receive);
  conn->cancel(operation::reconnection);
}

auto co_main(config cfg) -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto ctx = std::make_shared<net::ssl::context>(net::ssl::context::tls_client);
   auto conn = std::make_shared<connection>(ex, *ctx);

   net::co_spawn(ex, receiver(conn), net::detached);
   net::co_spawn(ex, periodic_task(conn), net::detached);
   conn->async_run(cfg, {}, net::consign(net::detached, conn, ctx));
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
