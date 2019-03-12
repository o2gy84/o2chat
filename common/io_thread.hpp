#pragma once

#include <boost/asio/io_service.hpp>
#include <thread>

#include "o2logger/src/o2logger.hpp"
using namespace o2logger;


class IoThread
{
public:
    explicit IoThread(const std::string &sert) :
        m_Context(m_IoService, boost::asio::ssl::context::sslv23),
        m_Redbull(m_IoService)
    {
        m_Context.set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2
            | boost::asio::ssl::context::single_dh_use);

        if (!sert.empty())
        {
            m_Context.use_certificate_chain_file(sert);
            m_Context.use_private_key_file(sert, boost::asio::ssl::context::pem);
        }
    }

    boost::asio::io_service &ioService()
    {
        return m_IoService;
    }

    boost::asio::ssl::context &sslContext()
    {
        return m_Context;
    }

    void start()
    {
        m_IoService.reset();
        m_Thread = std::thread(
            [this]()
            {
                this->run();
            });
    }

    void stop()
    {
        m_IoService.stop();
    }

    void join()
    {
        if (m_Thread.joinable())
        {
            m_Thread.join();
        }
    }

private:
    void run()
    {
        while (!m_IoService.stopped())
        {
            try
            {
                m_IoService.run();
            }
            catch (const boost::system::system_error &e)
            {
                loge("io thread loop exception: unknown reason(boost::system::system_error)");
            }
            catch (const std::exception& e)
            {
                loge("io thread loop exception: ", e.what());
            }
            catch (...)
            {
                loge("io thread loop exception: unknown reason");
            }
        }
    }

private:
    boost::asio::io_service m_IoService;
    boost::asio::ssl::context m_Context;

    std::thread m_Thread;

    boost::asio::io_service::work m_Redbull;
};
