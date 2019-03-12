#pragma once
#include <sys/time.h>
#include <vector>
#include <string>

template<class... Args>
inline void unused_args(const Args&...)
{
    // Do nothing.
}

// functions
namespace utils
{

std::string gen_random(int len);

std::pair<std::string, int> host_from_link(const std::string &link);

std::string int2ipv4(uint32_t ip);

std::vector<std::string> split(const std::string &line, const std::string &delimiter);
std::vector<std::string> split(const std::string &line, const std::string &delimiter, int max_parts);
std::string join(const std::vector<std::string> &vec, const std::string &connector);

bool starts_with(const std::string &s, const std::string &predicate);
bool ends_with(const std::string &s, const std::string &predicate);

std::string lowercased(const std::string &str);
std::string trimmed(std::string s);
std::string ltrimmed(std::string s);
std::string rtrimmed(std::string s);

std::string dequoted(std::string const& str);

std::string filename(const std::string &path);
bool file_exist(const std::string &path);

std::string to_base64(const std::string &s);
std::string to_base64(const std::vector<unsigned char> &v);

void do_chown(const char *file_path, const char *user_name, const char *group_name);
void change_process_privileges(const char *in_username);

std::string get_home_dir();

bool is_dir(const std::string &path);
std::vector<std::string> listdir(const std::string &path);
std::string concat_dir_file(const std::string &dir, const std::string &file);
std::string filepath(const std::string &filename);
std::string read_file(const std::string &path);
int crc32(const std::string &data);
int crc32(const std::vector<unsigned char> &data);

std::string process_name_by_pid(const int pid);

double truncate_double(double d, int n);
double round_double(double d, int n);
time_t file_mtime(const std::string &filename);
size_t file_size(const std::string &filename);

namespace unix
{

std::string my_hostname();

}   // namespace unix


namespace time
{

struct mtime_t
{
    uint64_t sec;
    uint64_t msec;
    friend std::ostream& operator<<(std::ostream& os, const mtime_t &t);
};

std::ostream& operator<<(std::ostream& os, const mtime_t &t);
mtime_t ms();

}   // namespace time
}   // namespace utils

// classess
namespace utils
{

// if there isn't chrono
class PerfTimer
{
public:
    PerfTimer()
    {
        reset();
    }
    void reset()
    {
        _timer = 0;
        gettimeofday(&_start, 0);
    }
    uint64_t ms()
    {
        timeval tmp;
        gettimeofday(&tmp, 0);
        _timer += int64_t(tmp.tv_sec - _start.tv_sec) * 1000000 + int64_t(tmp.tv_usec - _start.tv_usec);
        return _timer/1000;
    }
    uint64_t sec()
    {
        timeval tmp;
        gettimeofday(&tmp, 0);
        _timer += int64_t(tmp.tv_sec - _start.tv_sec) * 1000000 + int64_t(tmp.tv_usec - _start.tv_usec);
        return _timer/1000000;
    }

private:
    int64_t _timer;             // microseconds
    timeval _start;
};

}   // namespace utils
