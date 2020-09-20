//Copyright (c) 2020 Ultimaker B.V.
//CuraEngine is released under the terms of the AGPLv3 or higher.

#ifndef NDEBUG
#ifndef UTILS_SVGLOG_H
#define UTILS_SVGLOG_H

#include <iostream>
#include <list>
#include <memory>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include <clipper.hpp>

namespace cura
{
namespace svglog
{
template <typename T>
struct is_stl_const_container_like
{
    typedef typename std::remove_const<T>::type test_type;

    template <typename A>
    static constexpr bool test(A const* cpt = nullptr, decltype(cpt->begin())* = nullptr,
                               decltype(cpt->end())* = nullptr, typename A::const_iterator* pci = nullptr,
                               typename A::value_type* pv = nullptr)
    {
        typedef typename A::const_iterator const_iterator;
        typedef typename A::value_type value_type;
        return std::is_same<decltype(cpt->begin()), const_iterator>::value
               && std::is_same<decltype(cpt->end()), const_iterator>::value
               && std::is_same<decltype(**pci), value_type const&>::value;
    }

    template<typename A>
    static constexpr bool test(...) {
        return false;
    }

    static const bool value = test<test_type>(nullptr);

};

template <typename T>
struct is_stl_container_like
{
    typedef typename std::remove_const<T>::type test_type;

    template <typename A>
    static constexpr bool test(A* pt, A const* cpt = nullptr, decltype(pt->begin())* = nullptr,
                               decltype(pt->end())* = nullptr, decltype(cpt->begin())* = nullptr,
                               decltype(cpt->end())* = nullptr, typename A::iterator* pi = nullptr,
                               typename A::const_iterator* pci = nullptr, typename A::value_type* pv = nullptr)
    {
        typedef typename A::iterator iterator;
        typedef typename A::const_iterator const_iterator;
        typedef typename A::value_type value_type;
        return std::is_same<decltype(pt->begin()), iterator>::value
               && std::is_same<decltype(pt->end()), iterator>::value
               && std::is_same<decltype(cpt->begin()), const_iterator>::value
               && std::is_same<decltype(cpt->end()), const_iterator>::value
               && std::is_same<decltype(**pi), value_type&>::value
               && std::is_same<decltype(**pci), value_type const&>::value;
    }

    template<typename A>
    static constexpr bool test(...) {
        return false;
    }

    static const bool value = test<test_type>(nullptr);

};

class SVGsink
{
public:
    SVGsink();


    template <typename T>
    void log(const T& geometry)
    {
        if (is_stl_const_container_like<T>::value)
        {
            for (const auto& item : geometry)
            {
                log(item);
            }
        }
        else
        {
            buffer << geometry.X << ", " << geometry.Y << "\n";
        }
    }

private:

    std::stringstream buffer;
};

class SVGlog
{
public:
    SVGlog() = default;

    ~SVGlog();

    void registerSink(const std::string& sink);

    template <typename... Args>
    std::shared_ptr<SVGsink> log(const std::string& sink, const Args&... args)
    {
        auto ret_sink = sinks.at(sink);
        // Todo: write the different args to the sink
        return ret_sink;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<SVGsink>> sinks;

};


static std::shared_ptr<SVGlog> default_logger = std::make_shared<SVGlog>();


template <typename... Args>
std::shared_ptr<SVGsink> log(const std::string& sink, const Args&... args)
{
    return default_logger->log(sink, args...);
}

template <typename T>
void operator<<(std::shared_ptr<SVGsink> logger, const T& geometry)
{
    logger->log(geometry);
}

}
}
#endif // UTILS_SVGLOG_H
#endif // NDEBUG
