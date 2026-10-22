#include "core/api/kernel32.h"
#include "core/ipc/thetaserver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/stat.h>
#include <algorithm>
#include <sstream>

namespace ThetaZ {

// Global instance for lazy initialization
static PathResolver* g_path_resolver = nullptr;


void PathResolver::initialize() {
    if (g_path_resolver == nullptr) {
        return;
    }
    
    // Get search paths from environment
    std::vector<std::string> search_paths = get_search_paths();
    
    // Add default paths
    add_search_path(get_windows_path() + "/System32");
    add_search_path(get_windows_path() + "/System32/drivers");
    add_search_path(get_program_files_path() + "/");
    add_search_path(get_prefix_path());
    
    fprintf(stderr, "PathResolver: Initialized with %zu search paths\n", search_paths.size());
}

std::string PathResolver::resolve_path(const std::string& windows_path, int resolution_type) {
    if (!g_path_resolver) {
        initialize();
    }
    
    // Placeholder - actual implementation will be added later
    return windows_path;
}

std::string PathResolver::resolve_simple(const std::string& windows_path) {
    if (!g_path_resolver) {
        initialize();
    }
    
    return resolve_exact_path(windows_path);
}

std::string PathResolver::resolve_exact_path(const std::string& path) {
    return path;
}

std::string PathResolver::translate_windows_to_linux(const std::string& windows_path) {
    std::string path = windows_path;

    // Drive-letter paths: C: is the configured prefix, Z: is the host root
    // (Wine-style), and any other drive falls back to the host root as well
    // rather than silently failing.
    if (path.size() >= 2 && path[1] == ':' &&
        ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) {
        char letter = static_cast<char>(toupper(static_cast<unsigned char>(path[0])));
        std::string rest = path.substr(2);
        while (!rest.empty() && (rest[0] == '\\' || rest[0] == '/')) rest.erase(0, 1);
        std::string prefix;
        if (letter == 'C') prefix = get_drive_c_path();
        if (!prefix.empty()) {
            path = prefix + "/" + rest;
        } else {
            path = "/" + rest;
        }
    }

    // Replace all backslashes with forward slashes
    while (path.find("\\") != std::string::npos) {
        size_t pos = path.find("\\");
        path = path.substr(0, pos) + "/" + path.substr(pos + 1);
    }

    return path;
}

std::string PathResolver::convert_windows_format(const std::string& path) {
    std::string result = path;
    
    // Convert forward slashes to backslashes
    while (result.find("/") != std::string::npos) {
        size_t pos = result.find("/");
        result = result.substr(0, pos) + "\\" + result.substr(pos + 1);
    }
    
    return result;
}

std::string PathResolver::convert_unix_style(const std::string& path) {
    std::string result = path;
    
    // Convert backslashes to forward slashes
    while (result.find("\\") != std::string::npos) {
        size_t pos = result.find("\\");
        result = result.substr(0, pos) + "/" + result.substr(pos + 1);
    }
    
    return result;
}

std::string PathResolver::expand_environment_variables(const std::string& path) {
    std::string result = path;
    
    // Expand common environment variables
    if (result.find("%TEMP%") != std::string::npos) {
        const char* temp = getenv("TEMP");
        if (!temp) temp = "/tmp";
        result = result.replace(result.find("%TEMP%"), 6, temp);
    }
    
    if (result.find("%APPDATA%") != std::string::npos) {
        std::string appdata = getenv("APPDATA") ? getenv("APPDATA") : get_user_profile_path() + "/AppData/Roaming";
        result = result.replace(result.find("%APPDATA%"), 8, appdata);
    }
    
    if (result.find("%PROGRAMFILES%") != std::string::npos) {
        std::string programfiles = getenv("PROGRAMFILES") ? getenv("PROGRAMFILES") : get_program_files_path();
        result = result.replace(result.find("%PROGRAMFILES%"), 12, programfiles);
    }
    
    if (result.find("%USERPROFILE%") != std::string::npos) {
        std::string userprofile = getenv("USERPROFILE") ? getenv("USERPROFILE") : get_user_profile_path();
        result = result.replace(result.find("%USERPROFILE%"), 12, userprofile);
    }
    
    return result;
}

std::string PathResolver::resolve_with_search_paths(const std::string& windows_path) {
    return windows_path;
}

void PathResolver::add_search_path(const std::string& path) {
    // Placeholder - actual implementation will be added later
}

std::vector<std::string> PathResolver::get_search_paths() const {
    return std::vector<std::string>();
}

std::string PathResolver::create_path_if_missing(const std::string& windows_path) {
    return translate_windows_to_linux(windows_path);
}

void initialize_path_resolver() {
    if (!g_path_resolver) {
        g_path_resolver = new PathResolver();
        g_path_resolver->initialize();
    }
}

PathResolver& get_path_resolver() {
    if (!g_path_resolver) {
        initialize_path_resolver();
    }
    return *g_path_resolver;
}

std::string resolve_path(const std::string& windows_path) {
    return get_path_resolver().resolve_path(windows_path);
}

std::string resolve_path_simple(const std::string& windows_path) {
    return get_path_resolver().resolve_simple(windows_path);
}

void add_search_path(const std::string& path) {
    get_path_resolver().add_search_path(path);
}

std::vector<std::string> get_search_paths() {
    return get_path_resolver().get_search_paths();
}

} // namespace ThetaZ
