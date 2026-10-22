#pragma once

#include <string>
#include <vector>

namespace ThetaZ {

// PathResolver class - Multi-Path Resolution System
class PathResolver {
public:
    void initialize();
    std::string resolve_path(const std::string& windows_path, int resolution_type = 0);
    std::string resolve_simple(const std::string& windows_path);
    std::string resolve_exact_path(const std::string& path);
    std::string translate_windows_to_linux(const std::string& windows_path);
    std::string convert_windows_format(const std::string& path);
    std::string convert_unix_style(const std::string& path);
    std::string expand_environment_variables(const std::string& path);
    std::string resolve_with_search_paths(const std::string& windows_path);
    void add_search_path(const std::string& path);
    std::vector<std::string> get_search_paths() const;
    std::string create_path_if_missing(const std::string& windows_path);
};

// Path translation function (wine_to_host) - converts Windows paths to Linux paths
std::string wine_to_host(const char* path);

// Process-wide PathResolver singleton.
PathResolver& get_path_resolver();

} // namespace ThetaZ
