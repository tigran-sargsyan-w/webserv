#pragma once
#include <string>
#include <vector>

namespace Utils {
    std::string trim(const std::string& s);
    std::string toLower(const std::string& s);
    std::string toUpper(const std::string& s);
    std::vector<std::string> split(const std::string& s, char delim);
    std::string intToStr(long long n);
    long long strToLong(const std::string& s);
    std::string readFile(const std::string& path);
    bool fileExists(const std::string& path);
    bool isDirectory(const std::string& path);
    std::string getExtension(const std::string& path);
    std::string normalizePath(const std::string& path);
    std::string urlDecode(const std::string& s);
    void setNonBlocking(int fd);
}
