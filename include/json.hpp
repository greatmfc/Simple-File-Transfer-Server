#ifndef JSHPP
#define JSHPP
#include <variant>
#include <optional>
#include "util.hpp"
#include "io.hpp"
using val_type = std::variant<long long, bool, std::string, std::nullptr_t>;
namespace mfcslib {
	enum {
		VTYPE,
		ATYPE,
		KVTYPE
	};
	struct Val
	{
		val_type _val;
		std::vector<Val> _arr;
		std::unordered_map<std::string, Val> _obj;
		int _type = 0;
		Val() = default;
		Val(const Val&) = delete;
		Val(Val&& other) {
			_type = other._type;
			other._type = 0;
			switch (_type)
			{
			case VTYPE:_val = std::move(other._val); break;
			case ATYPE:_arr = std::move(other._arr); break;
			case KVTYPE:_obj = std::move(other._obj); break;
			}
		}
		Val& operator=(Val&& other) {
			_type = other._type;
			other._type = 0;
			switch (_type)
			{
			case VTYPE:_val = std::move(other._val); break;
			case ATYPE:_arr = std::move(other._arr); break;
			case KVTYPE:_obj = std::move(other._obj); break;
			}
			return *this;
		}
		Val(val_type&& _) :_val(_), _type(VTYPE) {}
		Val(std::string&& s, Val&& v) {
			_type = KVTYPE;
			_obj[s] = std::move(v);
		}
		std::optional<Val> find(const std::string& key) {
			if (_type == VTYPE) return {};
			if (_type == ATYPE) {
				for (auto& _value : _arr) {
					auto res = _value.find(key);
					if (res) return res;
				}
			}
			else {
				for (auto& [_key, _value] : _obj) {
					if (_key == key) return std::move(_value);
					else {
						auto res = _value.find(key);
						if (res) return res;
					}
				}
			}
			return {};
		}
		template<typename T>
		T* at() {
			return std::get_if<T>(&_val);
		}
	};

	class json_parser
	{
	public:
		json_parser() = default;
		json_parser(mfcslib::File& in) {
			TypeArray<char> buf(in.size() + 1);
			in.read(buf);
			pt = buf.get_ptr();
			_json = parse_val();
			pt = nullptr;
		}
		~json_parser() = default;
	private:
		Val parse_kv();
		Val parse_str() {
			++pt;
			std::string str;
			while (*pt != '"') {
				str += *pt++;
			}
			++pt;
			return val_type(str);
		}
		Val parse_num() {
			std::string num;
			while (*pt >= '0' && *pt <= '9') {
				num += *pt++;
			}
			return val_type(std::stoll(num));
		}
		Val parse_array() {
			Val arr;
			++pt;
			arr._type = ATYPE;
			while (*pt != ']') {
				arr._arr.emplace_back(parse_val());
				while (1) {
					switch (*pt)
					{
					case '\t':
					case '\n':
					case '\r':
					case ',': [[fallthrough]];
					case ' ':++pt; continue;
					}
					break;
				}
			}
			++pt;
			return arr;
		}
		Val parse_val() {
			while (*pt) {
				switch (*pt)
				{
				case '\t':
				case '\n':
				case '\r': [[fallthrough]];
				case ' ':++pt; continue;
				case '[':return parse_array();
				case '{':return parse_kv();
				case '"':return parse_str();
				case 'f':pt += 5; return val_type(false);
				case 't':pt += 4; return val_type(true);
				case 'n':pt += 4; return val_type(nullptr);
				default: return parse_num();
				}
			}
			return{};
		}
	public:
		void parse(const mfcslib::File& in) {
			TypeArray<char> buf(in.size() + 1);
			in.read(buf);
			pt = buf.get_ptr();
			_json = parse_val();
			pt = nullptr;
		}
		auto find(const std::string& key) {
			return _json.find(key);
		}
		auto& get_obj() {
			return _json._obj;
		}
		auto& get_arr() {
			return _json._arr;
		}
	private:
		Val _json;
		char* pt = nullptr;
	};

	Val json_parser::parse_kv()
	{
		++pt;
		Val kv;
		kv._type = KVTYPE;
		while (*pt != '}') {
			decltype(auto) tmp = parse_val()._val;
			std::string _key = *std::get_if<std::string>(&tmp);
			while (*pt == ' ' || *pt == ':') ++pt;
			Val __val = parse_val();
			kv._obj.emplace(_key, std::move(__val));
			while (1) {
				switch (*pt)
				{
				case ' ':
				case '\t':
				case '\n':
				case '\r':
				case ',':++pt; continue;
				}
				break;
			}
		}
		++pt;
		return kv;
	}
}
#endif // !JSHPP
