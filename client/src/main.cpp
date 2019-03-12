#include "libproperty/src/libproperty.hpp"
#include "o2logger/src/o2logger.hpp"

#include "client.hpp"
#include "common/utils.hpp"


using namespace o2logger;


void check_config_or_die(libproperty::Config *conf, libproperty::Options *opt)
{
    unused_args(conf);
    std::string server = opt->get<std::string>("server");
    if (server.empty())
    {
        std::cerr << "server is required" << std::endl;
        exit(-1);
    }

    std::vector<std::string> ip_parts = utils::split(server, ":");
    if (ip_parts.size() != 2)
    {
        std::cerr << "invalid server: " << "server" << std::endl;
        exit(-1);
    }

    try
    {
        std::stoi(ip_parts[1]);
    }
    catch (...)
    {
        std::cerr << "invalid server: " << "server" << std::endl;
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    srand(getpid() ^ time(NULL));
    libproperty::Options *opt = libproperty::Options::impl();
    opt->add("help", "h", "print help and exit", false);
    opt->add("server", "s", "server to connect to", "");
    opt->add("loglevel", "l", "loglevel (1..5)", 0);
    opt->add("syslog", "", "write logs into syslog", false);
    opt->add("user", "u", "user", "");
    opt->add("password", "p", "password", "");

    try
    {
        opt->parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }

    o2logger::Logger::impl().setOptionLogLevel(opt->get<int>("loglevel"));
    o2logger::Logger::impl().setOptionSyslog(argv[0], opt->get<bool>("syslog"));

    logi(opt->dump());
    if (opt->get<bool>("help"))
    {
        std::cout << opt->usage(argv[0]) << std::endl;
        return 0;
    }

    check_config_or_die(libproperty::Config::impl(), opt);

    std::vector<std::string> ip_parts = utils::split(opt->get<std::string>("server"), ":");
    try
    {
        Client c(ip_parts[0],
                 std::stoi(ip_parts[1]),
                 opt->get<std::string>("user"),
                 opt->get<std::string>("password"));
        c.run();
    }
    catch (const std::exception &e)
    {
        loge("api exception: ", e.what());
    }

    return 0;
}
