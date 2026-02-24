#include "Utils.hpp"
#include <sstream>
#include <cctype>
#include <fstream>
#include <sys/stat.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <cstdlib>
#include <iterator>

namespace Utils {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLower(const std::string& s) {
    std::string result = s;
    for (size_t i = 0; i < result.size(); i++)
        result[i] = (char)tolower((unsigned char)result[i]);
    return result;
}

std::string toUpper(const std::string& s) {
    std::string result = s;
    for (size_t i = 0; i < result.size(); i++)
        result[i] = (char)toupper((unsigned char)result[i]);
    return result;
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> result;
    std::istringstream iss(s);
    std::string token;
    while (std::getline(iss, token, delim))
        result.push_back(token);
    return result;
}

std::string intToStr(long long n) {
    std::ostringstream oss;
    oss << n;
    return oss.str();
}

long long strToLong(const std::string& s) {
    std::istringstream iss(s);
    long long n = 0;
    iss >> n;
    return n;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
}

bool fileExists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

bool isDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

std::string getExtension(const std::string& path) {
    size_t dot = path.rfind('.');
    if (dot == std::string::npos) return "";
    size_t slash = path.rfind('/');
    if (slash != std::string::npos && slash > dot) return "";
    return path.substr(dot);
}

std::string normalizePath(const std::string& path) {
    std::vector<std::string> parts;
    std::vector<std::string> segments = split(path, '/');
    for (size_t i = 0; i < segments.size(); i++) {
        const std::string& seg = segments[i];
        if (seg.empty() || seg == ".") continue;
        if (seg == "..") {
            if (!parts.empty()) parts.pop_back();
        } else {
            parts.push_back(seg);
        }
    }
    std::string result = "/";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0) result += "/";
        result += parts[i];
    }
    return result;
}

std::string urlDecode(const std::string& s) {
    std::string result;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            char h1 = s[i + 1], h2 = s[i + 2];
            bool valid = ((h1 >= '0' && h1 <= '9') || (h1 >= 'a' && h1 <= 'f') || (h1 >= 'A' && h1 <= 'F')) &&
                         ((h2 >= '0' && h2 <= '9') || (h2 >= 'a' && h2 <= 'f') || (h2 >= 'A' && h2 <= 'F'));
            if (valid) {
                std::string hex = s.substr(i + 1, 2);
                char c = (char)strtol(hex.c_str(), NULL, 16);
                result += c;
                i += 2;
            } else {
                result += s[i];
            }
        } else if (s[i] == '+') {
            result += ' ';
        } else {
            result += s[i];
        }
    }
    return result;
}

void setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        std::cerr << "fcntl F_GETFL failed for fd " << fd << ": " << strerror(errno) << "\n";
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        std::cerr << "fcntl F_SETFL failed for fd " << fd << ": " << strerror(errno) << "\n";
}

} // namespace Utils
