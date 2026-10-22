#include "core/api/environment.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <sys/stat.h>

namespace ThetaZ {

// Global instance for lazy initialization
static EnvironmentSetup* g_environment_setup = nullptr;

EnvironmentSetup::EnvironmentSetup() {
    // Get home directory
    const char* home = getenv("HOME");
    if (!home) {
        // Fallback: use current directory
        home = ".";
    }
    
    // Use the configured Wine/ThetaZ prefix when one is set. The default
    // (~/.ThetaZ) is only a fallback so fresh installs have a usable root.
    const char* configured_prefix = getenv("WINEPREFIX");
    if (!configured_prefix || !configured_prefix[0]) {
        configured_prefix = getenv("THETAZ_PREFIX");
    }
    if (configured_prefix && configured_prefix[0]) {
        prefix_path_ = configured_prefix;
        // Allow a prefix path with a trailing slash without producing "//".
        while (prefix_path_.size() > 1 && prefix_path_.back() == '/') {
            prefix_path_.pop_back();
        }
    } else {
        prefix_path_ = std::string(home) + "/.ThetaZ";
    }
    
    // Set subdirectories
    drive_c_path_ = prefix_path_ + "/drive_c";
    windows_path_ = drive_c_path_ + "/Windows";
    program_files_path_ = drive_c_path_ + "/Program Files";
    user_profile_path_ = drive_c_path_ + "/Users";
    system32_path_ = windows_path_ + "/System32";
}

EnvironmentSetup& EnvironmentSetup::instance() {
    static EnvironmentSetup* instance = nullptr;
    if (!instance) {
        instance = new EnvironmentSetup();
        fprintf(stderr, "EnvironmentSetup: Initialized at %s\n", instance->get_prefix_path().c_str());
    }
    return *instance;
}

bool EnvironmentSetup::create_all_directories() {
    bool success = true;
    
    // Create all necessary directories
    success &= create_directory_recursive(prefix_path_);
    success &= create_directory_recursive(drive_c_path_);
    success &= create_directory_recursive(windows_path_);
    success &= create_directory_recursive(system32_path_);
    success &= create_directory_recursive(program_files_path_);
    success &= create_directory_recursive(user_profile_path_);
    
    // Create common subdirectories
    success &= create_directory_recursive(program_files_path_ + "/Microsoft Office");
    success &= create_directory_recursive(program_files_path_ + "/Microsoft Office/root");
    success &= create_directory_recursive(program_files_path_ + "/Microsoft Office/root/Office16");
    success &= create_directory_recursive(user_profile_path_ + "/Default");
    success &= create_directory_recursive(user_profile_path_ + "/Default/AppData");
    success &= create_directory_recursive(windows_path_ + "/Fonts");
    success &= create_directory_recursive(windows_path_ + "/drivers");
    success &= create_directory_recursive(windows_path_ + "/Microsoft");
    
    // Create Windows subdirectories
    success &= create_directory_recursive(windows_path_ + "/Temp");
    success &= create_directory_recursive(windows_path_ + "/SysWow64");
    
    if (success) {
        fprintf(stderr, "EnvironmentSetup: Created all directories at %s\n", prefix_path_.c_str());
    } else {
        fprintf(stderr, "EnvironmentSetup: Some directories failed to create\n");
    }
    
    return success;
}

bool EnvironmentSetup::create_directory_recursive(const std::string& path) {
    // Try to create directory (already exists is ok)
    int result = mkdir(path.c_str(), 0755);
    
    if (result == 0) {
        // Successfully created
        fprintf(stderr, "EnvironmentSetup: Created directory %s\n", path.c_str());
        return true;
    }
    
    if (errno == EEXIST) {
        // Already exists - that's fine
        return true;
    }
    
    // Other errors
    fprintf(stderr, "EnvironmentSetup: Failed to create directory %s: %s\n", 
            path.c_str(), strerror(errno));
    return false;
}

std::string EnvironmentSetup::get_prefix_path() const { return prefix_path_; }

std::string EnvironmentSetup::get_drive_c_path() const { return drive_c_path_; }

std::string EnvironmentSetup::get_windows_path() const { return windows_path_; }

std::string EnvironmentSetup::get_program_files_path() const { return program_files_path_; }

std::string EnvironmentSetup::get_user_profile_path() const { return user_profile_path_; }

std::string EnvironmentSetup::get_system32_path() const { return system32_path_; }

void initialize_environment() {
    EnvironmentSetup::instance();
}

std::string get_prefix_path() {
    return EnvironmentSetup::instance().get_prefix_path();
}

std::string get_drive_c_path() {
    return EnvironmentSetup::instance().get_drive_c_path();
}

std::string get_windows_path() {
    return EnvironmentSetup::instance().get_windows_path();
}

std::string get_program_files_path() {
    return EnvironmentSetup::instance().get_program_files_path();
}

std::string get_user_profile_path() {
    return EnvironmentSetup::instance().get_user_profile_path();
}

std::string get_system32_path() {
    return EnvironmentSetup::instance().get_system32_path();
}

} // namespace ThetaZ
