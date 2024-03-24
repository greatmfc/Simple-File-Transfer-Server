#ifndef IO_HPP
#define IO_HPP
#include <stdexcept>
#include <filesystem>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <arpa/inet.h>
#include "util.hpp"
using std::out_of_range;
using std::runtime_error;
using std::string;
using namespace std::filesystem;
using Byte = char;
namespace mfcslib {
	enum {
		RDONLY,
		WRONLY,
		RDWR
	};
	class basic_io
	{
	protected:
		basic_io() = default;
		~basic_io() {
			::close(_fd);
		}
	public:
		auto read() {
			Byte charc = 0;
			auto ret = ::read(_fd, &charc, 1);
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return charc;
		}
		auto read(Byte* buf,size_t nbytes) {
			auto ret = ::read(_fd, buf, nbytes);
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		auto read(TypeArray<Byte>& buf) {
			auto ret = ::read(_fd, buf.get_ptr(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		auto read(TypeArray<Byte>& buf, size_t pos, size_t sz) {
			auto len = buf.length();
			if (pos >= len || sz > len || pos + sz > len)
				throw out_of_range("In read, pos or sz is out of range.");
			auto ret = ::read(_fd, buf.get_ptr() + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		auto write(TypeArray<Byte>& buf) {
			auto ret = ::write(_fd, buf.get_ptr(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		auto write(TypeArray<Byte>& buf, size_t pos, size_t sz) {
			auto len = buf.length();
			if (pos >= len || sz > len || pos + sz > len)
				throw out_of_range("In write, pos or sz is out of range.");
			auto ret = ::write(_fd, buf.get_ptr() + pos, sz);
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		auto write(const std::string_view& buf) {
			auto ret = ::write(_fd, buf.data(), buf.length());
			if (ret < 0 && errno != EAGAIN) throw IO_exception(strerror(errno));
			return ret;
		}
		void close() {
			if (_fd > 0) {
				::close(_fd);
				_fd = -1;
			}
		}
		auto get_fd() const {
			return _fd;
		}
		auto available() const {
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
			int flag = 0;
			switch (rwmode)
			{
			case 0: flag |= O_RDONLY; break;
			case 1: flag |= O_WRONLY | O_CREAT; break;
			default:flag |= O_RDWR | O_CREAT; break;
			}
			if (trunc) flag |= O_TRUNC;
			else flag |= O_APPEND;
			_fd = ::open(file.c_str(), flag, 0644);
			if (_fd < 0) throw file_exception(strerror(errno));
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
		bool is_existing() const {
			return _fd > 0;
		}
		auto size() const {
			return file_size(_path);
		}
		string size_string() const {
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
		string get_type() {
			auto _s = _path.filename().string();
			if (auto idx = _s.find_last_of('.'); idx != std::string::npos)
				return _s.substr(idx);
			else return {};
		}

	private:
		path _path;
	};

	class NetworkSocket :public basic_io
	{
	public:
		NetworkSocket() = default;
		NetworkSocket(const NetworkSocket&) = delete;
		NetworkSocket(NetworkSocket&& other)noexcept {
			this->_fd = other._fd;
			other._fd = -1;
			this->ip_port = other.ip_port;
			::memset(&other.ip_port, 0, sizeof(other.ip_port));
		}
		NetworkSocket(int fd, sockaddr_in addr_info){
			_fd = fd;
			ip_port = addr_info;
		}
		NetworkSocket(const string& ip, uint16_t port) {
			memset(&ip_port, 0, sizeof ip_port);
			ip_port.sin_family = AF_INET;
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
				throw socket_exception(error_msg);
			}
		}
		in_addr get_ip() {
			return ip_port.sin_addr;
		}
		std::string get_ip_s() {
			return inet_ntoa(ip_port.sin_addr);
		}
		auto get_port() {
			return ntohs(ip_port.sin_port);
		}
		std::string get_port_s() {
			return std::to_string(ntohs(ip_port.sin_port));
		}
		std::string get_ip_port_s() {
			return get_ip_s() + ':' + get_port_s();
		}
		~NetworkSocket() {}

		mfcslib::NetworkSocket& operator=(mfcslib::NetworkSocket&& other) {
			this->_fd = other._fd;
			other._fd = -1;
			this->ip_port = other.ip_port;
			::memset(&other.ip_port, 0, sizeof(other.ip_port));
			return *this;
		}

	protected:
		sockaddr_in ip_port;
	};

	class ServerSocket :public NetworkSocket
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
			memset(&ip_port, 0, sizeof ip_port);
			ip_port.sin_family = AF_INET;
			ip_port.sin_addr.s_addr = htonl(INADDR_ANY);
			ip_port.sin_port = htons(port);
			_fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
			if (_fd <= 0) {
				throw socket_exception(strerror(errno));
			}
			int flag = 1;
			setsockopt(_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
			int ret = ::bind(_fd, (sockaddr*)&ip_port, sizeof(ip_port));
			if (ret < 0) {
				throw socket_exception(strerror(errno));
			}
			auto rett = ::listen(_fd, 5);
			if (rett < 0) {
				throw socket_exception(strerror(errno));
			}
		}
		NetworkSocket accpet() {
			sockaddr_in addrs{};
			socklen_t len = sizeof addrs;
			auto ret = ::accept(_fd, (sockaddr*)&addrs, &len);
			if (ret < 0) {
				if (errno != EAGAIN) throw socket_exception(strerror(errno));
				else return {};
			}
			return NetworkSocket(ret, addrs);
		}
		~ServerSocket() {}
	};

}
#endif // !IO_HPP
