#include "libproperty/src/libproperty.hpp"
#include "o2logger/src/o2logger.hpp"

#include "server.hpp"
#include "common/utils.hpp"

using namespace o2logger;


void check_config_or_die(libproperty::Config *, libproperty::Options *opt)
{
    if (opt->get<std::string>("sert").empty())
    {
        loge("require to specify sert file");
        exit(-1);
    }
}

void drop_privileges(const std::string &run_as)
{
    if (run_as.empty())
    {
        logw("don't change process owner");
        return;
    }
    logi("change process owner to: ", run_as);
    utils::change_process_privileges(run_as.c_str());
}

int main(int argc, char *argv[])
{
    srand(getpid() ^ time(NULL));
    libproperty::Options *opt = libproperty::Options::impl();
    opt->add("help", "h", "print help and exit", false);
    opt->add("port", "p", "port to listen to (1025..65536)", 7788);
    opt->add("loglevel", "l", "loglevel (1..5)", 0);
    opt->add("sert", "", "path to .pem file", "");
    opt->add("syslog", "", "write logs into syslog", false);
    opt->add("run_as", "", "user which should be process owner", "");
    opt->add("io_workers", "", "count of threads to process io", 8);
    opt->add("pass_len", "", "how string should be password", 8);

    try
    {
        opt->parse(argc, argv);
    }
    catch (const std::exception &e)
    {
        loge("fatal: ", e.what());
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

    try
    {
        Server s(opt->get<int>("port"), opt->get<int>("io_workers"));

        drop_privileges(opt->get<std::string>("run_as"));
        s.run();
    }
    catch (const std::exception &e)
    {
        loge("api exception: ", e.what());
    }
    return 0;
}
