//
// Application.h
//
// Library: Util
// Package: Application
// Module:  Application
//
// Definition of the Application class.
//
// Copyright (c) 2004-2006, Applied Informatics Software Engineering GmbH.
// and Contributors.
//
// SPDX-License-Identifier:	BSL-1.0
//


#ifndef Util_Application_INCLUDED
#define Util_Application_INCLUDED


#include "Poco/AutoPtr.h"
#include "Poco/Logger.h"
#include "Poco/Path.h"
#include "Poco/Timespan.h"
#include "Poco/Timestamp.h"
#include "Poco/Util/LayeredConfiguration.h"
#include "Poco/Util/OptionSet.h"
#include "Poco/Util/Subsystem.h"
#include "Poco/Util/Util.h"
#include <typeinfo>
#include <vector>


namespace Poco
{
namespace Util
{


    class OptionSet;


    class Util_API Application : public Subsystem
    /// The Application class implements the main subsystem
    /// in a process. The application class is responsible for
    /// initializing all its subsystems.
    ///  Application类实现了进程中的主子系统。Application类负责初始化所有其子系统。
    ///
    /// Subclasses can and should override the following virtual methods:
    ///   - initialize() (the one-argument, protected variant)
    ///   - uninitialize()
    ///   - reinitialize()
    ///   - defineOptions()
    ///   - handleOption()
    ///   - main()
    /// 
    /// 子类可以并且应该重写以下虚拟方法：
    ///   - initialize() (单参数，受保护的变体)
    ///   - uninitialize()
    ///   - reinitialize()
    ///   - defineOptions()
    ///   - handleOption()
    ///   - main()
    ///
    /// The application's main logic should be implemented in
    /// the main() method.
    /// 应用程序的主逻辑应该在main()方法中实现。
    ///
    /// There may be at most one instance of the Application class
    /// in a process.
    /// 在进程中，最多只能有一个Application类的实例。
    ///
    /// The Application class maintains a LayeredConfiguration (available
    /// via the config() member function) consisting of:
    ///   - a MapConfiguration (priority -100) storing application-specific
    ///     properties, as well as properties from bound command line arguments.
    ///   - a SystemConfiguration (priority 100)
    ///   - the configurations loaded with loadConfiguration().
    /// 
    /// Application类维护一个LayeredConfiguration(通过config()成员函数可用)，由以下部分组成：
    ///   - 一个MapConfiguration(优先级-100)，存储应用程序特定的属性，以及从绑定命令行参数的属性。
    ///   - 一个SystemConfiguration(优先级100)
    ///   - 使用loadConfiguration()加载的配置。
    ///
    /// The Application class sets a few default properties in
    /// its configuration. These are:
    ///   - application.path: the absolute path to application executable
    ///   - application.name: the file name of the application executable
    ///   - application.baseName: the file name (excluding extension) of the application executable
    ///   - application.dir: the path to the directory where the application executable resides
    ///   - application.configDir: the path to the directory where user specific configuration files of the application should be stored.
    ///   - application.cacheDir: the path to the directory where user specific non-essential data files of the application should be stored.
    ///   - application.dataDir: the path to the directory where user specific data files of the application should be stored.
    ///   - application.tempDir: the path to the directory where user specific temporary files and other file objects of the application should be stored.
    /// 
    /// Application类在配置中设置了一些默认属性。这些属性是：
    ///   - application.path: 应用程序可执行文件的绝对路径
    ///   - application.name: 应用程序可执行文件的文件名
    ///   - application.baseName: 应用程序可执行文件的文件名(不包括扩展名)
    ///   - application.dir: 应用程序可执行文件所在的目录
    ///   - application.configDir: 用户特定配置文件的目录
    ///   - application.cacheDir: 用户特定非重要数据文件的目录
    ///   - application.dataDir: 用户特定数据文件的目录
    ///   - application.tempDir: 用户特定临时文件和其他文件对象的目录
    ///
    /// If loadConfiguration() has never been called, application.configDir will be equal to application.dir.
    /// 如果loadConfiguration()从未被调用，application.configDir将等于application.dir。
    ///
    /// The POCO_APP_MAIN macro can be used to implement main(argc, argv).
    /// POCO_APP_MAIN宏可以用于实现main(argc, argv)。
    ///
    /// If POCO has been built with POCO_WIN32_UTF8, POCO_APP_MAIN supports
    /// Unicode command line arguments.
    /// 如果POCO使用POCO_WIN32_UTF8构建，POCO_APP_MAIN支持Unicode命令行参数。
    {
    public:
        typedef std::vector<std::string> ArgVec;
        typedef Poco::AutoPtr<Subsystem> SubsystemPtr;
        typedef std::vector<SubsystemPtr> SubsystemVec;

        enum ExitCode
        /// Commonly used exit status codes.
        /// Based on the definitions in the 4.3BSD <sysexits.h> header file.
        /// 
        /// 常用的退出状态码。基于4.3BSD <sysexits.h>头文件中的定义。
        {
            EXIT_OK = 0, /// successful termination
            EXIT_USAGE = 64, /// command line usage error
            EXIT_DATAERR = 65, /// data format error
            EXIT_NOINPUT = 66, /// cannot open input
            EXIT_NOUSER = 67, /// addressee unknown
            EXIT_NOHOST = 68, /// host name unknown
            EXIT_UNAVAILABLE = 69, /// service unavailable
            EXIT_SOFTWARE = 70, /// internal software error
            EXIT_OSERR = 71, /// system error (e.g., can't fork)
            EXIT_OSFILE = 72, /// critical OS file missing
            EXIT_CANTCREAT = 73, /// can't create (user) output file
            EXIT_IOERR = 74, /// input/output error
            EXIT_TEMPFAIL = 75, /// temp failure; user is invited to retry
            EXIT_PROTOCOL = 76, /// remote error in protocol
            EXIT_NOPERM = 77, /// permission denied
            EXIT_CONFIG = 78 /// configuration error
        };

        enum ConfigPriority
        /// 配置优先级。
        {
            PRIO_APPLICATION = -100,
            PRIO_DEFAULT = 0,
            PRIO_SYSTEM = 100
        };

        Application();
        /// Creates the Application.

        Application(int argc, char * argv[]);
        /// Creates the Application and calls init(argc, argv).

        void addSubsystem(Subsystem * pSubsystem);
        /// Adds a new subsystem to the application. The
        /// application immediately takes ownership of it, so that a
        /// call in the form
        ///     Application::instance().addSubsystem(new MySubsystem);
        /// is okay.

        void init(int argc, char * argv[]);
        /// Processes the application's command line arguments
        /// and sets the application's properties (e.g.,
        /// "application.path", "application.name", etc.).
        ///
        /// Note that as of release 1.3.7, init() no longer
        /// calls initialize(). This is now called from run().


        void init(const ArgVec & args);
        /// Processes the application's command line arguments
        /// and sets the application's properties (e.g.,
        /// "application.path", "application.name", etc.).
        ///
        /// Note that as of release 1.3.7, init() no longer
        /// calls initialize(). This is now called from run().

        bool initialized() const;
        /// Returns true iff the application is in initialized state
        /// (that means, has been initialized but not yet uninitialized).

        void setUnixOptions(bool flag);
        /// Specify whether command line option handling is Unix-style
        /// (flag == true; default) or Windows/OpenVMS-style (flag == false).
        ///
        /// This member function should be called from the constructor of
        /// a subclass to be effective.

        int loadConfiguration(int priority = PRIO_DEFAULT);
        /// Loads configuration information from a default location.
        ///
        /// The configuration(s) will be added to the application's
        /// LayeredConfiguration with the given priority.
        ///
        /// The configuration file(s) must be located in the same directory
        /// as the executable or a parent directory of it, and must have the
        /// same base name as the executable, with one of the following extensions:
        /// .properties, .ini or .xml.
        ///
        /// The .properties file, if it exists, is loaded first, followed
        /// by the .ini file and the .xml file.
        ///
        /// If the application is built in debug mode (the _DEBUG preprocessor
        /// macro is defined) and the base name of the application executable
        /// ends with a 'd', a config file without the 'd' ending its base name is
        /// also found.
        ///
        /// Example: Given the application "SampleAppd.exe", built in debug mode.
        /// Then loadConfiguration() will automatically find a configuration file
        /// named "SampleApp.properties" if it exists and if "SampleAppd.properties"
        /// cannot be found.
        ///
        /// Returns the number of configuration files loaded, which may be zero.
        ///
        /// This method must not be called before init(argc, argv)
        /// has been called.

        void loadConfiguration(const std::string & path, int priority = PRIO_DEFAULT);
        /// Loads configuration information from the file specified by
        /// the given path. The file type is determined by the file
        /// extension. The following extensions are supported:
        ///   - .properties - properties file (PropertyFileConfiguration)
        ///   - .ini        - initialization file (IniFileConfiguration)
        ///   - .xml        - XML file (XMLConfiguration)
        ///
        /// Extensions are not case sensitive.
        ///
        /// The configuration will be added to the application's
        /// LayeredConfiguration with the given priority.

        template <class C>
        C & getSubsystem() const;
        /// Returns a reference to the subsystem of the class
        /// given as template argument.
        ///
        /// Throws a NotFoundException if such a subsystem has
        /// not been registered.

        SubsystemVec & subsystems();
        /// Returns a reference to the subsystem list

        virtual int run();
        /// Runs the application by performing additional (un)initializations
        /// and calling the main() method.
        /// 
        /// 运行应用程序，通过执行额外的(un)初始化并调用main()方法。
        ///
        /// First calls initialize(), then calls main(), and
        /// finally calls uninitialize(). The latter will be called
        /// even if main() throws an exception. If initialize() throws
        /// an exception, main() will not be called and the exception
        /// will be propagated to the caller. If uninitialize() throws
        /// an exception, the exception will be propagated to the caller.
        ///
        /// 首先调用initialize()，然后调用main()，最后调用uninitialize()。
        /// 如果initialize()抛出异常，main()将不会被调用，异常将传播给调用者。
        /// 如果uninitialize()抛出异常，异常将传播给调用者。
        
        std::string commandName() const;
        /// Returns the command name used to invoke the application.
        /// 
        /// 返回用于调用应用程序的命令名称。

        std::string commandPath() const;
        /// Returns the full command path used to invoke the application.
        ///
        /// 返回用于调用应用程序的完整命令路径。

        LayeredConfiguration & config() const;
        /// Returns the application's configuration.

        Poco::Logger & logger() const;
        /// Returns the application's logger.
        ///
        /// Before the logging subsystem has been initialized, the
        /// application's logger is "ApplicationStartup", which is
        /// connected to a ConsoleChannel.
        ///
        /// After the logging subsystem has been initialized, which
        /// usually happens as the first action in Application::initialize(),
        /// the application's logger is the one specified by the
        /// "application.logger" configuration property. If that property
        /// is not specified, the logger is "Application".

        const ArgVec & argv() const;
        /// Returns reference to vector of the application's arguments as
        /// specified on the command line. If user overrides the
        /// Application::main(const ArgVec&) function, it will receive
        /// only the command line parameters that were not processed in
        /// Application::processOptons(). This function returns the
        /// full set of command line parameters as received in
        /// main(argc, argv*).

        const OptionSet & options() const;
        /// Returns the application's option set.

        static Application & instance();
        /// Returns a reference to the Application singleton.
        ///
        /// Throws a NullPointerException if no Application instance exists.

        static Application * instanceRawPtr();
        /// Returns a raw pointer to the Application singleton.
        ///
        /// The caller should check whether the result is nullptr.

        const Poco::Timestamp & startTime() const;
        /// Returns the application start time (UTC).

        Poco::Timespan uptime() const;
        /// Returns the application uptime.

        void stopOptionsProcessing();
        /// If called from an option callback, stops all further
        /// options processing.
        ///
        /// If called, the following options on the command line
        /// will not be processed, and required options will not
        /// be checked.
        ///
        /// This is useful, for example, if an option for displaying
        /// help information has been encountered and no other things
        /// besides displaying help shall be done.

        const char * name() const;

    protected:
        void initialize(Application & self);
        /// Initializes the application and all registered subsystems.
        /// Subsystems are always initialized in the exact same order
        /// in which they have been registered.
        ///
        /// 初始化应用程序和所有注册的子系统。
        /// 子系统总是以它们注册的精确顺序初始化。
        ///
        /// 重写实现必须调用基类实现。
        ///
        /// Overriding implementations must call the base class implementation.

        void uninitialize();
        /// Uninitializes the application and all registered subsystems.
        /// Subsystems are always uninitialized in reverse order in which
        /// they have been initialized.
        ///
        /// Overriding implementations must call the base class implementation.

        void reinitialize(Application & self);
        /// Re-nitializes the application and all registered subsystems.
        /// Subsystems are always reinitialized in the exact same order
        /// in which they have been registered.
        ///
        /// Overriding implementations must call the base class implementation.

        virtual void defineOptions(OptionSet & options);
        /// Called before command line processing begins.
        /// If a subclass wants to support command line arguments,
        /// it must override this method.
        /// The default implementation does not define any options itself,
        /// but calls defineOptions() on all registered subsystems.
        ///
        /// Overriding implementations should call the base class implementation.

        virtual void handleOption(const std::string & name, const std::string & value);
        /// Called when the option with the given name is encountered
        /// during command line arguments processing.
        ///
        /// The default implementation does option validation, bindings
        /// and callback handling.
        ///
        /// Overriding implementations must call the base class implementation.

        void setLogger(Poco::Logger & logger);
        /// Sets the logger used by the application.

        virtual int main(const std::vector<std::string> & args);
        /// The application's main logic.
        ///
        /// Unprocessed command line arguments are passed in args.
        /// Note that all original command line arguments are available
        /// via the properties application.argc and application.argv[<n>].
        ///
        /// Returns an exit code which should be one of the values
        /// from the ExitCode enumeration.

        bool findFile(Poco::Path & path) const;
        /// Searches for the file in path in the application directory.
        ///
        /// If path is absolute, the method immediately returns true and
        /// leaves path unchanged.
        ///
        /// If path is relative, searches for the file in the application
        /// directory and in all subsequent parent directories.
        /// Returns true and stores the absolute path to the file in
        /// path if the file could be found. Returns false and leaves path
        /// unchanged otherwise.

        void init();
        /// Common initialization code.

        ~Application();
        /// Destroys the Application and deletes all registered subsystems.

    private:
        void setup();
        void setArgs(int argc, char * argv[]);
        void setArgs(const ArgVec & args);
        void getApplicationPath(Poco::Path & path) const;
        void processPocoOptions();
        bool findAppConfigFile(const std::string & appName, const std::string & extension, Poco::Path & path) const;
        bool findAppConfigFile(const Path & basePath, const std::string & appName, const std::string & extension, Poco::Path & path) const;

        typedef Poco::AutoPtr<LayeredConfiguration> ConfigPtr;

        ConfigPtr _pConfig;
        SubsystemVec _subsystems;
        bool _initialized;
        std::string _command;
        ArgVec _argv;
        ArgVec _unprocessedArgs;
        OptionSet _options;
        bool _unixOptions;
        Poco::Logger * _pLogger;
        Poco::Timestamp _startTime;
        bool _stopOptionsProcessing;

#if defined(POCO_OS_FAMILY_UNIX) && !defined(POCO_VXWORKS)
        std::string _workingDirAtLaunch;
#endif

        static Application * _pInstance;

        friend class LoggingSubsystem;

        Application(const Application &);
        Application & operator=(const Application &);
    };


    //
    // inlines
    //
    template <class C>
    C & Application::getSubsystem() const
    {
        for (SubsystemVec::const_iterator it = _subsystems.begin(); it != _subsystems.end(); ++it)
        {
            const Subsystem * pSS(it->get());
            const C * pC = dynamic_cast<const C *>(pSS);
            if (pC)
                return *const_cast<C *>(pC);
        }
        throw Poco::NotFoundException("The subsystem has not been registered", typeid(C).name());
    }

    inline Application::SubsystemVec & Application::subsystems()
    {
        return _subsystems;
    }


    inline bool Application::initialized() const
    {
        return _initialized;
    }


    inline LayeredConfiguration & Application::config() const
    {
        return *const_cast<LayeredConfiguration *>(_pConfig.get());
    }


    inline Poco::Logger & Application::logger() const
    {
        poco_check_ptr(_pLogger);
        return *_pLogger;
    }


    inline const Application::ArgVec & Application::argv() const
    {
        return _argv;
    }


    inline const OptionSet & Application::options() const
    {
        return _options;
    }


    inline Application & Application::instance()
    {
        poco_check_ptr(_pInstance);
        return *_pInstance;
    }


    inline Application * Application::instanceRawPtr()
    {
        return _pInstance;
    }


    inline const Poco::Timestamp & Application::startTime() const
    {
        return _startTime;
    }


    inline Poco::Timespan Application::uptime() const
    {
        Poco::Timestamp now;
        Poco::Timespan uptime = now - _startTime;

        return uptime;
    }


}
} // namespace Poco::Util


//
// Macro to implement main()
//
#    define POCO_APP_MAIN(App) \
        int main(int argc, char ** argv) \
        { \
            Poco::AutoPtr<App> pApp = new App; \
            try \
            { \
                pApp->init(argc, argv); \
            } \
            catch (Poco::Exception & exc) \
            { \
                pApp->logger().log(exc); \
                return Poco::Util::Application::EXIT_CONFIG; \
            } \
            return pApp->run(); \
        }


#endif // Util_Application_INCLUDED
