#pragma once
#include <string>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

namespace Mojo {

class Page {
public:
    virtual ~Page() = default;

    virtual bool goto_url(const std::string& url) = 0;
    virtual std::string content() = 0;
    virtual void close() = 0;
    
    virtual std::string evaluate(const std::string& script) = 0;
};

}
