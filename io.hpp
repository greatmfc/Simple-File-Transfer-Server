#ifndef IO_HPP
#define IO_HPP
#include <iterator>
#include <stdexcept>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include <array>
using std::out_of_range;
using std::runtime_error;
using std::string;
using namespace std::filesystem;
using Byte = char;
namespace mfcslib {
	template<typename _Type>
	class TypeArray
	{
		using size_type = size_t;
	public:
		TypeArray() = delete;
		TypeArray(const TypeArray& arg) = delete;
		~TypeArray() {
			if (_DATA != nullptr) {
				delete[] _DATA;
				_SIZE = 0;
				_DATA = nullptr;
			}
		};
		constexpr explicit TypeArray(size_t sz) :_SIZE(sz) {
			_DATA = new _Type[sz];
			memset(_DATA, 0, sz);
		}
		constexpr explicit TypeArray(TypeArray&& arg) {
			_DATA = arg._DATA;
			arg._DATA = nullptr;
			_SIZE = arg._SIZE;
			arg._SIZE = 0;
		}
		constexpr _Type& operator[](int arg) {
			if (arg < 0 || arg >= _SIZE) throw std::out_of_range("In [].");
			return _DATA[arg];
		}
		constexpr bool empty() {
			return _DATA == nullptr;
		}
		constexpr void fill(_Type val, size_t start, size_t end) {
			if (start >= end) throw std::out_of_range("In fill, start is greater or equal to end.");
			memset(_DATA + start, val, end - start);
		}
		constexpr void empty_array() {
			fill(0, 0, _SIZE);
		}
		constexpr size_type length() {
			return _SIZE;
		}
		constexpr auto read(int fd) {
			auto ret = ::read(fd, _DATA, _SIZE);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		constexpr auto read(int fd, size_t pos, size_t sz) {
			if (pos >= _SIZE || sz > _SIZE || pos + sz > _SIZE)
				throw out_of_range("In read, pos or sz is out of range.");
			auto ret=::read(fd, _DATA + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		constexpr auto write(int fd) {
			auto ret = ::write(fd, _DATA, _SIZE);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		constexpr auto write(int fd, ssize_t pos, size_t sz) {
			if (pos >= _SIZE || sz > _SIZE || pos + sz > _SIZE) 
				throw out_of_range("In write, pos or sz is out of range.");
			auto ret = ::write(fd, _DATA + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		constexpr void destroy() {
			this->~TypeArray();
		}
		constexpr auto get_ptr() {
			return _DATA;
		}
		constexpr auto to_string() {
			return std::string(_DATA);
		}
		friend constexpr std::basic_ostream<_Type>&
			operator<<(std::basic_ostream<_Type>& os, TypeArray<_Type>& str) {
			os << str._DATA;
			return os;
		}

	private:
		_Type* _DATA = nullptr;
		size_type _SIZE = 0;
	};

	template<typename T>
	auto make_array(size_t sz) {
		return TypeArray<T>(sz);
	}
	
	class basic_io
	{
	protected:
		basic_io() = default;
		~basic_io() {
			if (_fd > 0) {
				::close(_fd);
				_fd = -1;
			}
		}
	public:
		auto read() {
			Byte charc = 0;
			auto ret = ::read(_fd, &charc, 1);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return charc;
		}
		auto read(TypeArray<Byte>& buf) {
			auto ret = ::read(_fd, buf.get_ptr(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		auto read(TypeArray<Byte>& buf, size_t pos, size_t sz) {
			auto len = buf.length();
			if (pos >= len || sz > len || pos + sz > len)
				throw out_of_range("In read, pos or sz is out of range.");
			auto ret = ::read(_fd, buf.get_ptr() + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		auto write(TypeArray<Byte>& buf) {
			auto ret = ::write(_fd, buf.get_ptr(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		auto write(TypeArray<Byte>& buf, size_t pos, size_t sz) {
			auto len = buf.length();
			if (pos >= len || sz > len || pos + sz > len)
				throw out_of_range("In write, pos or sz is out of range.");
			auto ret = ::write(_fd, buf.get_ptr() + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		auto write(const string& buf) {
			auto ret = ::write(_fd, buf.c_str(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw runtime_error(strerror(errno));
			return ret;
		}
		void close() {
			this->~basic_io();
		}
		auto get_fd() {
			return _fd;
		}
		auto available() {
			return _fd != -1;
		}
		auto set_nonblocking() {
			int old_option = fcntl(_fd, F_GETFL);
			int new_option = old_option | O_NONBLOCK;
			fcntl(_fd, F_SETFL, new_option);
			return old_option;
		}

	protected:
		int _fd = -1;
	};

	class File :public basic_io
	{
	public:
		File() = default;
		File(const File&) = delete;
		auto open(const string& file, bool trunc, int rwmode) {
			int flag = O_CREAT;
			switch (rwmode)
			{
			case 0: flag |= O_RDONLY; break;
			case 1: flag |= O_WRONLY; break;
			default:flag |= O_RDWR; break;
			}
			if (trunc) flag |= O_TRUNC;
			else flag |= O_APPEND;
			_fd = ::open(file.c_str(), flag, 0644);
			if (_fd < 0) throw runtime_error(strerror(errno));
			_path = file;
			return _fd;
		}
		File(const string& file, bool trunc,int rwmode) {
			this->open(file, trunc, rwmode);
		}
		File(File&& other) {
			this->_fd = other._fd;
			this->_path = std::move(other._path);
			other._fd = -1;
		}
		~File() {}
		bool is_existing() {
			return _fd > 0;
		}
		auto size() {
			return file_size(_path);
		}
		string size_string() {
			return std::to_string(file_size(_path));
		}
		string get_parent() {
			return _path.parent_path().string();
		}
		string get_absolute() {
			return _path.parent_path().string() + _path.filename().string();
		}
		string filename() {
			return _path.filename().string();
		}

	private:
		path _path;
	};

	class Socket :public basic_io
	{
	public:
		Socket() = default;
		Socket(const Socket&) = delete;
		Socket(Socket&& other)noexcept {
			this->_fd = other._fd;
			other._fd = -1;
			this->ip_port = other.ip_port;
			::memset(&other.ip_port, 0, sizeof(other.ip_port));
		}
		Socket(int fd, sockaddr_in addr_info){
			_fd = fd;
			ip_port = addr_info;
		}
		Socket(const string& ip, uint16_t port) {
			memset(&ip_port, 0, sizeof ip_port);
			ip_port.sin_family = AF_INET;
			//unsigned long target_addr = 0;
			ip_port.sin_addr.s_addr = inet_addr(ip.c_str());
			if (ip_port.sin_addr.s_addr == INADDR_NONE) {
				throw std::invalid_argument("Invalid address:");
			}
			ip_port.sin_port = htons(port);
			_fd = socket(AF_INET, SOCK_STREAM, 0);
			int ret = connect(_fd, (struct sockaddr*)&ip_port, sizeof(ip_port));
			if (ret < 0) {
				string error_msg = "Can not connect: ";
				error_msg += strerror(errno);
				throw std::runtime_error(error_msg);
			}
		}
		~Socket() {
			this->close();
		}

	protected:
		sockaddr_in ip_port;
	};

	class ServerSocket :public Socket
	{
	public:
		ServerSocket() = delete;
		ServerSocket(const ServerSocket&) = delete;
		ServerSocket(ServerSocket&& other)noexcept {
			this->_fd = other._fd;
			other._fd = -1;
			this->ip_port = other.ip_port;
			::memset(&other.ip_port, 0, sizeof(other.ip_port));
		}
		ServerSocket(uint16_t port) {
		#ifdef _WIN32
			WORD sockVersion=MAKEWORD(2,2);
			WSADATA wsaData;
			if (WSAStartup(sockVersion, &wsaData) != 0) {
				throw runtime_error("Startup fail.");
			}
		#endif // _WIN32
			memset(&ip_port, 0, sizeof ip_port);
			ip_port.sin_family = AF_INET;
			ip_port.sin_addr.s_addr = htonl(INADDR_ANY);
			ip_port.sin_port = htons(port);
			_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
			if (_fd <= 0) {
				throw runtime_error(strerror(errno));
			}
			char flag = 1;
			setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
			int ret = ::bind(_fd, (sockaddr*)&ip_port, sizeof(ip_port));
			if (ret < 0) {
				throw runtime_error(strerror(errno));
			}
			auto rett = ::listen(_fd, 5);
			if (rett < 0) {
				throw runtime_error(strerror(errno));
			}
		}
		Socket accpet() {
			sockaddr_in addrs{};
			socklen_t len = sizeof addrs;
			auto ret = ::accept(_fd, (sockaddr*)&addrs, &len);
			if (ret < 0) {
				throw runtime_error(strerror(errno));
			}
			return Socket(ret, addrs);
		}
		~ServerSocket() {}
	};

	constexpr std::array<std::string_view, 11> all_percent = {
		"\r[----------]",
		"\r[*---------]",
		"\r[**--------]",
		"\r[***-------]",
		"\r[****------]",
		"\r[*****-----]",
		"\r[******----]",
		"\r[*******---]",
		"\r[********--]",
		"\r[*********-]",
		"\r[**********]",
	};
	template<typename T, typename R>
	inline bool progress_bar(T num1, R num2) {
		double percent = static_cast<double>(num1) / static_cast<double>(num2);
		if (percent > 1 || percent <= 0) {
			throw std::invalid_argument("Wrong percent");
		}
		uintmax_t index = uintmax_t(percent * 10);
		std::cout << all_percent[index] << ' ' << std::to_string(percent * 100) << '%';
		std::cout.flush();
		return true;
	}
}
#endif // !IO_HPP
