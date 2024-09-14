#ifndef APP_CORE_LOG_H
#define APP_CORE_LOG_H

#include <core/api.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <oneapi/tbb/concurrent_queue.h>
#include <string>
#include <string_view>
#include "std/basic_types.hpp"
#include "std/vector.hpp"

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

    using TokenHandlerList = astl::vector<std::shared_ptr<TokenHandler>>;

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
        Logger(const std::string &name) : _name(name), _tokens(std::make_shared<TokenHandlerList>()) {}

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
        std::shared_ptr<TokenHandlerList> _tokens;
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

        void write(const std::string &message) override
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

        void write(const std::string &message) override { std::cout << message; }
    };

    /**
     * @class LogManager
     * @brief Manages loggers for the application.
     *
     * The LogManager class provides functionality to add, get, and remove loggers. It also allows logging messages with
     * different log levels. The LogManager is a singleton class, meaning only one instance of it can exist in the
     * application.
     */
    class APPLIB_API LogManager
    {
    public:
        static void init();
        /**
         * @brief Adds a logger with the specified name and file path.
         * @param name The name of the logger.
         * @param path The file path where the log file should be created.
         * @param flags The flags to open the log file with.
         * @return A shared pointer to the added Logger object.
         */
        template <typename T, typename... Args>
        std::shared_ptr<T> addLogger(const std::string &name, Args &&...args)
        {
            std::shared_ptr<T> logger = std::make_shared<T>(name, std::forward<Args>(args)...);
            _loggers[name] = logger;
            return logger;
        }

        /**
         * @brief Gets the logger with the specified name.
         * @param name The name of the logger to retrieve.
         * @return A shared pointer to the Logger object, or nullptr if the logger was not found.
         */
        std::shared_ptr<Logger> getLogger(const std::string &name) const;

        /**
         * @brief Removes the logger with the specified name.
         * @param name The name of the logger to remove.
         */
        void removeLogger(const std::string &name);

        __attribute__((format(printf, 4, 5))) APPLIB_API void log(const std::shared_ptr<Logger> &logger, Level level,
                                                                  const char *message, ...);

        std::shared_ptr<Logger> defaultLogger() const { return _defaultLogger; }
        void defaultLogger(std::shared_ptr<Logger> logger) { _defaultLogger = logger; }

        Level level() const { return _level; }

        void level(Level level) { _level = level; }

        /// \brief Perform processing in the worker thread.
        void workerThread();

        /// \brief Stop the worker threads for task processing.
        void stopWorkerThread();

        void await()
        {
            while (!_logQueue.empty()) std::this_thread::yield();
        }

        static void destroy();

    private:
        Level _level{Level::Error};
        astl::hashmap<std::string, std::shared_ptr<Logger>> _loggers;
        std::shared_ptr<Logger> _defaultLogger;
        bool _running{true};
        std::thread _taskThread;
        oneapi::tbb::concurrent_queue<std::pair<std::shared_ptr<Logger>, std::string>> _logQueue;
        std::condition_variable _queueChanged;
        std::mutex _queueMutex;
    };

    extern APPLIB_API LogManager *mng;

    inline std::shared_ptr<Logger> getLogger(const std::string &name) { return mng->getLogger(name); }
} // namespace logging

#define logInfo(...)  logging::mng->log(logging::mng->defaultLogger(), logging::Level::Info, __VA_ARGS__)
#define logDebug(...) logging::mng->log(logging::mng->defaultLogger(), logging::Level::Debug, __VA_ARGS__)
#define logTrace(...) logging::mng->log(logging::mng->defaultLogger(), logging::Level::Trace, __VA_ARGS__)
#define logWarn(...)  logging::mng->log(logging::mng->defaultLogger(), logging::Level::Warn, __VA_ARGS__)
#define logError(...) logging::mng->log(logging::mng->defaultLogger(), logging::Level::Error, __VA_ARGS__)
#define logFatal(...) logging::mng->log(logging::mng->defaultLogger(), logging::Level::Fatal, __VA_ARGS__)

#endif