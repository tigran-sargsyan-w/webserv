#include "ConfigParser.hpp"
#include "Utils.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

// ── tokenizer ────────────────────────────────────────────────────────────────

void ConfigParser::tokenize(const std::string& content) {
    tokens_.clear();
    pos_ = 0;
    size_t i = 0;
    while (i < content.size()) {
        // skip whitespace
        if (isspace((unsigned char)content[i])) { i++; continue; }
        // skip comments
        if (content[i] == '#') {
            while (i < content.size() && content[i] != '\n') i++;
            continue;
        }
        // single-char tokens
        if (content[i] == '{' || content[i] == '}' || content[i] == ';') {
            tokens_.push_back(std::string(1, content[i]));
            i++;
            continue;
        }
        // word token
        std::string tok;
        while (i < content.size() && !isspace((unsigned char)content[i]) &&
               content[i] != '{' && content[i] != '}' &&
               content[i] != ';' && content[i] != '#') {
            tok += content[i++];
        }
        if (!tok.empty()) tokens_.push_back(tok);
    }
}

std::string ConfigParser::next() {
    if (pos_ >= tokens_.size())
        throw std::runtime_error("Unexpected end of config");
    return tokens_[pos_++];
}

std::string ConfigParser::peek() {
    if (pos_ >= tokens_.size()) return "";
    return tokens_[pos_];
}

bool ConfigParser::hasMore() {
    return pos_ < tokens_.size();
}

void ConfigParser::expect(const std::string& tok) {
    std::string got = next();
    if (got != tok)
        throw std::runtime_error("Expected '" + tok + "' got '" + got + "'");
}

// ── size parser ───────────────────────────────────────────────────────────────

long long ConfigParser::parseSize(const std::string& s) {
    if (s.empty()) return 0;
    std::string num = s;
    long long mult = 1;
    char last = s[s.size() - 1];
    if (last == 'k' || last == 'K') { mult = 1024; num = s.substr(0, s.size()-1); }
    else if (last == 'm' || last == 'M') { mult = 1024*1024; num = s.substr(0, s.size()-1); }
    else if (last == 'g' || last == 'G') { mult = 1024LL*1024*1024; num = s.substr(0, s.size()-1); }
    return Utils::strToLong(num) * mult;
}

// ── main parse entry ──────────────────────────────────────────────────────────

Config ConfigParser::parse(const std::string& filename) {
    std::ifstream f(filename.c_str());
    if (!f.is_open())
        throw std::runtime_error("Cannot open config file: " + filename);
    std::ostringstream oss;
    oss << f.rdbuf();
    tokenize(oss.str());

    Config cfg;
    while (hasMore()) {
        std::string tok = next();
        if (tok == "server") {
            cfg.servers.push_back(parseServer());
        } else {
            throw std::runtime_error("Unexpected token: " + tok);
        }
    }
    if (cfg.servers.empty())
        throw std::runtime_error("No server blocks found in config");
    return cfg;
}

// ── server block ─────────────────────────────────────────────────────────────

ServerConfig ConfigParser::parseServer() {
    ServerConfig srv;
    expect("{");
    while (peek() != "}") {
        std::string key = next();
        if (key == "listen") {
            std::string val = next();
            expect(";");
            // val may be "8080" or "0.0.0.0:8080"
            size_t colon = val.find(':');
            if (colon != std::string::npos) {
                srv.host = val.substr(0, colon);
                srv.port = (int)Utils::strToLong(val.substr(colon + 1));
            } else {
                srv.port = (int)Utils::strToLong(val);
            }
        } else if (key == "server_name") {
            while (peek() != ";") srv.serverNames.push_back(next());
            expect(";");
        } else if (key == "client_max_body_size") {
            srv.clientMaxBodySize = parseSize(next());
            expect(";");
        } else if (key == "error_page") {
            int code = (int)Utils::strToLong(next());
            std::string page = next();
            expect(";");
            srv.errorPages[code] = page;
        } else if (key == "location") {
            std::string path = next();
            srv.locations.push_back(parseLocation(path));
        } else {
            // skip unknown directive
            while (peek() != ";" && peek() != "{" && peek() != "}" && hasMore())
                next();
            if (peek() == ";") next();
        }
    }
    expect("}");
    return srv;
}

// ── location block ───────────────────────────────────────────────────────────

LocationConfig ConfigParser::parseLocation(const std::string& path) {
    LocationConfig loc;
    loc.path = path;
    expect("{");
    while (peek() != "}") {
        std::string key = next();
        if (key == "root") {
            loc.root = next(); expect(";");
        } else if (key == "index") {
            loc.index = next(); expect(";");
        } else if (key == "autoindex") {
            std::string val = next(); expect(";");
            loc.autoindex = (val == "on");
        } else if (key == "allowed_methods" || key == "limit_except") {
            loc.allowedMethods.clear();
            while (peek() != ";") loc.allowedMethods.push_back(next());
            expect(";");
        } else if (key == "upload_dir") {
            loc.uploadDir = next(); expect(";");
        } else if (key == "cgi_extension") {
            loc.cgiExtension = next(); expect(";");
        } else if (key == "client_max_body_size") {
            loc.clientMaxBodySize = parseSize(next()); expect(";");
        } else if (key == "return") {
            std::string code = next();
            std::string url = next();
            expect(";");
            loc.redirect = code + " " + url;
        } else {
            while (peek() != ";" && peek() != "{" && peek() != "}" && hasMore())
                next();
            if (peek() == ";") next();
        }
    }
    expect("}");
    return loc;
}
