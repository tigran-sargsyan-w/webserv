#include "MimeTypes.hpp"

#include <cctype>
#include <map>

namespace
{
    std::string getFileExtension(const std::string &path)
    {
        size_t dot;
        size_t slash;

        dot = path.find_last_of('.');
        slash = path.find_last_of('/');

        if (dot == std::string::npos)
            return ("");
        if (slash != std::string::npos && dot < slash)
            return ("");
        return (path.substr(dot));
    }

    std::string toLower(const std::string &text)
    {
        std::string result;
        size_t i;

        i = 0;
        while (i < text.length())
        {
            result += static_cast<char>(
                std::tolower(static_cast<unsigned char>(text[i])));
            i++;
        }
        return (result);
    }

    std::map<std::string, std::string> createMimeTypes(void)
    {
        std::map<std::string, std::string> types;

        types[".html"] = "text/html";
        types[".htm"] = "text/html";
        types[".css"] = "text/css";
        types[".js"] = "application/javascript";
        types[".mjs"] = "application/javascript";
        types[".json"] = "application/json";
        types[".xml"] = "application/xml";
        types[".txt"] = "text/plain";
        types[".csv"] = "text/csv";

        types[".png"] = "image/png";
        types[".jpg"] = "image/jpeg";
        types[".jpeg"] = "image/jpeg";
        types[".gif"] = "image/gif";
        types[".svg"] = "image/svg+xml";
        types[".ico"] = "image/x-icon";
        types[".webp"] = "image/webp";
        types[".bmp"] = "image/bmp";

        types[".pdf"] = "application/pdf";
        types[".zip"] = "application/zip";
        types[".tar"] = "application/x-tar";
        types[".gz"] = "application/gzip";

        types[".mp3"] = "audio/mpeg";
        types[".wav"] = "audio/wav";
        types[".ogg"] = "audio/ogg";

        types[".mp4"] = "video/mp4";
        types[".webm"] = "video/webm";

        types[".woff"] = "font/woff";
        types[".woff2"] = "font/woff2";
        types[".ttf"] = "font/ttf";
        types[".otf"] = "font/otf";

        types[".wasm"] = "application/wasm";

        return (types);
    }
}

std::string MimeTypes::getMimeType(const std::string &path)
{
    static const std::map<std::string, std::string> types = createMimeTypes();

    std::string extension;
    std::map<std::string, std::string>::const_iterator it;

    extension = toLower(getFileExtension(path));
    it = types.find(extension);
    if (it != types.end())
        return (it->second);
    return ("application/octet-stream");
}