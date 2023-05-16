#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <ctime>
#include <iomanip>
#include <sstream>

using boost::asio::ip::tcp;

// Identificador de mensagens
const char* LOG = "LOG";
const char* GET = "GET";
const char* ERROR = "ERROR";
const char delimiter = '|';

#pragma pack(push, 1)
struct LogRecord {
    char sensor_id[32]; // supondo um ID de sensor de até 32 caracteres
    std::time_t timestamp; // timestamp UNIX
    double value; // valor da leitura
};
#pragma pack(pop)


// função que irá quebrar a mensagens nos dados necessários para construir o log
std::vector<std::string> splitString(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss, item, delimiter)) {
        result.push_back(item);
    }

    return result;
}

// converte time_t para string
std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// converte string para time_t
std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

class session
  : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket)
    : socket_(std::move(socket))
  {
  }

  void start()
  {
    read_message();
  }

private:
  void read_message()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            std::cout << "Received: " << message << std::endl;

            std::vector<std::string> dataMessage = splitString(message, delimiter);
            std::cout << "Essa é uma mensagem do tipo " << dataMessage[0] << std::endl;
            if (dataMessage[0] == LOG){
              LogRecord dataLog;
              strcpy(dataLog.sensor_id, dataMessage[1].c_str());
              dataLog.timestamp = string_to_time_t(dataMessage[2]);
              dataLog.value = std::stod(dataMessage[3]);
              std::cout << "e o seu valor é  " << dataLog.value << std::endl;
            }
            write_message(message);
          }
        });
  }

  void write_message(const std::string& message)
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec, std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
  }

  tcp::socket socket_;
  boost::asio::streambuf buffer_;
};

class server
{
public:
  server(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
  {
    accept();
  }

private:
  void accept()
  {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
//   if (argc != 2)
//   {
//     std::cerr << "Usage: chat_server <port>\n";
//     return 1;
//   }

  boost::asio::io_context io_context;

//   server s(io_context, std::atoi(argv[1]));
  const char* port = "9000";
  server s(io_context, std::atoi(port));

  io_context.run();

  return 0;
}
