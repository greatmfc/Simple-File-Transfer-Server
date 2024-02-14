#ifndef HTTP_HPP
#define HTTP_HPP
#include "util.hpp"
#include "io.hpp"
namespace mfcslib {
	enum http_type
	{
		http10,
		http11,
		http2
	};

	#define hd_host "host"
	#define hd_connection "connection"
	#define hd_method "method"
	#define hd_edition "edition"
	#define hd_path "path"
	#define hd_range "range"
	#define hd_content_length "content_length"
	#define hd_post_content "post_content"

	class response_header
	{
	public:
		response_header(int status_code) {
			_response = "HTTP/1.1 ";
			switch (status_code) {
			case 200:
				_response += "200 OK\r\n";
				break;
			case 404:
				_response += "404 Not Found\r\n";
				break;
			case 206:
				_response += "206 Partial Content\r\n";
				break;
			case 403:
				_response += "403 Forbidden\r\n";
				break;
			case 500:
				_response += "500 Internal Server Error\r\n";
				break;
			default:
				_response += std::to_string(status_code) + "\r\n";
				//throw std::invalid_argument(std::format("Unsupported status code:{}.", status_code));
				break;
			}
		}
		response_header(int edition, int status_code) {
			switch (edition) {
			case http10:
				_response = "HTTP/1.0 ";
				break;
			case http2:
				_response = "HTTP/2 ";
				break;
			case http11:
			default:
				_response = "HTTP/1.1 ";
				break;
			}

			switch (status_code) {
			case 200:
				_response += "200 OK\r\n";
				break;
			case 404:
				_response += "404 Not Found\r\n";
				break;
			case 206:
				_response += "206 Partial Content\r\n";
				break;
			case 403:
				_response += "403 Forbidden\r\n";
				break;
			case 500:
				_response += "500 Internal Server Error\r\n";
				break;
			default:
				_response += std::to_string(status_code) + "\r\n";
				//throw std::invalid_argument(std::format("Unsupported status code:{}.", status_code));
				break;
			}
		}
		~response_header() = default;

		[[deprecated]]
		void add_status_code(int status_code) {
			switch (status_code) {
			case 200:
				_response += "200 OK\r\n";
				break;
			case 404:
				_response += "404 Not Found\r\n";
				break;
			case 206:
				_response += "206 Partial Content\r\n";
				break;
			case 403:
				_response += "403 Forbidden\r\n";
				break;
			case 500:
				_response += "500 Internal Server Error\r\n";
				break;
			default:
				_response += std::to_string(status_code) + "\r\n";
				//throw std::invalid_argument(std::format("Unsupported status code:{}.", status_code));
				break;
			}
		}

		void add_content_length(ssize_t length) {
			_response += "Content-Length: ";
			_response += std::to_string(length) + "\r\n";
		}

		void add_content_length(mfcslib::File& file) {
			_response += "Content-Length: ";
			_response += file.size_string() + "\r\n";
		}

		void add_server_info() {
			_response += "Server: Simple-File-Transfer\r\n";
		}

		constexpr void add_server_info(const char* version) {
			_response = _response + "Server: Simple-File-Transfer" + version + "\r\n";
		}

		constexpr inline void add_connection_type(bool is_closed) {
			if (is_closed) {
				_response += "Connection: close\r\n";
			} else {
				_response += "Connection: keep-alive\r\n";
			}
		}

		void add_line(const string& str) {
			_response += str + "\r\n";
		}

		void add_content_range(ssize_t first_byte_pos, ssize_t last_byte_pos, ssize_t file_length) {
			_response += "Content-Range: bytes ";
			_response += std::to_string(first_byte_pos) + '-';
			_response += std::to_string(last_byte_pos) + '/';
			_response += std::to_string(file_length) + "\r\n";
		}

		void add_blank_line() {
			_response += "\r\n";
		}

		auto data() {
			return _response;
		}

		void add_content_type(const string& type) {
			if (auto ite = content_type.find(type); ite != content_type.end()) {
				_response += ite->second + "\r\n";
			}
			else {
				_response += content_type[".*"] + "\r\n";
			}
		}

	private:
		std::string _response;
		std::unordered_map<string, string> content_type = {
	{".html","Content-Type: text/html; charset=utf-8"},
	{".txt","Content-Type: text/plain; charset=utf-8"},
	{".jpg","Content-Type: image/jpeg;"},
	{".png","Content-Type: image/png;"},
	{".mp4","Content-Type: video/mpeg4;"},
	{".mkv","Content-Type: video/mkv;"},
	{".pdf","Content-Type: application/pdf;"},
	{".zip","Content-Type: application/zip;"},
	{".*","Content-Type: application/octet-stream;"}
		};
	};

	template<typename String_Type>
	auto parse_http_request(const String_Type& raw_header) {
		std::unordered_map<std::string, std::string> parse_result;
		auto header_lines = str_split(raw_header, "\r\n");
		/*
		Parse request line.
		*/
		auto& first_line = header_lines[0];
		const char* lidx = first_line.data();
		auto ridx = lidx;
		if (*ridx++ == 'P') {
			parse_result[hd_method] = "POST";
		} else {
			parse_result[hd_method] = "GET";
		}
		while (*ridx++ != '/');
		if (*ridx != ' ') {
			lidx = ridx;
			while (*ridx++ != ' ');
			parse_result[hd_path] = string(lidx, ridx - 1);
		}
		while (*ridx++ != '/');
		if (*ridx == '1') {
			parse_result[hd_edition] = string(ridx, 3);
		} else {
			parse_result[hd_edition] = *ridx;
		}

		/*
		Parse request header.
		*/
		size_t i = 1;
		for (; i < header_lines.size(); i++) {
			auto& line = header_lines[i];
			auto idx = line.find_first_of(':');
			[[unlikely]] if (idx == string::npos) break;
			parse_result[line.substr(0, idx)] = line.substr(idx + 2);
		}
		if (++i < header_lines.size()) {
			parse_result[hd_post_content] = header_lines[i];
		}
		return parse_result;
	}
}

//rewrite exceptions for File
#endif
