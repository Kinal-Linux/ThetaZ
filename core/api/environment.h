#pragma once

#include <string>
#include <vector>

namespace ThetaZ {

// EnvironmentSetup class - Automatic directory creation system
class EnvironmentSetup {
private:
    std::string prefix_path_;
    std::string drive_c_path_;
    std::string windows_path_;
    std::string program_files_path_;
    std::string system32_path_;
    std::string user_profile_path_;
    EnvironmentSetup();
    bool create_directory_recursive(const std::string& path);
    
public:
    static EnvironmentSetup& instance();
    std::string get_prefix_path() const;
    std::string get_drive_c_path() const;
    std::string get_windows_path() const;
    std::string get_program_files_path() const;
    std::string get_user_profile_path() const;
    std::string get_system32_path() const;
    
    bool create_all_directories();
};

} // namespace ThetaZ
