/* Copyright (c) 2018 - present, VE Software Inc. All rights reserved
 *
 * This source code is licensed under Apache 2.0 License
 *  (found in the LICENSE.Apache file in the root directory)
 */

#include "base/Base.h"
#include <signal.h>
#include "fs/FileUtils.h"
#include "process/ProcessUtils.h"
#include "proc/ProcAccessor.h"

namespace nebula {

Status ProcessUtils::isPidAvailable(uint32_t pid) {
    constexpr auto SIG_OK = 0;
    if (::kill(pid, SIG_OK) == 0) {
        return Status::Error("Process `%u' already existed", pid);
    }
    if (errno == EPERM) {
        return Status::Error("Process `%u' already existed but denied to access", pid);
    }
    if (errno != ESRCH) {
        return Status::Error("Uknown error: `%s'", ::strerror(errno));
    }
    return Status::OK();
}


Status ProcessUtils::isPidAvailable(const std::string &pidFile) {
    // Test existence and readability
    if (::access(pidFile.c_str(), R_OK) == -1) {
        if (errno == ENOENT) {
            return Status::OK();
        } else {
            return Status::Error("%s: %s", pidFile.c_str(), ::strerror(errno));
        }
    }
    // Pidfile is readable
    static const std::regex pattern("([0-9]+)");
    std::smatch result;
    proc::ProcAccessor accessor(pidFile);
    if (!accessor.next(pattern, result)) {
        // Pidfile is readable but has no valid pid
        return Status::OK();
    }
    // Now we have a valid pid
    return isPidAvailable(folly::to<uint32_t>(result[1].str()));
}


Status ProcessUtils::makePidFile(const std::string &pidFile, uint32_t pid) {
    // TODO(dutor) mkdir -p `dirname pidFile`
    auto *file = ::fopen(pidFile.c_str(), "w");
    if (file == nullptr) {
        return Status::Error("Open or create `%s': %s", pidFile.c_str(), ::strerror(errno));
    }
    if (pid == 0) {
        pid = ::getpid();
    }
    ::fprintf(file, "%u\n", pid);
    ::fflush(file);
    ::fclose(file);
    return Status::OK();
}


StatusOr<std::string> ProcessUtils::getExePath(uint32_t pid) {
    if (pid == 0) {
        pid = ::getpid();
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%u/exe", pid);
    return fs::FileUtils::readLink(path);
}


StatusOr<std::string> ProcessUtils::getExeCWD(uint32_t pid) {
    if (pid == 0) {
        pid = ::getpid();
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%u/cwd", pid);
    return fs::FileUtils::readLink(path);
}


StatusOr<std::string> ProcessUtils::getProcessName(uint32_t pid) {
    if (pid == 0) {
        pid = ::getpid();
    }
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%u/comm", pid);
    proc::ProcAccessor accessor(path);
    std::string name;
    if (!accessor.next(name)) {
        return Status::Error("Failed to read from `%s'", path);
    }
    return name;
}


uint32_t ProcessUtils::maxPid() {
    proc::ProcAccessor accessor("/proc/sys/kernel/pid_max");
    static const std::regex pattern("([0-9]+)");
    std::smatch result;
    auto ok = accessor.next(pattern, result);
    CHECK(ok);
    return folly::to<uint32_t>(result[1].str());
}

}   // namespace nebula