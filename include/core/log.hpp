#ifndef APP_CORE_LOG_H
#define APP_CORE_LOG_H

#include <filesystem>
#include <fstream>
#include <iostream>
#include "task.hpp"

namespace logging
{
    enum class Level
    {
        Fatal,
        Error,
        Warn,
        Info,
        Debug,
        Trace
    };

    class TokenHandler
    {
    public:
        virtual ~TokenHandler() = default;
        virtual void handle(Level level, const char *message, std::stringstream &ss) const = 0;
    };

    using TokenHandlerList = astl::vector<astl::shared_ptr<TokenHandler>>;

    class TextTokenHandler final : public TokenHandler
    {
    public:
        explicit TextTokenHandler(const std::string_view &text) : _text(text) {}

        void handle(Level level, const char *message, std::stringstream &ss) const override { ss << _text; }

    private:
        const std::string _text;
    };

    class TimeTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override;
    };

    class ThreadIdTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override
        {
            ss << std::this_thread::get_id();
        }
    };

    class LevelNameTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override;
    };

    class MessageTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override { ss << message; }
    };

    namespace colors
    {
        constexpr std::string_view red = "\x1b[31m";
        constexpr std::string_view green = "\x1b[32m";
        constexpr std::string_view yellow = "\x1b[33m";
        constexpr std::string_view blue = "\x1b[34m";
        constexpr std::string_view magenta = "\x1b[35m";
        constexpr std::string_view cyan = "\x1b[36m";
        constexpr std::string_view reset = "\x1b[0m";
    }; // namespace colors

    class ColorizeTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override;
    };

    class DecolorizeTokenHandler final : public TokenHandler
    {
    public:
        void handle(Level level, const char *message, std::stringstream &ss) const override { ss << colors::reset; }
    };

    class APPLIB_API Logger
    {
    public:
        Logger(const std::string &name) : _name(name), _tokens(astl::make_shared<TokenHandlerList>()) {}

        virtual ~Logger() = default;

        void setPattern(const std::string &pattern);

        std::string name() const { return _name; }

        virtual std::ostream &stream() = 0;

        virtual void write(const std::string &message) = 0;

        void parseTokens(Level level, const char *message, std::stringstream &ss)
        {
            for (auto &token : *_tokens) token->handle(level, message, ss);
        }

    private:
        std::string _name;
        astl::shared_ptr<TokenHandlerList> _tokens;
    };

    class APPLIB_API FileLogger final : public Logger
    {
    public:
        FileLogger(const std::string &name, const std::filesystem::path &path, std::ios_base::openmode flags)
            : Logger(name), _path(path)
        {
            _fs.open(path, flags);
        }

        ~FileLogger()
        {
            if (_fs.is_open()) _fs.close();
        }

        std::ostream &stream() override { return _fs; }

        virtual void write(const std::string &message) override
        {
            if (_fs.is_open()) _fs << message;
        }

    private:
        std::filesystem::path _path;
        std::ofstream _fs;
    };

    class ConsoleLogger final : public Logger
    {
    public:
        explicit ConsoleLogger(const std::string &name) : Logger(name) {}

        std::ostream &stream() override { return std::cout; }

        virtual void write(const std::string &message) override { std::cout << message; }
    };

    /**
     * @class The Log Service
     * @brief Manages loggers for the application.
     *
     * Provides functionality to add, get, and remove loggers. It also allows logging messages with
     * different log levels.
     */
    class APPLIB_API LogService final : public task::IService
    {
    public:
        ~LogService();

        /**
         * @brief Adds a logger with the specified name and file path.
         * @param name The name of the logger.
         * @param path The file path where the log file should be created.
         * @param flags The flags to open the log file with.
         * @return A pointer to the added Logger object.
         */
        template <typename T, typename... Args>
        T *addLogger(const std::string &name, Args &&...args)
        {
            auto *logger = astl::alloc<T>(name, std::forward<Args>(args)...);
            _loggers[name] = logger;
            return logger;
        }

        /**
         * @brief Gets the logger with the specified name.
         * @param name The name of the logger to retrieve.
         * @return A pointer to the Logger object, or nullptr if the logger was not found.
         */
        Logger *getLogger(const std::string &name) const
        {
            auto it = _loggers.find(name);
            return it == _loggers.end() ? nullptr : it->second;
        }

        /**
         * @brief Removes the logger with the specified name.
         * @param name The name of the logger to remove.
         */
        void removeLogger(const std::string &name)
        {
            auto it = _loggers.find(name);
            if (it == _loggers.end()) return;
            astl::release(it->second);
            _loggers.erase(it);
        }

        __attribute__((format(printf, 4, 5))) APPLIB_API void log(Logger *logger, Level level, const char *message,
                                                                  ...);

        Level level() const { return _level; }

        void level(Level level) { _level = level; }

        virtual std::chrono::steady_clock::time_point dispatch() override;

        virtual void await(bool force = false) override
        {
            if (force)
            {
                _queue.clear();
                return;
            }
            while (_count.load(std::memory_order_relaxed) > 0) std::this_thread::yield();
        }

    private:
        Level _level{Level::Error};
        astl::hashmap<std::string, Logger *> _loggers;
        oneapi::tbb::concurrent_queue<std::pair<Logger *, std::string>> _queue;
        std::atomic<int> _count{0};
    };

    extern APPLIB_API LogService *g_LogService;
    extern APPLIB_API Logger *g_DefaultLogger;

    inline Logger *getLogger(const std::string &name) { return g_LogService->getLogger(name); }

    inline LogService::~LogService()
    {
        for (auto &logger : _loggers) astl::release(logger.second);
        _loggers.clear();
        g_LogService = nullptr;
        g_DefaultLogger = nullptr;
    }
} // namespace logging

#define logInfo(...)  logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Info, __VA_ARGS__)
#define logDebug(...) logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Debug, __VA_ARGS__)
#define logTrace(...) logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Trace, __VA_ARGS__)
#define logWarn(...)  logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Warn, __VA_ARGS__)
#define logError(...) logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Error, __VA_ARGS__)
#define logFatal(...) logging::g_LogService->log(logging::g_DefaultLogger, logging::Level::Fatal, __VA_ARGS__)

#endif