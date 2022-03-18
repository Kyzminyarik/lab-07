#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/program_options.hpp>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <suggestion.hpp>
#include <json_storage.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::string make_json(const json& data) {
  std::stringstream ss;
  if (data.is_null())
    ss << "No suggestions";
  else
    ss << std::setw(4) << data;
  return ss.str();
}

template <class Body, class Allocator, class Send>
void handle_request(http::request<Body, http::basic_fields<Allocator>>&& req,
                    Send&& send, const std::shared_ptr<std::timed_mutex>& mutex,
                    const std::shared_ptr<suggestionsColl>& collection)
{
  // Returns a bad request response
  auto const bad_request = [&req](beast::string_view ans) {
    http::response<http::string_body> res{http::status::bad_request,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(ans);
    res.prepare_payload();
    return res;
  };

  auto const not_found = [&req](beast::string_view target) {
    http::response<http::string_body> res{http::status::not_found,
                                          req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.";
    res.prepare_payload();
    return res;
  };

//  auto const server_error =[&req](beast::string_view what) {
//    http::response<http::string_body> res{http::status::internal_server_error,
//                                          req.version()};
//    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//    res.set(http::field::content_type, "text/html");
//    res.keep_alive(req.keep_alive());
//    res.body() = "An error occurred: '" + std::string(what) + "'";
//    res.prepare_payload();
//    return res;
//  };

  if( req.method() != http::verb::post){
    return send(bad_request("Unknown HTTP-method. You should use POST method"));
  }

  if (req.target() != "/v1/api/suggest"){
    return send(not_found(req.target()));
  }

  json input_body;
  try{
    input_body = json::parse(req.body());
  } catch (std::exception& e){
    return send(bad_request(e.what()));
  }

  boost::optional<std::string> input;
  try {
    input = input_body.at("input").get<std::string>();
  } catch (std::exception& e){
    return send(bad_request(R"(JSON format: {"input" : "<user_input">})"));
  }
  if (!input.has_value()){
    return send(bad_request(R"(JSON format: {"input" : "<user_input">})"));
  }
  mutex->lock();
  auto result = collection->suggest(input.value());
  mutex->unlock();
  http::string_body::value_type body = make_json(result);
  auto const size = body.size();

//  if (req.method() == http::verb::head){
//    http::response<http::empty_body> res{http::status::ok, req.version()};
//    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
//    res.set(http::field::content_type, "application/json");
//    res.content_length(size);
//    res.keep_alive(req.keep_alive());
//    return send(std::move(res));
//  }

  http::response<http::string_body> res{
      std::piecewise_construct, std::make_tuple(std::move(body)),
      std::make_tuple(http::status::ok, req.version())};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "application/json");
  res.content_length(size);
  res.keep_alive(req.keep_alive());
  return send(std::move(res));
}

void fail(beast::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}
//////////////////////////////////////////////////////////////////////////////
template <class Stream>
struct send_lambda {
  Stream& stream_;
  bool& close_;
  beast::error_code& ec_;

  explicit send_lambda(Stream& stream, bool& close, beast::error_code& ec)
      : stream_(stream), close_(close), ec_(ec) {}

  template <bool isRequest, class Body, class Fields>
  void operator()(http::message<isRequest, Body, Fields>&& msg) const {
    close_ = msg.need_eof();
    http::serializer<isRequest, Body, Fields> sr{msg};
    http::write(stream_, sr, ec_);
  }
};
//////////////////////////////////////////////////////////////////////////////
void do_session(net::ip::tcp::socket& socket,
                const std::shared_ptr<suggestionsColl>& collection,
                const std::shared_ptr<std::timed_mutex>& mutex) {
  bool close = false;
  beast::error_code ec;
  beast::flat_buffer buffer;
  send_lambda<tcp::socket> lambda{socket, close, ec};

  for (;;) {
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if (ec == http::error::end_of_stream) break;
    if (ec) return fail(ec, "read");

    handle_request(std::move(req), lambda, mutex, collection);
    if (ec) return fail(ec, "write");
    if(close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }
  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);
  // At this point the connection is closed gracefully
}

[[noreturn]]void suggestion_updater(
    const std::shared_ptr<JsonStorage>& storage,
    const std::shared_ptr<suggestionsColl>& suggestions,
    const std::shared_ptr<std::timed_mutex>& mutex) {
  for (;;) {
    mutex->lock();
    storage->load();
    suggestions->update(storage->get_storage());
    mutex->unlock();
    std::cout << "Suggestions updated!" << std::endl;
    std::this_thread::sleep_for(std::chrono::minutes(15));
  }
}

namespace po = boost::program_options;

int main(int argc, char *argv[]) {

  std::shared_ptr<std::timed_mutex> mutex =
      std::make_shared<std::timed_mutex>();
  std::shared_ptr<JsonStorage> storage =
      std::make_shared<JsonStorage>("../suggestions.json");
  std::shared_ptr<suggestionsColl> suggestions =
      std::make_shared<suggestionsColl>();

//  po::options_description desc("Options");
//  desc.add_options()("help", "Show help message")(
//      "address, a", po::value<std::string>(), "Address")(
//      "port, p", po::value<std::string>(), "Port");
//  po::variables_map vm;
//  po::store(po::parse_command_line(argc, argv, desc), vm);
//  po::notify(vm);
//
//  if (vm.count("help")) {
//    std::cout << desc << '\n';
//    return 0;
//  }

//  std::string addr;
//  if (vm.count("address")) {
//    if (vm.at("address").as<std::string>().empty()) {
//      throw std::runtime_error{"empty address"};
//    }
//    addr = vm.at("address").as<std::string>();
//  }
//  auto const address = net::ip::make_address(addr);
//
//  std::string p;
//  if (vm.count("port")) {
//    if (vm.at("port").as<std::string>().empty()) {
//      throw std::runtime_error{"empty port"};
//    }
//    p = vm.at("port").as<std::string>();
//  }
//  auto const port = static_cast<uint16_t>(std::stoi(p));
//
//  net::io_context ctx{1};
//  tcp::acceptor acceptor{ctx, {address, port}};
//  std::thread{suggestion_updater, storage, suggestions, mutex}.detach();

//  for (;;){
//    tcp::socket socket{ctx};
//    acceptor.accept(socket);
//    std::thread{std::bind(&do_session, std::move(socket),
//                          suggestions, mutex)}.detach();
//  }

    try {
      if (argc != 3) {
        std::cerr << "Usage: suggestion_server <address> <port>\n"
                  << "Example:\n"
                  << "    http-server-sync 0.0.0.0 8080\n";
        return EXIT_FAILURE;
      }
      auto const address = net::ip::make_address(argv[1]);
      auto const port = static_cast<uint16_t>(std::atoi(argv[2]));

      net::io_context ioc{1};

      tcp::acceptor acceptor {
          ioc, { address, port }
      };
      std::thread{suggestion_updater, storage, suggestions, mutex}.detach();
      for (;;) {
        tcp::socket socket{ioc};

        acceptor.accept(socket);

        std::thread{std::bind(&do_session, std::move(socket),
                              suggestions, mutex)}
            .detach();
      }
    } catch (std::exception& e) {
      std::cerr << e.what() << '\n';
      return EXIT_FAILURE;
    }
}