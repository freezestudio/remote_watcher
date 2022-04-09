#ifndef SERVICE_UTILS_H
#define SERVICE_UTILS_H

#include <bit>
#include <bitset>
#include <string>
#include <vector>
#include <format>
#include <memory>
#include <utility>
#include <filesystem>
#include <fstream>
#include <tuple>
#include <regex>
#include <semaphore>
#include <thread>

#include "service_utils_ex.h"

namespace fs = std::filesystem;
using namespace std::literals;
using volume_type = std::tuple<std::string, std::string, std::string>;
// using disk_type = std::tuple<std::string, std::string>;

inline constexpr auto EMPTY_IP = static_cast<uint32_t>(-1);
inline constexpr auto BAD_LEN_IP = static_cast<uint32_t>(-2);
inline constexpr auto BAD_IP = static_cast<uint32_t>(-3);

#if defined(_DEBUG) || defined(DEBUG)
template <typename... Args>
void debug_output(std::wstring_view const &fmt, Args &&...args)
{
	auto msg = std::format(fmt, args...);

	// test only
	// std::wcout << msg << std::endl;

	OutputDebugString(msg.data());
}
#else
#define debug_output(...)
#endif

namespace freeze::detail
{
	std::string to_utf8(const wchar_t *, int);
	std::string to_utf8(std::wstring const &);
	std::wstring to_utf16(std::string const &);
}

namespace freeze::detail
{
	uint32_t make_ip_address(std::wstring const &);
	std::string parse_ip_address(uint32_t);
	bool check_ip_address(uint32_t);
}

namespace freeze::detail
{
	fs::path to_normal(fs::path const &path);
	bool check_exists(fs::path const &path);
	bool normal_check_exists(fs::path const &path);
}

namespace freeze::detail
{
	enum class response_type : int
	{
		result,
		status,
		overlapped,
	};
	std::wstring to_str(response_type t);
	std::wstring to_str(uint32_t notify);
}

namespace freeze::detail
{
	bool save_ip(uint32_t);
	uint32_t read_ip();
	bool save_latest_folder(std::wstring const &);
	std::wstring read_latest_folder();
	bool save_latest_ignores(std::vector<std::wstring> const &);
	std::vector<std::wstring> read_latest_ignores();
	bool save_token(std::wstring const &);
	std::string read_token();
}

namespace freeze::detail
{
	struct tree_information
	{
		std::string file_path;
		std::string file_name;
		uint64_t file_size; // bytes
	};
}

namespace freeze::detail
{
	std::vector<std::string> get_harddisks();
	std::vector<std::string> get_harddisks_ex();
	std::vector<std::string> get_harddisk_names();
	std::vector<volume_type> get_volume_names();
	std::vector<std::string> get_directories_without_subdir(fs::path const &);
	std::vector<std::string> get_files_without_subdir(fs::path const &);
	std::vector<tree_information> get_dirtree_info(fs::path const &, std::vector<fs::path> const &);
	std::vector<fs::path> get_dirtree_paths(fs::path const &, std::vector<fs::path> const &);
}

namespace freeze::detail
{
	bool read_file(fs::path const &, uintmax_t, uint8_t *);
	bool read_file_ex(fs::path const &, uintmax_t, uint8_t *);
}

namespace freeze
{
	class regkey_semaphore
	{
	public:
		regkey_semaphore();

	private:
		regkey_semaphore(regkey_semaphore const &) = delete;
		regkey_semaphore &operator=(regkey_semaphore const &) = delete;

	public:
		void wait();
		void notify();
		void reset();

	private:
		std::binary_semaphore _singal{0};
	};

	class locked_regkey
	{
	public:
		locked_regkey();
		~locked_regkey();
	public:
		static locked_regkey& instance();

	public:
		void save_latest_folder(std::wstring const &);
		std::wstring read_latest_folder();

	private:
		void thread_work();
		void thread_loop();

	private:
		std::jthread _thread;
		regkey_semaphore _semaphore;
		std::wstring _folder;
		bool _save{false};
		bool _running{false};
	};
}

#endif
