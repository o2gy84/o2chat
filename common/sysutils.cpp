#include <sys/types.h>
#include <sys/times.h>
#include <dirent.h>
#include <string.h>

#include <array>
#include <memory>
#include "sysutils.hpp"



namespace
{

struct pstat
{
    long unsigned int utime_ticks;      //                               NOLINT
    long int cutime_ticks;              //                               NOLINT
    long unsigned int stime_ticks;      //                               NOLINT
    long int cstime_ticks;              //                               NOLINT
    long unsigned int vsize;            // virtual memory size in bytes  NOLINT
    long unsigned int rss;              // resident set size in bytes    NOLINT
    long unsigned int cpu_total_time;   //                               NOLINT
};

int get_usage(const pid_t pid, struct pstat* result)
/*
 * read /proc data into the passed struct pstat
 * returns 0 on success, -1 on error
 */
{
    std::string proc_pid = "/proc/";
    proc_pid += std::to_string(pid) + "/stat";

    {
        FILE *fpstat = fopen(proc_pid.c_str(), "r");
        if (fpstat == NULL)
        {
            return -1;
        }

        // read values from /proc/pid/stat
        bzero(result, sizeof(struct pstat));
        long int rss;   // NOLINT
        if (fscanf(fpstat, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu"
                    "%lu %ld %ld %*d %*d %*d %*d %*u %lu %ld",
                    &result->utime_ticks, &result->stime_ticks,
                    &result->cutime_ticks, &result->cstime_ticks, &result->vsize,
                    &rss) == EOF)
        {
            fclose(fpstat);
            return -1;
        }
        fclose(fpstat);
        result->rss = rss * getpagesize();
    }

    {
        FILE *fstat = fopen("/proc/stat", "r");
        if (fstat == NULL)
        {
            return -1;
        }

        // read+calc cpu total time from /proc/stat
        long unsigned int cpu_time[10];     // NOLINT
        bzero(cpu_time, sizeof(cpu_time));
        if (fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
                    &cpu_time[0], &cpu_time[1], &cpu_time[2], &cpu_time[3],
                    &cpu_time[4], &cpu_time[5], &cpu_time[6], &cpu_time[7],
                    &cpu_time[8], &cpu_time[9]) == EOF)
        {
            fclose(fstat);
            return -1;
        }
        fclose(fstat);

        for (int i = 0; i < 10; i++)
        {
            result->cpu_total_time += cpu_time[i];
        }
    }

    return 0;
}

void calc_cpu_usage_pct(const struct pstat* cur_usage, const struct pstat* last_usage, double* ucpu_usage, double* scpu_usage)
/*
 * calculates the elapsed CPU usage between 2 measuring points. in percent
 */
{
    const long unsigned int total_time_diff = cur_usage->cpu_total_time - last_usage->cpu_total_time;       // NOLINT

    *ucpu_usage = 100 * (((cur_usage->utime_ticks + cur_usage->cutime_ticks)
                - (last_usage->utime_ticks + last_usage->cutime_ticks))
            / static_cast<double>(total_time_diff));

    *scpu_usage = 100 * ((((cur_usage->stime_ticks + cur_usage->cstime_ticks)
                    - (last_usage->stime_ticks + last_usage->cstime_ticks))) /
            static_cast<double>(total_time_diff));
}

bool contains_only_numbers(const char* item)
{
    for (; *item; item++)
    {
        if (*item < '0' || *item > '9')
            return false;
    }
    return true;
}

}   // namespace

std::vector<BackendUtils::ProcessPropertie> BackendUtils::processList()
{
    std::vector<BackendUtils::ProcessPropertie> ret;

    DIR *dir_proc = opendir("/proc/");
    if (dir_proc == NULL)
    {
        return ret;
    }

    struct dirent *dir = NULL;
    while ((dir = readdir(dir_proc)))
    {
        if (dir->d_type != DT_DIR)
        {
            continue;
        }

        if (!contains_only_numbers(dir->d_name))
        {
            continue;
        }

        std::string path = "/proc/";
        path += std::string(dir->d_name) + "/cmdline";
        FILE *f = fopen(path.c_str(), "rt");
        if (!f)
        {
            continue;
        }

        char name[1024];
        memset(name, 0, sizeof(name));

        // cppcheck-suppress invalidscanf
        if (fscanf(f, "%s", name) <= 0)
        {
            fclose(f);
            continue;
        }
        fclose(f);

        BackendUtils::ProcessPropertie prop;
        prop.name = name;
        prop.pid = (pid_t)atoi(dir->d_name);
        ret.emplace_back(prop);
    }
    closedir(dir_proc);

    return ret;
}

// cppcheck-suppress unusedFunction
std::string BackendUtils::execCmd(const char* cmd)
{
    std::array<char, 128> buffer;
    std::string result;

    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
    {
        throw std::runtime_error("popen() failed!");
    }

    while (!feof(pipe.get()))
    {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
        {
            result += buffer.data();
        }
    }
    return result;
}

int BackendUtils::resourceUsage(pid_t pid, size_t &virtual_mem, size_t &res_mem, double &sys_cpu, double &user_cpu, double &cpu)
{
    static int num_processors;
    static struct pstat last_usage;

    static int inited = 0;
    if (inited == 0)
    {
        ++inited;
        num_processors = sysconf(_SC_NPROCESSORS_ONLN);
        get_usage(pid, &last_usage);
        return -1;
    }

    struct pstat cur_usage;
    if (get_usage(pid, &cur_usage))
    {
        return -1;
    }

    double ucpu_usage = 0, scpu_usage = 0;
    calc_cpu_usage_pct(&cur_usage, &last_usage, &ucpu_usage, &scpu_usage);

    last_usage = cur_usage;

    virtual_mem = cur_usage.vsize;
    res_mem = cur_usage.rss;
    sys_cpu = num_processors * scpu_usage;
    user_cpu = num_processors * ucpu_usage;
    cpu = num_processors * (ucpu_usage + scpu_usage);
    return 0;
}
