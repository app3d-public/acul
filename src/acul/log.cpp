#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cstdarg>
#include <ctime>
#include <regex>

namespace acul
{
    namespace log
    {
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
            _tokens->clear();
            std::regex token_regex("%\\((.*?)\\)");
            const char *begin = pattern.begin();
            const char *end = pattern.begin() + pattern.size();
            std::cregex_iterator it(begin, end, token_regex);
            std::cregex_iterator regex_end;

            size_t last_pos = 0;
            for (; it != regex_end; ++it)
            {
                std::size_t pos = it->position();
                if (pos != last_pos)
                {
                    string text = pattern.substr(last_pos, pos - last_pos);
                    _tokens->push_back(acul::make_shared<text_handler>(text));
                }

                acul::hashmap<string, acul::shared_ptr<token_handler_base>> tokenHandlers = {
                    {"ascii_time", acul::make_shared<time_handler>()},
                    {"level_name", acul::make_shared<level_name_handler>()},
                    {"thread", acul::make_shared<thread_id_handler>()},
                    {"message", acul::make_shared<message_handler>()},
                    {"color_auto", acul::make_shared<color_handler>()},
                    {"color_off", acul::make_shared<decolor_handler>()}};

                string token = it->str(1).c_str();
                auto handlerIter = tokenHandlers.find(token);
                if (handlerIter != tokenHandlers.end()) _tokens->push_back(handlerIter->second);

                last_pos = pos + it->length();
            }
            if (last_pos != pattern.size())
            {
                string text = pattern.substr(last_pos);
                _tokens->push_back(acul::make_shared<text_handler>(text));
            }
        }

        log_service *g_log_service{nullptr};
        logger_base *g_default_logger{nullptr};

        std::chrono::steady_clock::time_point log_service::dispatch()
        {
            while (true)
            {
                std::pair<logger_base *, string> pair;
                if (_queue.try_pop(pair))
                {
                    pair.first->write(pair.second);
                    _count.fetch_sub(1, std::memory_order_relaxed);
                }
                else
                    return std::chrono::steady_clock::time_point::max();
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
            _queue.emplace(logger, acul::format_va_list(ss.str().c_str(), args));
            va_end(args);
            notify();
        }

        log_service::~log_service()
        {
            for (auto &logger : _loggers) acul::release(logger.second);
            _loggers.clear();
            g_log_service = nullptr;
            g_default_logger = nullptr;
        }

        log_service *log_service::instance = nullptr;
    } // namespace log
} // namespace acul