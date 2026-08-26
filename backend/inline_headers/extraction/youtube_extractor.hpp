#ifndef YOUTUBE_EXTRACTOR_HPP
#define YOUTUBE_EXTRACTOR_HPP

#include <string>
#include <iostream>
#include <filesystem>
#include <regex>
#include <cstdlib>

namespace fs = std::filesystem;

// Operating System Detection & Constants
#if defined(_WIN32) || defined(_WIN64)
    #define YT_OS_WINDOWS
    #define YT_EXE_EXT ".exe"
    #define YT_DEV_NULL "NUL"
#elif defined(__APPLE__)
    #define YT_OS_MACOS
    #define YT_EXE_EXT ""
    #define YT_DEV_NULL "/dev/null"
#else
    #define YT_OS_LINUX
    #define YT_EXE_EXT ""
    #define YT_DEV_NULL "/dev/null"
#endif

class YouTubeExtractor {
private:
    static fs::path GetModulesDir() {
        fs::path modulesDir = "modules";
        std::error_code ec;
        if (!fs::exists(modulesDir)) {
            fs::create_directories(modulesDir, ec);
        }
        return modulesDir;
    }

    static int RunSystemCommand(const std::string& cmd) {
#if defined(YT_OS_WINDOWS)
        std::string wrapped = "\"" + cmd + "\"";
        return system(wrapped.c_str());
#else
        return system(cmd.c_str());
#endif
    }

    static void MakeExecutable(const fs::path& filePath) {
#ifndef YT_OS_WINDOWS
        std::string cmd = "chmod +x \"" + filePath.string() + "\" 2>" YT_DEV_NULL;
        RunSystemCommand(cmd);
#endif
    }

public:
    static bool IsYouTubeUrl(const std::string& url) {
        return url.find("youtube.com/") != std::string::npos || 
               url.find("youtu.be/") != std::string::npos;
    }

    static std::string ExtractVideoId(const std::string& yt_url) {
        std::regex id_regex(R"((?:v=|\/embed\/|\/shorts\/|\/v\/|youtu\.be\/|\/watch\?v=)([^"&?\/\s]{11}))");
        std::smatch match;
        if (std::regex_search(yt_url, match, id_regex) && match.size() > 1) {
            return match[1].str();
        }
        return "";
    }

    static bool EnsureYtDlpPresent() {
        fs::path ytDlpPath = GetModulesDir() / ("yt-dlp" YT_EXE_EXT);
        if (fs::exists(ytDlpPath) && fs::file_size(ytDlpPath) > 100000) {
            MakeExecutable(ytDlpPath);
            return true;
        }

        std::cout << "[YT Debug] yt-dlp binary not found in modules/. Downloading to modules/...\n";

        std::string downloadUrl;
#if defined(YT_OS_WINDOWS)
        downloadUrl = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
#elif defined(YT_OS_MACOS)
        downloadUrl = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos";
#else
        downloadUrl = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp";
#endif

        std::string downloadCmd = "curl -s -L \"" + downloadUrl + "\" -o \"" + ytDlpPath.string() + "\"";
        RunSystemCommand(downloadCmd);

        if (fs::exists(ytDlpPath) && fs::file_size(ytDlpPath) > 100000) {
            MakeExecutable(ytDlpPath);
            return true;
        }

        return false;
    }

    static bool EnsureFfmpegPresent() {
        fs::path ffmpegPath = GetModulesDir() / ("ffmpeg" YT_EXE_EXT);
        if (fs::exists(ffmpegPath) && fs::file_size(ffmpegPath) > 100000) {
            MakeExecutable(ffmpegPath);
            return true;
        }

        std::cout << "[YT Debug] ffmpeg binary not found in modules/. Downloading to modules/...\n";

#if defined(YT_OS_WINDOWS)
        std::string psCmd = "powershell -Command \"$ProgressPreference = 'SilentlyContinue'; "
            "Invoke-WebRequest -Uri 'https://github.com/GyanD/codexffmpeg/releases/download/7.0.2/ffmpeg-7.0.2-essentials_build.zip' -OutFile 'ffmpeg_temp.zip'; "
            "Expand-Archive -Path 'ffmpeg_temp.zip' -DestinationPath 'ffmpeg_extracted' -Force; "
            "Get-ChildItem -Path 'ffmpeg_extracted' -Filter 'ffmpeg.exe' -Recurse | Copy-Item -Destination 'modules'; "
            "Remove-Item -Recurse -Force 'ffmpeg_temp.zip', 'ffmpeg_extracted'\"";
        RunSystemCommand(psCmd);
#elif defined(YT_OS_MACOS)
        std::string macCmd = "curl -s -L 'https://evermeet.cx/ffmpeg/getrelease/zip' -o 'ffmpeg_temp.zip' && "
                             "unzip -o -q 'ffmpeg_temp.zip' -d 'modules' && "
                             "rm -f 'ffmpeg_temp.zip'";
        RunSystemCommand(macCmd);
#else // Linux
        std::string linuxCmd = "curl -s -L 'https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz' -o 'ffmpeg_temp.tar.xz' && "
                               "mkdir -p ffmpeg_extracted && "
                               "tar -xf 'ffmpeg_temp.tar.xz' -C ffmpeg_extracted --strip-components=1 && "
                               "cp ffmpeg_extracted/ffmpeg modules/ && "
                               "rm -rf 'ffmpeg_temp.tar.xz' ffmpeg_extracted";
        RunSystemCommand(linuxCmd);
#endif

        if (fs::exists(ffmpegPath) && fs::file_size(ffmpegPath) > 100000) {
            MakeExecutable(ffmpegPath);
            return true;
        }

        return false;
    }

    static std::string ResolveToLocalFile(const std::string& inputUrl, const fs::path& cacheDir) {
        if (!IsYouTubeUrl(inputUrl)) {
            return inputUrl;
        }

        fs::path absoluteCacheDir = fs::absolute(cacheDir);
        if (!fs::exists(absoluteCacheDir)) {
            std::error_code ec;
            fs::create_directories(absoluteCacheDir, ec);
        }

        std::string videoId = ExtractVideoId(inputUrl);
        if (videoId.empty()) {
            std::cout << "[YT Debug] ERROR: Invalid YouTube URL or Video ID.\n";
            return "";
        }

        fs::path outputTemplatePath = absoluteCacheDir / ("yt_" + videoId);
        fs::path expectedMp3 = absoluteCacheDir / ("yt_" + videoId + ".mp3");

        if (fs::exists(expectedMp3) && fs::file_size(expectedMp3) > 1024) {
            std::cout << "[YT Debug] Using cached MP3 audio file: " << expectedMp3.string() << "\n";
            return expectedMp3.string();
        }

        if (!EnsureYtDlpPresent()) {
            std::cout << "[YT Debug] ERROR: Missing yt-dlp binary in modules/\n";
            return "";
        }

        EnsureFfmpegPresent();

        fs::path ytDlpPath = GetModulesDir() / ("yt-dlp" YT_EXE_EXT);
        fs::path ffmpegDir = GetModulesDir();

        std::cout << "[YT Debug] Downloading and converting stream to MP3 for video ID: " << videoId << "...\n";

        std::string cmd = "\"" + ytDlpPath.string() + "\" -q --no-warnings --no-playlist "
                          "--ffmpeg-location \"" + ffmpegDir.string() + "\" "
                          "-x --audio-format mp3 --audio-quality 0 "
                          "-o \"" + outputTemplatePath.string() + ".%(ext)s\" \"" + inputUrl + "\"";

        RunSystemCommand(cmd);

        if (!fs::exists(expectedMp3)) {
            std::error_code ec;
            for (auto& p : fs::directory_iterator(absoluteCacheDir, ec)) {
                if (p.path().filename().string().find("yt_" + videoId) != std::string::npos && p.path().extension() == ".mp3") {
                    expectedMp3 = p.path();
                    break;
                }
            }
        }

        if (fs::exists(expectedMp3) && fs::file_size(expectedMp3) > 1024) {
            std::cout << "[YT Debug] MP3 ready for miniaudio: " << expectedMp3.string() << "\n";
            return expectedMp3.string();
        }

        std::cout << "[YT Debug] ERROR: Stream extraction or MP3 conversion failed.\n";
        return "";
    }
};

#endif
