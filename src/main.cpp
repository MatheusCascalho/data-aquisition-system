#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#include <ctime>
#include <iomanip>
#include <sstream>

#include <fstream>

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

void saveRecord(std::string fileName, LogRecord rec){
  // Abre o arquivo para leitura e escrita em modo binário e coloca o apontador do arquivo
	// apontando para o fim de arquivo
	std::fstream file(fileName, std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 
	// Caso não ocorram erros na abertura do arquivo
	if (file.is_open())
	{
		// Imprime a posição atual do apontador do arquivo (representa o tamanho do arquivo)
		int file_size = file.tellg();

		// Recupera o número de registros presentes no arquivo
		int n = file_size/sizeof(LogRecord);
		std::cout << "Num records: " << n << " (file size: " << file_size << " bytes)" << std::endl;

		// Escreve registros no arquivo
		file.write((char*)&rec, sizeof(LogRecord));
		
		// Fecha o arquivo
		file.close();
	}
	else
	{
		std::cout << "Error opening file!" << std::endl;
	}
}

std::string concatenateStrings(const std::vector<std::string>& strings) {
    std::string result;

    if (!strings.empty()) {
        result = strings[0];

        for (std::size_t i = 1; i < strings.size(); i++) {
            result += ";" + strings[i];
        }
    }

    return result;
}

std::string readRecord(std::string fileName, int numRegisters){
  // Abre o arquivo para leitura e escrita em modo binário e coloca o apontador do arquivo
	// apontando para o fim de arquivo
	std::fstream file(fileName, std::fstream::out | std::fstream::in | std::fstream::binary 
																	 | std::fstream::app); 
	// Caso não ocorram erros na abertura do arquivo
	if (file.is_open())
	{
		// Imprime a posição atual do apontador do arquivo (representa o tamanho do arquivo)
		int file_size = file.tellg();

		// Recupera o número de registros presentes no arquivo
		int n = file_size/sizeof(LogRecord);
		std::cout << "Num records: " << n << " (file size: " << file_size << " bytes)" << std::endl;

    // std::vector<LogRecord> records;
    std::string convertedData = std::to_string(numRegisters);
      
    // Ler e armazenar os 5 primeiros registros
    for (int i = 0; i < numRegisters; i++) {
      // Le o registro selecionado
      LogRecord rec;
      // file.read(reinterpret_cast<char*>(&rec), sizeof(LogRecord));
      file.read((char*)&rec, sizeof(LogRecord));
      convertedData += ";" + time_t_to_string(rec.timestamp) + "|" + std::to_string(rec.value);
      // records.push_back(rec);
    }
    convertedData += "\r\n";

		// Fecha o arquivo
		file.close();

    return convertedData;
	}
	else
	{
		std::cout << "Error opening file!" << std::endl;
	}
  return fileName;
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
            std::string replyMessage;
            
            std::cout << "Received: " << message << std::endl;

            std::vector<std::string> dataMessage = splitString(message, delimiter);
            std::cout << "Essa é uma mensagem do tipo " << dataMessage[0] << std::endl;
            if (dataMessage[0] == LOG){
              LogRecord dataLog;
              strcpy(dataLog.sensor_id, dataMessage[1].c_str());
              dataLog.timestamp = string_to_time_t(dataMessage[2]);
              dataLog.value = std::stod(dataMessage[3]);
              std::cout << "e o seu valor é  " << dataLog.value << std::endl;
              
              std::string filename = dataMessage[1] + ".dat";
              saveRecord(filename, dataLog);
            }
            else if(dataMessage[0] == GET){
              int numRegisters = std::stoi(dataMessage[2]);
              std::string filename = dataMessage[1] + ".dat";
              replyMessage = readRecord(filename, numRegisters);
            }
            write_message(replyMessage);
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
