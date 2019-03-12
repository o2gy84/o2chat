#pragma once


namespace
{
    std::function<bool(const std::string&, const std::string&)> string_comparator =
                            [](const std::string &lhs, const std::string &rhs)
                            {
                                return ::strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
                            };
}


struct HttpReply
{
    HttpReply() : _status(0), _parsedHeaders(string_comparator) {}

    bool hasHeader(const std::string &header) const;
    std::string getHeader(const std::string &header) const;
    std::string asString() const { return _headers + std::string("\n\n") + _body; }

    int _status;
    std::string _headers;
    std::string _body;

    std::string _method;
    std::string _resource;

    std::map<std::string, std::string, std::function<bool(const std::string&, const std::string&)>> _parsedHeaders;
};

