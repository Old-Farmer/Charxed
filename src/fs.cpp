#include "fs.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>

#include "exception.h"
#include "linux/limits.h"
#include "str.h"
#include "utils.h"

namespace charxed {

namespace {
constexpr const char* kXDGConfigHomeEnv = "XDG_CONFIG_HOME";
constexpr const char* kXDGCacheHomeEnv = "XDG_CACHE_HOME";

constexpr mode_t kDefaultCreateMode = 0644;
constexpr mode_t kDefaultMkDirMode = 0755;
}  // namespace

Path::Path() {}

Path::Path(const std::string& str) {
    CHX_ASSERT(!str.empty());
    if (IsAbsolutePath(str)) {
        absolute_path_ = Normalize(str);
    } else {
        absolute_path_ = Normalize(cwd_ + str);
        GenRelativePath();
    }

    std::string::size_type pos = absolute_path_.find_last_of(kPathSeperator);
    CHX_ASSERT(pos != std::string::npos);
    file_name_len_ = absolute_path_.size() - pos - 1;
    last_cwd_version_ = cwd_version_;
}

zstring_view Path::FileName() const noexcept {
    return zstring_view(
        absolute_path_.c_str() + absolute_path_.size() - file_name_len_,
        file_name_len_);
}

zstring_view Path::ThisPath() noexcept {
    if (relative_path_.empty()) {
        return zstring_view(absolute_path_);
    }

    if (last_cwd_version_ != cwd_version_) {
        last_cwd_version_ = cwd_version_;
        GenRelativePath();
    }
    return zstring_view(relative_path_);
}

const std::string& Path::AbsolutePath() const noexcept {
    return absolute_path_;
}

std::string_view Path::Dir() const noexcept {
    std::string::size_type pos = absolute_path_.find_last_of(kPathSeperator);
    CHX_ASSERT(pos != std::string::npos);
    return std::string_view(absolute_path_).substr(0, pos + 1);
}

std::string Path::Normalize(const std::string& path) {
    CHX_ASSERT(IsAbsolutePath(path));
    if (path.empty()) {
        return {};
    }

    auto sub_paths = StrSplit(path, kPathSeperator);
    std::vector<std::string_view> sta;
    for (auto sub_path : sub_paths) {
        if (sub_path.empty() || sub_path == ".") {
            ;
        } else if (sub_path == "..") {
            if (!sta.empty()) sta.pop_back();
        } else {
            sta.push_back(sub_path);
        }
    }
    std::string normalized_path;
    for (auto& sub_path : sta) {
        normalized_path += kPathSeperator;
        normalized_path += sub_path;
    }
    if (path.back() == kPathSeperator) {
        normalized_path += kPathSeperator;
    }
    return normalized_path;
}

const std::string& Path::GetCwd() noexcept { return cwd_; }

const std::string& Path::GetAppRoot() noexcept { return app_root_; }

std::string Path::GetConfig() {
    const char* env = getenv(kXDGConfigHomeEnv);
    std::string path;
    if (env == nullptr) {
        path = GetHome() + kPathSeperator + ".config" + kPathSeperator;
    } else {
        path = std::string(env) + kPathSeperator;
    }
    mkdir(path.c_str(), 755);  // best effort, ignore ret
    return path;
}

std::string Path::GetCache() {
    static std::string path = [] {
        const char* env = getenv(kXDGCacheHomeEnv);
        std::string path;
        if (env == nullptr) {
            path = GetHome() + kPathSeperator + ".cache" + kPathSeperator;
        } else {
            path = std::string(env) + kPathSeperator;
        }
        mkdir(path.c_str(), 755);  // best effort, ignore ret
        return path;
    }();
    return path;
}

std::string Path::GetHome() {
    const char* env = getenv("HOME");
    // TODO: better home detection?
    if (env == nullptr) {
        throw Exception("{}", "HOME detection fail");
    }
    return env;
}

const std::string& Path::GetCwdSys() {
    cwd_version_++;
    char buf[PATH_MAX + 1];
    char* ret = getcwd(buf, PATH_MAX + 1);
    if (ret == nullptr) {
        throw FSException("Getcwd Error: {}", strerror(errno));
    }
    cwd_ = std::string(buf) + kPathSeperator;
    return cwd_;
}

const std::string& Path::GetAppRootSys() {
    char buf[PATH_MAX + 1];
    ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX + 1);
    if (len == -1) {
        throw FSException("GetAppRootSys Error: {}", strerror(errno));
    }
    buf[len] = '\0';
    app_root_ = std::string(buf);
    auto pos = app_root_.find_last_of(kPathSeperator, app_root_.size() - 1);
    if (pos == std::string::npos) {
        throw FSException("{}",
                          "GetAppRootSys Error: find_last_of can't find a /");
    }
    pos = app_root_.find_last_of(kPathSeperator, pos - 1);
    if (pos == std::string::npos) {
        throw FSException(
            "{}", "GetAppRootSys Error: find_last_of can't find another /");
    }
    app_root_.resize(pos + 1);
    return app_root_;
}

int64_t Path::LastPathSeperator(std::string_view path) {
    size_t loc = path.find_last_of(kPathSeperator);
    return loc == std::string_view::npos ? -1 : loc;
}

std::vector<std::string> Path::ListUnderPath(const std::string& path) {
    std::vector<std::string> ret;

    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        if (errno == ENOTDIR) {
            return {};
        }
        throw FSException("opendir error: {}", strerror(errno));
    }
    struct dirent* ent;
    errno = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        if (ent->d_type == DT_DIR) {
            auto child_path = std::string(ent->d_name);
            child_path.append(1, kPathSeperator);
            ret.push_back(std::move(child_path));
        } else if (ent->d_type == DT_REG) {
            auto child_path = std::string(ent->d_name);
            ret.push_back(std::move(child_path));
        }
        // We ignore other type file.
        // TODO: maybe we shouldn't ignore them?
    }
    closedir(dir);  // If dir is not bad, closedir will never fail, so it will
                    // not effect errno.
    if (errno != 0) {
        throw FSException("readdir error: {}", strerror(errno));
    }
    return ret;
}

bool Path::IsAbsolutePath(std::string_view path) {
    CHX_ASSERT(!path.empty());
    return path[0] == kPathSeperator;
}

void Path::GenRelativePath() {
    size_t i = 0;
    for (; i < absolute_path_.size() && i < cwd_.size(); i++) {
        if (absolute_path_[i] != cwd_[i]) {
            break;
        }
    }
    relative_path_.clear();
    size_t path_sep_cnt =
        std::count(cwd_.begin() + i, cwd_.end(), kPathSeperator);
    for (size_t j = 0; j < path_sep_cnt; j++) {
        relative_path_.append(std::string("..") + kPathSeperator);
    }
    relative_path_.append(absolute_path_.begin() + i, absolute_path_.end());
    if (relative_path_.empty()) {
        relative_path_.append(".");
        relative_path_.append(1, kPathSeperator);
    }
}

std::string Path::cwd_ = "";
int64_t Path::cwd_version_ = 0;

std::string Path::app_root_ = "";

Result GetFileStat(const std::string& path, FileStat& file_stat) {
    CHX_ASSERT(!path.empty());
    struct stat sta;
    int res = stat(path.c_str(), &sta);
    if (res == -1) {
        if (errno == ENOENT) {
            return kNotExist;
        }
        throw FSException("stat error: {}", strerror(errno));
    }
    if (sta.st_mode & S_IRUSR) file_stat.mode |= kFMRead;
    if (sta.st_mode & S_IWUSR) file_stat.mode |= kFMWrite;
    if (sta.st_mode & S_IXUSR) file_stat.mode |= kFMExec;
    file_stat.size = sta.st_size;
    return kOk;
}

void Create(const std::string& path) {
    int fd = open(path.c_str(), O_CREAT, kDefaultCreateMode);
    if (fd == -1) {
        throw OSException(errno, "open {} error: {}", path, strerror(errno));
    }
    close(fd);
}

void Remove(const std::string& path) {
    if (unlink(path.c_str()) == -1) {
        throw OSException(errno, "unlink {} error: {}", path, strerror(errno));
    }
}

void MakeDirectory(const std::string& path) {
    if (mkdir(path.c_str(), kDefaultMkDirMode) == -1) {
        throw OSException(errno, "mkdir {} error: {}", path, strerror(errno));
    }
}

void RemoveDirectory(const std::string& path) {
    CHX_ASSERT(!path.empty());
    CHX_ASSERT(path.back() == kPathSeperator);

    auto entries = Path::ListUnderPath(path);
    if (entries.empty()) {
        rmdir(path.c_str());
    }
    for (auto& e : entries) {
        if (e.back() == kPathSeperator) {
            RemoveDirectory(path + e);
        } else {
            Remove(e);
        }
    }
}

}  // namespace charxed
