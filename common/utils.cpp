#include <iostream>
#include <algorithm>
#include <string.h>
#include <sstream>
#include <fstream>
#include <cmath>
#include <chrono>

#include <unistd.h>
#include <grp.h>        // getgrnam
#include <pwd.h>
#include <dirent.h>

#include <limits.h>
#include <stdlib.h>

#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/crc.hpp>
#include <boost/filesystem.hpp>

#include <sys/stat.h>

#include "utils.hpp"

namespace utils
{

// cppcheck-suppress unusedFunction
std::string int2ipv4(uint32_t ip)
{
    char buf[128];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip&0xFF, (ip&0xFF00) >> 8, (ip&0xFF0000) >> 16, (ip&0xFF000000) >> 24);
    return buf;
}

std::string gen_random(int len)
{
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    std::string s(len, '\0');
    for (int i = 0; i < len; ++i)
    {
        s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }

    return s;
}

std::pair<std::string, int> host_from_link(const std::string &link)
{
    std::string shema = "//";
    std::string::size_type pos1 = link.find(shema, 0);
    if (pos1 == std::string::npos)
    {
        return std::make_pair("", -1);
    }

    std::string host;

    std::string::size_type pos2 = link.find("/", pos1 + shema.size());
    if (pos2 == std::string::npos)
    {
        host = link.substr(pos1 + shema.size(), std::string::npos);
    }
    else
    {
        host = link.substr(pos1 + shema.size(), pos2 - pos1 - shema.size());
    }

    int port = -1;

    std::vector<std::string> parts = utils::split(host, ":");
    if (parts.size() == 1)
    {
        // do nothing, it is only host
    }
    else if (parts.size() == 2)
    {
        host = parts[0];
        try
        {
            port = std::stoi(parts[1]);
        }
        catch (const std::exception &e)
        {
            return std::make_pair("", -1);
        }
    }
    else
    {
        return std::make_pair("", -1);
    }
    return std::make_pair(host, port);
}


std::vector<std::string> split(const std::string &line, const std::string &delimiter, int max_parts)
{
    int parts = 0;
    std::vector<std::string> strs;
    size_t prev_pos = 0;
    size_t pos = 0;
    while ((pos = line.find(delimiter, prev_pos)) != std::string::npos)
    {
        ++parts;
        std::string tmp(line.begin() + prev_pos, line.begin() + pos);
        strs.emplace_back(tmp);
        prev_pos = pos + delimiter.size();

        if (parts == max_parts - 1)
            break;
    }

    if (prev_pos <= line.size())
    {
        std::string tmp(line.begin() + prev_pos, line.end());
        strs.emplace_back(tmp);
    }

    return strs;
}

std::vector<std::string> split(const std::string &line, const std::string &delimiter)
{
    return split(line, delimiter, -1);
}

std::string join(const std::vector<std::string> &vec, const std::string &connector)
{
    std::stringstream ss;
    bool first = true;
    for (auto const& it : vec)
    {
        if (!first) ss << connector;
        ss << it;
        first = false;
    }
    return ss.str();
}

bool starts_with(const std::string &s, const std::string &predicate)
{
    if (strncmp(s.data(), predicate.data(), predicate.size()) == 0)
        return true;
    return false;
}

// cppcheck-suppress unusedFunction
bool ends_with(const std::string &s, const std::string &predicate)
{
    if (s.size() < predicate.size())
        return false;

    if (strncmp(s.data() + s.size() - predicate.size(), predicate.data(), predicate.size()) == 0)
        return true;

    return false;
}

std::string lowercased(const std::string &str)
{
    std::string ret;
    ret.resize(str.size());
    std::transform(str.begin(), str.end(), ret.begin(), ::tolower);
    return ret;
}

// trim from start (in place)
void ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                std::not1(std::ptr_fun<int, int>(std::isspace))));
}

// trim from end (in place)
void rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
                std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
}

// trim from both ends (in place)
void trim(std::string &s)
{
    ltrim(s);
    rtrim(s);
}

// trim from start (copying)
// cppcheck-suppress unusedFunction
std::string ltrimmed(std::string s)
{
    ltrim(s);
    return s;
}

// trim from end (copying)
// cppcheck-suppress unusedFunction
std::string rtrimmed(std::string s)
{
    rtrim(s);
    return s;
}

// trim from both ends (copying)
std::string trimmed(std::string s)
{
    trim(s);
    return s;
}

std::string dequoted(std::string const& str)
{
    if (str.empty()) return str;

    size_t beg = 0, end = str.size();
    if (str.front() == '\"')
    {
        if (end < 2 || str.back() != '\"') return str;
        ++beg; --end;
    }

    std::ostringstream s;
    while (beg < end)
    {
        size_t i = str.find('\\', beg);
        if (i == std::string::npos || i + 1 >= end)
        {
            s << str.substr(beg, end - beg);
            beg = end;
        }
        else
        {
            s << str.substr(beg, i - beg);
            switch (str[i + 1])
            {
                case '\\': s << '\\'; beg = i + 2; break;
                case '\"': s << '\"'; beg = i + 2; break;
                default: s << '\\'; beg = i + 1; break;
            }
        }
    }

    return s.str();
}

std::string filename(const std::string &path)
{
    size_t pos = path.rfind("/");
    if (pos == std::string::npos)
        return path;

    std::string res = path.substr(pos + 1, std::string::npos);
    return res;
}

bool file_exist(const std::string &path)
{
    std::ifstream ifs(path.c_str());
    if (!ifs.good())
    {
        return false;
    }
    return true;
}

// cppcheck-suppress unusedFunction
void do_chown(const char *file_path, const char *user_name, const char *group_name)
{
    uid_t          uid;
    gid_t          gid;
    struct passwd *pwd;
    struct group  *grp;

    pwd = getpwnam(user_name);  // NOLINT(runtime/threadsafe_fn)
    if (pwd == NULL)
    {
        throw std::runtime_error("Failed to get uid");
    }
    uid = pwd->pw_uid;

    grp = getgrnam(group_name);  // NOLINT(runtime/threadsafe_fn)
    if (grp == NULL)
    {
        throw std::runtime_error("Failed to get gid");
    }
    gid = grp->gr_gid;

    if (chown(file_path, uid, gid) == -1)
    {
        throw std::runtime_error("chown fail");
    }
}

void change_process_privileges(const char *in_username)
{
    // Not thread safe
    gid_t gid = getgid();
    gid_t uid = getuid();

    if ((gid != 0) && (uid != 0))
    {
        // Nothing to do -- not root to drop root privileges"?
        // Need to throw exception?
        // Possibly, if no permission, exception will be catched in code below
    }

    struct passwd *pw = getpwnam(in_username);  // NOLINT(runtime/threadsafe_fn)
    if (!pw)
    {
        throw std::runtime_error("couldn't find user uid: " + std::string(in_username));
    }

    if (initgroups(in_username, pw->pw_gid) != 0)
    {
        std::string e = std::string("couldn't change to: ") + in_username;
        e += " (initgroups(" + std::string(in_username) + ", " + std::to_string(pw->pw_gid) + ")): " + strerror(errno);
        throw std::runtime_error(e);
    }

    if (setgid(pw->pw_gid) != 0)
    {
        throw std::runtime_error(std::string("couldn't change to: ") + in_username + ", setgid: " + strerror(errno));
    }

    if (setegid(pw->pw_gid) != 0)
    {
        throw std::runtime_error(std::string("couldn't change to: ") + in_username + ", setegid: " + strerror(errno));
    }

    if (setuid(pw->pw_uid) != 0)
    {
        throw std::runtime_error(std::string("couldn't change to: ") + in_username + ", setuid: " + strerror(errno));
    }

    if (seteuid(pw->pw_uid) != 0)
    {
        throw std::runtime_error(std::string("couldn't change to: ") + in_username + ", seteuid: " + strerror(errno));
    }
}

std::string to_base64(const std::string &s)
{
    typedef boost::archive::iterators::base64_from_binary<boost::archive::iterators::transform_width<std::string::const_iterator, 6, 8> > it_base64_t;
    unsigned int writePaddChars = (3 - s.length()%3)%3;
    std::string base64(it_base64_t(s.begin()), it_base64_t(s.end()));
    base64.append(writePaddChars, '=');
    return base64;
}

std::string to_base64(const std::vector<unsigned char> &v)
{
    typedef boost::archive::iterators::base64_from_binary
        <boost::archive::iterators::transform_width
            <std::vector<unsigned char>::const_iterator, 6, 8>> it_base64_t;
    unsigned int writePaddChars = (3 - v.size() % 3) % 3;
    std::string base64(it_base64_t(v.begin()), it_base64_t(v.end()));
    base64.append(writePaddChars, '=');
    return base64;
}

std::string get_home_dir()
{
    uid_t uid = geteuid();
    struct passwd *pw = getpwuid(uid);  // NOLINT(runtime/threadsafe_fn)

    if (!pw)
    {
        return "";
    }
    std::string path = std::string(pw->pw_dir);
    return path;
}

bool is_dir(const std::string &path)
{
    return boost::filesystem::is_directory(path);
}

std::vector<std::string> listdir(const std::string& path)
{
    std::vector<std::string> res;

    DIR* d;
    struct dirent *dir;
    d = opendir(path.c_str());
    if (d)
    {
        while ((dir = readdir(d)) != NULL)
        {
            std::string name = dir->d_name;
            if (name != "." && name != "..")
                res.push_back(name);
        }

        closedir(d);
    }
    else
    {
        throw std::runtime_error((path + ": Failed to open directory").c_str());
    }
    return res;
}

std::string concat_dir_file(const std::string& dir, const std::string& file)
{
    if (dir.back() == '/')
        return dir + file;
    else
        return dir.substr(0, dir.size()) + '/' + file;
}

std::string filepath(const std::string &filename)
{
    size_t pos = filename.rfind("/");
    if (pos == std::string::npos)
        return filename;

    std::string res = filename.substr(0, pos);
    return res;
}

std::string read_file(const std::string &path)
{
    if (path.empty())
    {
        throw std::runtime_error("path is empty");
    }

    std::ifstream ifs(path.c_str());
    if (!ifs.good())
    {
        throw std::runtime_error(std::string("cannot open file: ") + path + std::string(", errno: ") + strerror(errno));
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
            std::istreambuf_iterator<char>());
    return content;
}

int crc32(const std::string &data)
{
    boost::crc_32_type result;
    result.process_bytes(data.data(), data.length());
    return result.checksum();
}

int crc32(const std::vector<unsigned char> &data)
{
    boost::crc_32_type result;
    result.process_bytes(data.data(), data.size());
    return result.checksum();
}

std::string process_name_by_pid(const int pid)
{
    char name[1024];
    snprintf(name, sizeof(name), "/proc/%d/cmdline", pid);
    FILE* f = fopen(name, "r");
    if (f)
    {
        size_t size;
        size = fread(name, sizeof(char), 1024, f);
        if (size > 0)
        {
            if ('\n' == name[size-1])
            {
                name[size-1]='\0';
            }
        }
        fclose(f);
    }
    return name;
}

double truncate_double(double d, int n)
{
    static const double power_of_ten[] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0 };
    assert(n < sizeof(power_of_ten)/sizeof(power_of_ten[0]));
    double truncated = std::trunc(d * power_of_ten[n]) / power_of_ten[n];
    return truncated;
}

double round_double(double d, int n)
{
    static const double power_of_ten[] = { 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0 };
    assert(n < sizeof(power_of_ten)/sizeof(power_of_ten[0]));
    double rounded = std::floor(d * power_of_ten[n] + 0.5) / power_of_ten[n];
    return rounded;
}

time_t file_mtime(const std::string &filename)
{
    struct stat result;
    if (stat(filename.c_str(), &result) != 0)
    {
        throw std::runtime_error("error gettings file stat: " + filename);
    }

    return result.st_mtim.tv_sec;
}

size_t file_size(const std::string &filename)
{
    struct stat result;

    if (stat(filename.c_str(), &result) != 0)
    {
        throw std::runtime_error("error gettings file stat: " + filename);
    }

    return result.st_size;
}

namespace unix
{

std::string my_hostname()
{
    char host_name[HOST_NAME_MAX];
    if (gethostname(host_name, sizeof(host_name)) != 0 || host_name[0] == '\0')
    {
        throw std::runtime_error(std::string("error get hostname: ") + strerror(errno));
    }
    return std::string(host_name);
}

}   // namespace unix


namespace time
{

std::ostream& operator<<(std::ostream& os, const mtime_t &t)
{
    return os << t.sec << "." << t.msec;
}

mtime_t ms()
{
    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    mtime_t t;
    t.sec = ms.count() / 1000;
    t.msec = ms.count() % 1000;
    return t;
}

}   // namespace time
}   // namespace utils
