#include "ProcessManager.h"
#include <iostream>
#include <signal.h>
#include <sys/wait.h>

// 更新构造函数以初始化参数列表
ProcessManager::ProcessManager(const std::string &executable_path, const std::vector<std::string> &args)
    : path_(executable_path), args_(args) {}

ProcessManager::~ProcessManager()
{
    stop();
}

bool ProcessManager::start()
{
    if (child_pid_ != -1)
    {
        std::cout << "[ProcManager] Process is already running." << std::endl;
        return true;
    }

    child_pid_ = fork();

    if (child_pid_ < 0)
    {
        std::cerr << "[ProcManager] ERROR: Failed to fork process." << std::endl;
        return false;
    }

    if (child_pid_ == 0)
    {
        std::vector<char *> argv;
        argv.push_back(const_cast<char *>(path_.c_str()));

        for (const auto &arg : args_)
        {
            argv.push_back(const_cast<char *>(arg.c_str()));
        }
        argv.push_back(nullptr);

        execv(path_.c_str(), argv.data());
        std::cerr << "[ProcManager] ERROR: Failed to execute " << path_ << std::endl;
        _exit(1);
    }
    else
    {
        std::cout << "[ProcManager] Started process '" << path_ << "' with PID " << child_pid_ << std::endl;

        sleep(1);
        int status;
        pid_t result = waitpid(child_pid_, &status, WNOHANG);
        if (result == child_pid_)
        {
            std::cerr << "[ProcManager] ERROR: Process " << path_ << " terminated immediately. Is the path correct and executable?" << std::endl;
            child_pid_ = -1;
            return false;
        }
    }
    return true;
}

void ProcessManager::stop()
{
    if (child_pid_ != -1)
    {
        std::cout << "[ProcManager] Stopping process with PID " << child_pid_ << std::endl;
        kill(child_pid_, SIGTERM);

        int status;
        waitpid(child_pid_, &status, 0);

        child_pid_ = -1;
        std::cout << "[ProcManager] Process stopped." << std::endl;
    }
}