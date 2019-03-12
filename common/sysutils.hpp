#pragma once

#include <unistd.h>

#include <string>
#include <vector>

class BackendUtils
{
public:
    struct ProcessPropertie
    {
        pid_t pid;
        std::string name;
    };
    static std::vector<ProcessPropertie> processList();
    static std::string execCmd(const char *cmd);
    static int resourceUsage(pid_t pid, size_t &virtual_mem, size_t &res_mem, double &sys_cpu, double &user_cpu, double &cpu);
};
