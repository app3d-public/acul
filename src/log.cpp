#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cstdarg>
#include <ctime>

namespace acul::log
{
    namespace detail
    {
        struct log_ctx g_log_ctx{nullptr, nullptr};
    }

    void time_handler::handle(level level, const char *message, stringstream &ss) const
    {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto now_ns = now.time_since_epoch();
        long long ns = duration_cast<nanoseconds>(now_ns).count() % 1000000000;

        time_t time_t_now = system_clock::to_time_t(now);
        std::tm tm_now;

#ifdef _WIN32
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif

        string time = acul::format("%04d-%02d-%02d %02d:%02d:%02d.%09lld", tm_now.tm_year + 1900, tm_now.tm_mon + 1,
                                   tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, ns);
        ss << time.c_str();
    }

    void level_name_handler::handle(level level, const char *message, stringstream &ss) const
    {
        switch (level)
        {
            case level::info:
                ss << "info";
                break;
            case level::debug:
                ss << "debug";
                break;
            case level::trace:
                ss << "trace";
                break;
            case level::warn:
                ss << "warn";
                break;
            case level::error:
                ss << "error";
                break;
            case level::fatal:
                ss << "fatal";
                break;
            default:
                ss << "unknown";
                break;
        }
    }

    void color_handler::handle(level level, const char *message, stringstream &ss) const
    {
        switch (level)
        {
            case level::fatal:
                ss << colors::magenta;
                break;
            case level::error:
                ss << colors::red;
                break;
            case level::warn:
                ss << colors::yellow;
                break;
            case level::info:
                ss << colors::green;
                break;
            case level::debug:
                ss << colors::blue;
                break;
            case level::trace:
                ss << colors::cyan;
                break;
            default:
                ss << colors::reset;
                break;
        }
    };

    void logger_base::set_pattern(const string &pattern)
    {
        _tokens->clear();

        const char *p = pattern.data();
        const char *end = p + pattern.size();
        const char *begin = p;
        while (p < end)
        {
            if (p + 1 < end && p[0] == '%' && p[1] == '(')
            {
                if (p > begin) _tokens->push_back(make_shared<text_handler>(string(begin, size_t(p - begin))));
                const char *tok_begin = p + 2;
                const void *close_v = memchr(tok_begin, ')', size_t(end - tok_begin));
                if (!close_v)
                {
                    p += 2;
                    begin = p - 2;
                    continue;
                }

                const char *tok_end = static_cast<const char *>(close_v);
                string token = strip_controls(string(tok_begin, size_t(tok_end - tok_begin)));
                if (token == "ascii_time") _tokens->push_back(make_shared<time_handler>());
                else if (token == "level_name") _tokens->push_back(make_shared<level_name_handler>());
                else if (token == "thread") _tokens->push_back(make_shared<thread_id_handler>());
                else if (token == "message") _tokens->push_back(make_shared<message_handler>());
                else if (token == "color_auto") _tokens->push_back(make_shared<color_handler>());
                else if (token == "color_off") _tokens->push_back(make_shared<decolor_handler>());
                p = tok_end + 1;
                begin = p;
                continue;
            }
            ++p;
        }

        if (begin < end) _tokens->push_back(make_shared<text_handler>(string(begin, size_t(end - begin))));
    }

    std::chrono::steady_clock::time_point log_service::dispatch()
    {
        while (true)
        {
            pair<logger_base *, string> pair;
            if (_queue.try_pop(pair))
            {
                try
                {
                    pair.first->write(pair.second);
                }
                catch (...)
                {
                    _count.fetch_sub(1, std::memory_order_release);
                    throw;
                }
                _count.fetch_sub(1, std::memory_order_release);
            }
            else return std::chrono::steady_clock::time_point::max();
        }
    }

    void log_service::log(logger_base *logger, enum level level, const char *message, ...)
    {
        if (!logger || !message || level > this->level) return;
        va_list args;
        va_start(args, message);
        try
        {
            vlog(logger, level, message, args);
        }
        catch (...)
        {
            va_end(args);
            throw;
        }
        va_end(args);
    }

    void log_service::vlog(logger_base *logger, enum level level, const char *message, va_list args)
    {
        if (!logger || !message || level > this->level) return;
        stringstream ss;
        va_list copy;
        va_copy(copy, args);
        string formatted;
        try
        {
            logger->parse_tokens(level, message, ss);
            string parsed = ss.str();
            formatted = acul::format_va_list(parsed.c_str(), copy);
        }
        catch (...)
        {
            va_end(copy);
            throw;
        }
        va_end(copy);

        _count.fetch_add(1, std::memory_order_relaxed);
        try
        {
            _queue.emplace(logger, std::move(formatted));
        }
        catch (...)
        {
            _count.fetch_sub(1, std::memory_order_release);
            throw;
        }
        notify();
    }

    void log_service::await(bool force)
    {
        if (force)
        {
            pair<logger_base *, string> discarded;
            while (_queue.try_pop(discarded)) _count.fetch_sub(1, std::memory_order_release);
        }
        while (_count.load(std::memory_order_acquire) > 0) std::this_thread::yield();
    }

    void log_service::remove_logger(const string &name)
    {
        auto it = _loggers.find(name);
        if (it == _loggers.end()) return;

        // Queued records keep a raw logger pointer, so the queue must no longer
        // reference it before the logger is destroyed.
        await();
        if (detail::g_log_ctx.default_logger == it->second) detail::g_log_ctx.default_logger = nullptr;
        acul::release(it->second);
        _loggers.erase(it);
    }

    void write(log_service *log_service, logger_base *logger, enum level level, const char *message, ...)
    {
        if (!log_service || !logger) return;
        va_list args;
        va_start(args, message);
        try
        {
            log_service->vlog(logger, level, message, args);
        }
        catch (...)
        {
            va_end(args);
            throw;
        }
        va_end(args);
    }

    log_service::~log_service()
    {
        // service_dispatch stops and joins its worker before releasing services.
        // Flush anything that was queued just before shutdown while logger
        // instances are still alive.
        try
        {
            dispatch();
        }
        catch (...)
        {
            pair<logger_base *, string> discarded;
            while (_queue.try_pop(discarded)) _count.fetch_sub(1, std::memory_order_relaxed);
        }
        for (auto &logger : _loggers)
        {
            if (detail::g_log_ctx.default_logger == logger.second) detail::g_log_ctx.default_logger = nullptr;
            acul::release(logger.second);
        }
        _loggers.clear();
        if (detail::g_log_ctx.log_service == this)
        {
            detail::g_log_ctx.default_logger = nullptr;
            detail::g_log_ctx.log_service = nullptr;
        }
    }
} // namespace acul::log
