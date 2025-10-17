#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <string>
#include <vector>
#include <unistd.h>

class ProcessManager
{
public:
    ProcessManager(const std::string &executable_path, const std::vector<std::string> &args = {});
    ~ProcessManager();

    bool start();
    void stop();

private:
    std::string path_;
    std::vector<std::string> args_; 
    pid_t child_pid_{-1};
};

#endif 