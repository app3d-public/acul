#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cstdarg>
#include <ctime>

namespace acul
{
    namespace log
    {
        namespace internal
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
                    ss << "INFO";
                    break;
                case level::debug:
                    ss << "DEBUG";
                    break;
                case level::trace:
                    ss << "TRACE";
                    break;
                case level::warn:
                    ss << "WARN";
                    break;
                case level::error:
                    ss << "ERROR";
                    break;
                case level::fatal:
                    ss << "FATAL";
                    break;
                default:
                    ss << "UNKNOWN";
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
            static acul::hashmap<string, shared_ptr<token_handler_base>> token_handlers = {
                {"ascii_time", make_shared<time_handler>()},  {"level_name", make_shared<level_name_handler>()},
                {"thread", make_shared<thread_id_handler>()}, {"message", make_shared<message_handler>()},
                {"color_auto", make_shared<color_handler>()}, {"color_off", make_shared<decolor_handler>()}};

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
                    auto it = token_handlers.find(token);
                    if (it != token_handlers.end()) _tokens->push_back(it->second);
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
                    pair.first->write(pair.second);
                    _count.fetch_sub(1, std::memory_order_relaxed);
                }
                else return std::chrono::steady_clock::time_point::max();
            }
        }

        void log_service::log(logger_base *logger, enum level level, const char *message, ...)
        {
            if (level > this->level) return;
            stringstream ss;
            logger->parse_tokens(level, message, ss);
            va_list args;
            va_start(args, message);
            _count.fetch_add(1, std::memory_order_relaxed);
            string parsed = ss.str();
            _queue.emplace(logger, acul::format_va_list(parsed.c_str(), args));
            va_end(args);
            notify();
        }

        log_service::~log_service()
        {
            for (auto &logger : _loggers) acul::release(logger.second);
            _loggers.clear();
            internal::g_log_ctx.log_service = nullptr;
        }
    } // namespace log
} // namespace acul