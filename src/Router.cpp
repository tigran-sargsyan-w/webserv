#include "Router.hpp"
#include "CgiHandler.hpp"
#include "Utils.hpp"
#include <sstream>
#include <dirent.h>
#include <sys/stat.h>
#include <ctime>
#include <cerrno>
#include <cstring>
#include <fstream>

Router::Router(const ServerConfig& config) : config_(config) {}

// ── public entry point ────────────────────────────────────────────────────────

HttpResponse Router::route(const HttpRequest& req) {
    // Handle HTTP version not supported already caught by parser; handle 501 here
    if (req.method != "GET" && req.method != "POST" && req.method != "DELETE"
        && req.method != "HEAD") {
        return errorResponse(501);
    }

    const LocationConfig* loc = findLocation(req.path);
    if (!loc) return errorResponse(404);

    // redirect
    if (!loc->redirect.empty()) {
        size_t sp = loc->redirect.find(' ');
        int code = 302;
        std::string url = loc->redirect;
        if (sp != std::string::npos) {
            code = (int)Utils::strToLong(loc->redirect.substr(0, sp));
            url  = loc->redirect.substr(sp + 1);
        }
        HttpResponse resp;
        resp.setStatus(code);
        resp.setHeader("Location", url);
        resp.setBody("", "text/html");
        return resp;
    }

    if (!isMethodAllowed(req.method, *loc)) {
        HttpResponse resp = errorResponse(405);
        // Build Allow header
        std::string allow;
        for (size_t i = 0; i < loc->allowedMethods.size(); i++) {
            if (i > 0) allow += ", ";
            allow += loc->allowedMethods[i];
        }
        resp.setHeader("Allow", allow);
        return resp;
    }

    if (req.method == "GET" || req.method == "HEAD")
        return handleGet(req, *loc);
    if (req.method == "POST")
        return handlePost(req, *loc);
    if (req.method == "DELETE")
        return handleDelete(req, *loc);
    return errorResponse(501);
}

// ── location matching ─────────────────────────────────────────────────────────

const LocationConfig* Router::findLocation(const std::string& path) const {
    const LocationConfig* best = NULL;
    size_t bestLen = 0;
    for (size_t i = 0; i < config_.locations.size(); i++) {
        const LocationConfig& loc = config_.locations[i];
        const std::string& lp = loc.path;
        // prefix match
        if (path == lp || (path.size() > lp.size() &&
            path.substr(0, lp.size()) == lp &&
            (lp[lp.size()-1] == '/' || path[lp.size()] == '/'))) {
            if (lp.size() >= bestLen) {
                bestLen = lp.size();
                best = &config_.locations[i];
            }
        } else if (lp == "/") {
            if (bestLen == 0) { best = &config_.locations[i]; bestLen = 1; }
        }
    }
    return best;
}

// ── path resolution ───────────────────────────────────────────────────────────

std::string Router::resolvePath(const std::string& root, const std::string& reqPath,
                                 const std::string& locPath) {
    // strip the location prefix from reqPath then append to root
    std::string rel = reqPath;
    if (locPath != "/" && rel.substr(0, locPath.size()) == locPath)
        rel = rel.substr(locPath.size());
    if (rel.empty() || rel[0] != '/') rel = "/" + rel;

    std::string base = root;
    if (!base.empty() && base[base.size()-1] == '/') base.erase(base.size()-1);

    std::string full = base + rel;
    // normalize to prevent traversal
    // manually normalize the combined path
    std::vector<std::string> parts;
    std::vector<std::string> segs = Utils::split(full, '/');
    for (size_t i = 0; i < segs.size(); i++) {
        const std::string& s = segs[i];
        if (s.empty() || s == ".") continue;
        if (s == "..") { if (!parts.empty()) parts.pop_back(); }
        else parts.push_back(s);
    }
    std::string normalized;
    if (full[0] == '/') normalized = "/";
    for (size_t i = 0; i < parts.size(); i++) {
        if (i > 0 || !normalized.empty()) normalized += "/";
        normalized += parts[i];
    }

    // security: result must still start with root (also normalized)
    std::string normRoot;
    std::vector<std::string> rootParts = Utils::split(base, '/');
    std::vector<std::string> rp;
    for (size_t i = 0; i < rootParts.size(); i++) {
        if (rootParts[i].empty() || rootParts[i] == ".") continue;
        if (rootParts[i] == "..") { if (!rp.empty()) rp.pop_back(); }
        else rp.push_back(rootParts[i]);
    }
    if (base[0] == '/') normRoot = "/";
    for (size_t i = 0; i < rp.size(); i++) {
        if (i > 0 || !normRoot.empty()) normRoot += "/";
        normRoot += rp[i];
    }

    if (normalized.substr(0, normRoot.size()) != normRoot)
        return ""; // traversal detected
    return normalized;
}

// ── method check ─────────────────────────────────────────────────────────────

bool Router::isMethodAllowed(const std::string& method, const LocationConfig& loc) const {
    if (loc.allowedMethods.empty()) return true;
    for (size_t i = 0; i < loc.allowedMethods.size(); i++)
        if (loc.allowedMethods[i] == method) return true;
    return false;
}

// ── CGI check ────────────────────────────────────────────────────────────────

bool Router::isCgiRequest(const std::string& path, const LocationConfig& loc) const {
    if (loc.cgiExtension.empty()) return false;
    std::string ext = Utils::getExtension(path);
    return ext == loc.cgiExtension;
}

// ── GET handler ───────────────────────────────────────────────────────────────

HttpResponse Router::handleGet(const HttpRequest& req, const LocationConfig& loc) {
    std::string root = loc.root;
    if (root.empty()) root = config_.errorPages.empty() ? "./www" : "./www";

    std::string fsPath = resolvePath(root, req.path, loc.path);
    if (fsPath.empty()) return errorResponse(403);

    if (isCgiRequest(fsPath, loc)) {
        if (!Utils::fileExists(fsPath)) return errorResponse(404);
        return handleCgi(req, fsPath, loc);
    }

    if (Utils::isDirectory(fsPath)) {
        return serveDirectory(fsPath, req.path, loc);
    }
    if (!Utils::fileExists(fsPath)) return errorResponse(404);
    return serveFile(fsPath, req.path);
}

// ── POST handler ──────────────────────────────────────────────────────────────

HttpResponse Router::handlePost(const HttpRequest& req, const LocationConfig& loc) {
    // CGI
    std::string root = loc.root;
    if (root.empty()) root = "./www";
    std::string fsPath = resolvePath(root, req.path, loc.path);
    if (!fsPath.empty() && isCgiRequest(fsPath, loc)) {
        if (!Utils::fileExists(fsPath)) return errorResponse(404);
        return handleCgi(req, fsPath, loc);
    }

    // File upload
    if (!loc.uploadDir.empty()) {
        long long maxBody = loc.clientMaxBodySize >= 0
                          ? loc.clientMaxBodySize
                          : config_.clientMaxBodySize;
        if (maxBody >= 0 && (long long)req.body.size() > maxBody)
            return errorResponse(413);

        // derive filename with counter for uniqueness
        static unsigned long uploadSequence = 0;
        if (uploadSequence == static_cast<unsigned long>(-1)) uploadSequence = 0;
        std::string filename = "upload_" + Utils::intToStr((long long)time(NULL)) +
                               "_" + Utils::intToStr((long long)(++uploadSequence));
        std::map<std::string, std::string>::const_iterator ct =
            req.headers.find("content-disposition");
        if (ct != req.headers.end()) {
            size_t fnpos = ct->second.find("filename=\"");
            if (fnpos != std::string::npos) {
                fnpos += 10;
                size_t fnend = ct->second.find('"', fnpos);
                if (fnend != std::string::npos) {
                    std::string rawName = ct->second.substr(fnpos, fnend - fnpos);
                    // Sanitize: strip path separators and keep basename only
                    size_t slash = rawName.rfind('/');
                    if (slash != std::string::npos) rawName = rawName.substr(slash + 1);
                    size_t bslash = rawName.rfind('\\');
                    if (bslash != std::string::npos) rawName = rawName.substr(bslash + 1);
                    // Remove any remaining dangerous characters; collapse multiple dots
                    std::string safe;
                    bool lastDot = false;
                    for (size_t i = 0; i < rawName.size(); ++i) {
                        char c = rawName[i];
                        if (c == '/' || c == '\\' || c == '\0') continue;
                        if (c == '.') {
                            if (safe.empty() || lastDot) continue; // no leading/consecutive dots
                            safe += c;
                            lastDot = true;
                        } else {
                            safe += c;
                            lastDot = false;
                        }
                    }
                    // Strip trailing dot
                    if (!safe.empty() && safe[safe.size() - 1] == '.') safe.erase(safe.size() - 1);
                    if (!safe.empty()) filename = safe;
                }
            }
        }

        std::string uploadPath = loc.uploadDir;
        if (!uploadPath.empty() && uploadPath[uploadPath.size()-1] != '/')
            uploadPath += "/";
        uploadPath += filename;

        std::ofstream out(uploadPath.c_str(), std::ios::binary);
        if (!out.is_open()) return errorResponse(500);
        out.write(req.body.c_str(), (std::streamsize)req.body.size());
        out.close();

        HttpResponse resp;
        resp.setStatus(201);
        resp.setBody("<html><body>File uploaded</body></html>", "text/html");
        return resp;
    }

    return errorResponse(405);
}

// ── DELETE handler ────────────────────────────────────────────────────────────

HttpResponse Router::handleDelete(const HttpRequest& req, const LocationConfig& loc) {
    std::string root = loc.root;
    if (root.empty()) root = "./www";
    std::string fsPath = resolvePath(root, req.path, loc.path);
    if (fsPath.empty()) return errorResponse(403);
    if (!Utils::fileExists(fsPath)) return errorResponse(404);
    if (Utils::isDirectory(fsPath)) return errorResponse(403);
    if (remove(fsPath.c_str()) != 0) return errorResponse(500);

    HttpResponse resp;
    resp.setStatus(204);
    resp.setBody("", "text/plain");
    return resp;
}

// ── static file serving ───────────────────────────────────────────────────────

HttpResponse Router::serveFile(const std::string& filePath, const std::string& /*urlPath*/) {
    std::string content = Utils::readFile(filePath);
    if (content.empty() && !Utils::fileExists(filePath)) return errorResponse(404);

    std::string ext  = Utils::getExtension(filePath);
    std::string mime = HttpResponse::getMimeType(ext);

    HttpResponse resp;
    resp.setStatus(200);
    resp.setBody(content, mime);
    return resp;
}

// ── directory serving ─────────────────────────────────────────────────────────

HttpResponse Router::serveDirectory(const std::string& dirPath, const std::string& urlPath,
                                     const LocationConfig& loc) {
    // try index file
    std::string indexFile = loc.index.empty() ? "index.html" : loc.index;
    std::string indexPath = dirPath;
    if (indexPath[indexPath.size()-1] != '/') indexPath += "/";
    indexPath += indexFile;

    if (Utils::fileExists(indexPath)) return serveFile(indexPath, urlPath);

    if (loc.autoindex) return generateAutoindex(dirPath, urlPath);

    return errorResponse(403);
}

// ── autoindex ─────────────────────────────────────────────────────────────────

HttpResponse Router::generateAutoindex(const std::string& dirPath, const std::string& urlPath) {
    DIR* dir = opendir(dirPath.c_str());
    if (!dir) return errorResponse(500);

    std::string up = urlPath;
    if (!up.empty() && up[up.size()-1] != '/') up += "/";

    std::ostringstream html;
    html << "<!DOCTYPE html><html><head><title>Index of " << up << "</title></head>"
         << "<body><h1>Index of " << up << "</h1><hr><pre>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == ".") continue;
        std::string fullPath = dirPath + "/" + name;
        struct stat st;
        stat(fullPath.c_str(), &st);
        bool isDir = S_ISDIR(st.st_mode);
        std::string displayName = name + (isDir ? "/" : "");
        html << "<a href=\"" << up << name << (isDir ? "/" : "") << "\">"
             << displayName << "</a>\n";
    }
    closedir(dir);
    html << "</pre><hr></body></html>";

    HttpResponse resp;
    resp.setStatus(200);
    resp.setBody(html.str(), "text/html");
    return resp;
}

// ── error responses ───────────────────────────────────────────────────────────

HttpResponse Router::errorResponse(int code) {
    HttpResponse resp;
    resp.setStatus(code);

    // check for custom error page
    std::map<int, std::string>::const_iterator it = config_.errorPages.find(code);
    if (it != config_.errorPages.end()) {
        // find the root for the error page – use first location or ./www
        std::string root = "./www";
        if (!config_.locations.empty() && !config_.locations[0].root.empty())
            root = config_.locations[0].root;
        std::string filePath = root + it->second;
        if (Utils::fileExists(filePath)) {
            std::string content = Utils::readFile(filePath);
            resp.setBody(content, "text/html");
            return resp;
        }
    }

    std::ostringstream body;
    body << "<!DOCTYPE html><html><body><h1>"
         << code << " " << HttpResponse::statusText(code)
         << "</h1></body></html>";
    resp.setBody(body.str(), "text/html");
    return resp;
}

// ── CGI dispatch ─────────────────────────────────────────────────────────────

HttpResponse Router::handleCgi(const HttpRequest& req, const std::string& scriptPath,
                                const LocationConfig& /*loc*/) {
    CgiHandler cgi(req, scriptPath, config_);
    return cgi.execute();
}
