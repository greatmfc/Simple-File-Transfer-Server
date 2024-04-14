#ifndef IO_HPP
#define IO_HPP
#include <stdexcept>
#ifndef __unix__
#include <filesystem>
#endif // !__unix__
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
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
		auto read(TypeArray<Byte>& buf) const {
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
			::close(_fd);
			_fd = -1;
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
#ifdef __unix__
		File(const string& path) : _file_path(path) {}
		File(File&& other) {
			this->_fd = other._fd;
			this->_file_stat = other._file_stat;
			this->_file_path = other._file_path;
			memset(&other._file_stat, 0, sizeof(struct stat));
			other._file_path.clear();
			other._fd = -1;
		}
		~File() {}

		void operator=(const string& path) {
			_file_path = path;
		}
		auto open(const string& path, bool trunc, int rwmode) {
			int flag = 0;
			switch (rwmode)
			{
			case 0: flag |= O_RDONLY; break;
			case 1: flag |= O_WRONLY | O_CREAT; break;
			default:flag |= O_RDWR | O_CREAT; break;
			}
			if (trunc) flag |= O_TRUNC;
			else flag |= O_APPEND;
			_fd = ::open(path.c_str(), flag, 0644);
			if (_fd < 0) throw file_exception(strerror(errno));
			fstat(_fd, &_file_stat);
			if (!S_ISREG(_file_stat.st_mode)) {
				::close(_fd);
				throw std::invalid_argument(std::format("'{}' is not a regular file!\n", path));
			}
			_file_path = _get_path_from_fd(_fd);
			return _fd;
		}
		auto open(bool trunc, int rwmode) {
			int flag = 0;
			switch (rwmode)
			{
			case 0: flag |= O_RDONLY; break;
			case 1: flag |= O_WRONLY | O_CREAT; break;
			default:flag |= O_RDWR | O_CREAT; break;
			}
			if (trunc) flag |= O_TRUNC;
			else flag |= O_APPEND;
			_fd = ::open(_file_path.c_str(), flag, 0644);
			if (_fd < 0) throw file_exception(strerror(errno));
			fstat(_fd, &_file_stat);
			if (!S_ISREG(_file_stat.st_mode)) {
				::close(_fd);
				throw std::invalid_argument(std::format("'{}' is not a regular file!\n", _file_path));
			}
			_file_path = _get_path_from_fd(_fd);
			return _fd;
		}
		auto open_read_only() {
			_fd = ::open(_file_path.c_str(), O_RDONLY);
			if (_fd < 0) throw file_exception(strerror(errno));
			fstat(_fd, &_file_stat);
			if (!S_ISREG(_file_stat.st_mode)) {
				::close(_fd);
				throw std::invalid_argument(std::format("'{}' is not a regular file!\n", _file_path));
			}
			_file_path = _get_path_from_fd(_fd);
			return _fd;
		}
		bool is_existing() const {
			return _fd > 0;
		}
		unsigned long size() const {
			fstat(_fd, (struct stat*)&_file_stat);
			return _file_stat.st_size;
		}
		string size_string() const {
			fstat(_fd, (struct stat*)&_file_stat);
			return std::to_string(_file_stat.st_size);
		}
		string get_parent() {
			return _file_path.substr(0, _file_path.find_last_of('/'));
		}
		string get_absolute() {
			return _file_path;
		}
		string filename() {
			return _file_path.substr(_file_path.find_last_of('/') + 1);
		}
		string get_type() {
			if (auto idx = _file_path.find_last_of('.'); idx != std::string::npos)
				return _file_path.substr(idx);
			else return {};
		}
		auto get_last_modified_time() const {
			fstat(_fd, (struct stat*)&_file_stat);
			return _file_stat.st_mtim;
		}

	private:
		struct stat _file_stat;
		string _file_path;

		string _get_path_from_fd(int fd) {
			char path[256]{ 0 };
			char symlink_path[32]{ 0 };
			snprintf(symlink_path, sizeof(symlink_path), "/proc/self/fd/%d", fd);
			readlink(symlink_path, path, sizeof(path) - 1);
			return path;
		}
#else
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
		std::filesystem::path _path;
#endif // __unix__
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

	std::vector<std::string> list_all_files_in_directory(const char* path) {
		auto dir_d = opendir(path);
		if (dir_d == nullptr) {
			return {};
		}
		std::vector<std::string> res;
		struct dirent* ptr = readdir(dir_d);
		std::string _path = path;
		if (_path.back() != '/') {
			_path += '/';
		}
		while ((ptr = readdir(dir_d)) != nullptr) {
			if (strcmp(ptr->d_name, ".") == 0 || strcmp(ptr->d_name, "..") == 0) {
				continue;
			}
			if (ptr->d_type == DT_REG) {
				res.emplace_back(std::format("{}{}", _path, ptr->d_name));
			} else if (ptr->d_type == DT_DIR) {
				auto son_dir = list_all_files_in_directory(ptr->d_name);
				for (auto& file : son_dir) {
					res.emplace_back(_path + file);
				}
			}
		}
		return res;
	}
}
#endif // !IO_HPP
