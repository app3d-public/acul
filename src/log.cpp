#include <core/log.hpp>
#include <core/std/string.hpp>
#include <cstdarg>
#include <ctime>
#include <regex>
#include <thread>

namespace logging
{
    void TimeTokenHandler::handle(Level level, const char *message, std::stringstream &ss) const
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

        ss << astl::format("%04d-%02d-%02d %02d:%02d:%02d.%09lld", tm_now.tm_year + 1900, tm_now.tm_mon + 1,
                           tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec, ns);
    }

    void LevelNameTokenHandler::handle(Level level, const char *message, std::stringstream &ss) const
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

    void ColorizeTokenHandler::handle(Level level, const char *message, std::stringstream &ss) const
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

    void Logger::setPattern(const std::string &pattern)
    {
        _tokens->clear();
        std::regex token_regex("%\\((.*?)\\)");
        std::sregex_iterator it(pattern.begin(), pattern.end(), token_regex);
        std::sregex_iterator end;

        size_t last_pos = 0;
        for (; it != end; ++it)
        {
            std::size_t pos = it->position();
            if (pos != last_pos)
            {
                std::string text = pattern.substr(last_pos, pos - last_pos);
                _tokens->push_back(std::make_shared<TextTokenHandler>(text));
            }

            astl::hashmap<std::string, std::shared_ptr<TokenHandler>> tokenHandlers = {
                {"ascii_time", std::make_shared<TimeTokenHandler>()},
                {"level_name", std::make_shared<LevelNameTokenHandler>()},
                {"thread", std::make_shared<ThreadIdTokenHandler>()},
                {"message", std::make_shared<MessageTokenHandler>()},
                {"color_auto", std::make_shared<ColorizeTokenHandler>()},
                {"color_off", std::make_shared<DecolorizeTokenHandler>()}};

            std::string token = it->str(1);
            auto handlerIter = tokenHandlers.find(token);
            if (handlerIter != tokenHandlers.end()) _tokens->push_back(handlerIter->second);

            last_pos = pos + it->length();
        }
        if (last_pos != pattern.length())
        {
            std::string text = pattern.substr(last_pos);
            _tokens->push_back(std::make_shared<TextTokenHandler>(text));
        }
    }

    LogManager *mng{nullptr};

    void LogManager::init()
    {
        mng = new LogManager();
        mng->_taskThread = std::thread(&LogManager::workerThread, mng);
    }

    void LogManager::destroy()
    {
        {
            std::unique_lock<std::mutex> lock(mng->_queueMutex);
            mng->_running = false;
            mng->_queueChanged.notify_one();
        }
        mng->_taskThread.join();
        delete mng;
        mng = nullptr;
    }

    void LogManager::workerThread()
    {
        while (_running)
        {
            std::pair<std::shared_ptr<Logger>, std::string> pair;
            if (_logQueue.try_pop(pair)) { pair.first->write(pair.second); }
            else
            {
                std::unique_lock<std::mutex> lock(_queueMutex);
                _queueChanged.wait(lock, [this]() { return !_logQueue.empty() || !_running; });
            }
        }
    }

    std::shared_ptr<Logger> LogManager::getLogger(const std::string &name) const
    {
        auto it = _loggers.find(name);
        return it == _loggers.end() ? nullptr : it->second;
    }

    void LogManager::removeLogger(const std::string &name) { _loggers.erase(name); }

    void LogManager::log(const std::shared_ptr<Logger> &logger, Level level, const char *message, ...)
    {
        if (level > _level) return;
        std::stringstream ss;
        logger->parseTokens(level, message, ss);
        va_list args;
        va_start(args, message);
        _logQueue.push({logger, astl::format_va_list(ss.str().c_str(), args)});
        va_end(args);
        _queueChanged.notify_one();
    }
} // namespace logging