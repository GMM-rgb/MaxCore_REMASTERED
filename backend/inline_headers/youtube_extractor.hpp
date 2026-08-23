#ifndef YOUTUBE_EXTRACTOR_HPP
#define YOUTUBE_EXTRACTOR_HPP

#include <string>
#include <iostream>
#include <filesystem>
#include <regex>
#include <cstdlib>

namespace fs = std::filesystem;

class YouTubeExtractor {
private:
    static fs::path GetModulesDir() {
        fs::path modulesDir = "modules";
        if (!fs::exists(modulesDir)) {
            fs::create_directories(modulesDir);
        }
        return modulesDir;
    }

    static int RunSystemCommand(const std::string& cmd) {
#if defined(_WIN32) || defined(_WIN64)
        std::string wrapped = "\"" + cmd + "\"";
        return system(wrapped.c_str());
#else
        return system(cmd.c_str());
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
        fs::path ytDlpPath = GetModulesDir() / "yt-dlp.exe";
        if (fs::exists(ytDlpPath) && fs::file_size(ytDlpPath) > 1000000) {
            return true;
        }

        std::cout << "[YT Debug] yt-dlp.exe not found in modules/. Downloading...\n";
        std::string downloadCmd = "curl -s -L \"https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe\" -o \"" + ytDlpPath.string() + "\"";
        RunSystemCommand(downloadCmd);

        return (fs::exists(ytDlpPath) && fs::file_size(ytDlpPath) > 1000000);
    }

    static bool EnsureFfmpegPresent() {
        fs::path ffmpegPath = GetModulesDir() / "ffmpeg.exe";
        if (fs::exists(ffmpegPath) && fs::file_size(ffmpegPath) > 1000000) {
            return true;
        }

        if (system("ffmpeg -version >NUL 2>&1") == 0) {
            return true;
        }

        std::cout << "[YT Debug] ffmpeg.exe not found in modules/. Downloading...\n";
        std::string psCmd = "powershell -Command \"$ProgressPreference = 'SilentlyContinue'; "
            "Invoke-WebRequest -Uri 'https://github.com/GyanD/codexffmpeg/releases/download/7.0.2/ffmpeg-7.0.2-essentials_build.zip' -OutFile 'ffmpeg_temp.zip'; "
            "Expand-Archive -Path 'ffmpeg_temp.zip' -DestinationPath 'ffmpeg_extracted' -Force; "
            "Get-ChildItem -Path 'ffmpeg_extracted' -Filter 'ffmpeg.exe' -Recurse | Copy-Item -Destination 'modules'; "
            "Remove-Item -Recurse -Force 'ffmpeg_temp.zip', 'ffmpeg_extracted'\"";

        RunSystemCommand(psCmd);

        return (fs::exists(ffmpegPath) && fs::file_size(ffmpegPath) > 1000000);
    }

    static std::string ResolveToLocalFile(const std::string& inputUrl, const fs::path& cacheDir) {
        if (!IsYouTubeUrl(inputUrl)) {
            return inputUrl;
        }

        // Force resolution to absolute path based on execution working directory
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
            std::cout << "[YT Debug] ERROR: Missing yt-dlp.exe in modules/\n";
            return "";
        }

        EnsureFfmpegPresent();

        fs::path ytDlpPath = GetModulesDir() / "yt-dlp.exe";
        std::cout << "[YT Debug] Downloading and converting stream to MP3 for video ID: " << videoId << "...\n";

        std::string cmd = "\"" + ytDlpPath.string() + "\" -q --no-warnings --no-playlist "
                          "--ffmpeg-location \"" + GetModulesDir().string() + "\" "
                          "-x --audio-format mp3 --audio-quality 0 "
                          "-o \"" + outputTemplatePath.string() + ".%(ext)s\" \"" + inputUrl + "\"";

        RunSystemCommand(cmd);

        if (!fs::exists(expectedMp3)) {
            for (auto& p : fs::directory_iterator(absoluteCacheDir)) {
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
