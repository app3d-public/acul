#include <acul/log.hpp>
#include <acul/string/utils.hpp>
#include <cstdarg>
#include <ctime>
#include <regex>

namespace acul
{
    namespace log
    {
        void TimeTokenHandler::handle(Level level, const char *message, stringstream &ss) const
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

        void LevelNameTokenHandler::handle(Level level, const char *message, stringstream &ss) const
        {
            switch (level)
            {
                case Level::Info:
                    ss << "INFO";
                    break;
                case Level::Debug:
                    ss << "DEBUG";
                    break;
                case Level::Trace:
                    ss << "TRACE";
                    break;
                case Level::Warn:
                    ss << "WARN";
                    break;
                case Level::Error:
                    ss << "ERROR";
                    break;
                case Level::Fatal:
                    ss << "FATAL";
                    break;
                default:
                    ss << "UNKNOWN";
                    break;
            }
        }

        void ColorizeTokenHandler::handle(Level level, const char *message, stringstream &ss) const
        {
            switch (level)
            {
                case Level::Fatal:
                    ss << colors::magenta;
                    break;
                case Level::Error:
                    ss << colors::red;
                    break;
                case Level::Warn:
                    ss << colors::yellow;
                    break;
                case Level::Info:
                    ss << colors::green;
                    break;
                case Level::Debug:
                    ss << colors::blue;
                    break;
                case Level::Trace:
                    ss << colors::cyan;
                    break;
                default:
                    ss << colors::reset;
                    break;
            }
        };

        void Logger::setPattern(const string &pattern)
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
                    _tokens->push_back(acul::make_shared<TextTokenHandler>(text));
                }

                acul::hashmap<string, acul::shared_ptr<TokenHandler>> tokenHandlers = {
                    {"ascii_time", acul::make_shared<TimeTokenHandler>()},
                    {"level_name", acul::make_shared<LevelNameTokenHandler>()},
                    {"thread", acul::make_shared<ThreadIdTokenHandler>()},
                    {"message", acul::make_shared<MessageTokenHandler>()},
                    {"color_auto", acul::make_shared<ColorizeTokenHandler>()},
                    {"color_off", acul::make_shared<DecolorizeTokenHandler>()}};

                string token = it->str(1).c_str();
                auto handlerIter = tokenHandlers.find(token);
                if (handlerIter != tokenHandlers.end()) _tokens->push_back(handlerIter->second);

                last_pos = pos + it->length();
            }
            if (last_pos != pattern.size())
            {
                string text = pattern.substr(last_pos);
                _tokens->push_back(acul::make_shared<TextTokenHandler>(text));
            }
        }

        LogService *g_LogService{nullptr};
        Logger *g_DefaultLogger{nullptr};

        std::chrono::steady_clock::time_point LogService::dispatch()
        {
            while (true)
            {
                std::pair<Logger *, string> pair;
                if (_queue.try_pop(pair))
                {
                    pair.first->write(pair.second);
                    _count.fetch_sub(1, std::memory_order_relaxed);
                }
                else
                    return std::chrono::steady_clock::time_point::max();
            }
        }

        void LogService::log(Logger *logger, Level level, const char *message, ...)
        {
            if (level > _level) return;
            stringstream ss;
            logger->parseTokens(level, message, ss);
            va_list args;
            va_start(args, message);
            _count.fetch_add(1, std::memory_order_relaxed);
            _queue.emplace(logger, acul::format_va_list(ss.str().c_str(), args));
            va_end(args);
            notify();
        }

        LogService::~LogService()
        {
            for (auto &logger : _loggers) acul::release(logger.second);
            _loggers.clear();
            g_LogService = nullptr;
            g_DefaultLogger = nullptr;
        }
    } // namespace log
} // namespace acul