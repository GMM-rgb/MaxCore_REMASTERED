#ifndef YOUTUBE_EXTRACTOR_HPP
#define YOUTUBE_EXTRACTOR_HPP

#include <string>
#include <iostream>
#include <filesystem>
#include <regex>
#include <cstdlib>
#include <future>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <atomic>
#include <algorithm>

// ============================================================================
// Platform Abstraction Layer
// ============================================================================

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    #define EXTRACTOR_OS_WINDOWS
    #include <windows.h>
    #include <urlmon.h>
    #pragma comment(lib, "urlmon.lib")
    #define CROSS_POPEN _popen
    #define CROSS_PCLOSE _pclose
    constexpr const char* YT_DLP_NAME = "yt-dlp.exe";
    constexpr const char* FFMPEG_NAME = "ffmpeg.exe";
#elif defined(__APPLE__) || defined(__MACH__)
    #define EXTRACTOR_OS_MAC
    #include <unistd.h>
    #define CROSS_POPEN popen
    #define CROSS_PCLOSE pclose
    constexpr const char* YT_DLP_NAME = "yt-dlp";
    constexpr const char* FFMPEG_NAME = "ffmpeg";
#else
    #define EXTRACTOR_OS_LINUX
    #include <unistd.h>
    #define CROSS_POPEN popen
    #define CROSS_PCLOSE pclose
    constexpr const char* YT_DLP_NAME = "yt-dlp";
    constexpr const char* FFMPEG_NAME = "ffmpeg";
#endif

namespace fs = std::filesystem;

// ============================================================================
// Progress Tracker
// ============================================================================

struct ProgressTracker {
    std::atomic<double> downloadCurrent{0};
    std::atomic<double> downloadTotal{0};
    std::atomic<double> extractCurrent{0};
    std::atomic<double> extractTotal{100};
    std::atomic<bool> isExtracting{false};
    std::string downloadLabel{"Downloading Stream"};
    std::string extractLabel{"MP3 Conversion"};

    void UpdateDownload(double currentBytes, double streamTotalBytes = 0) {
        if (isExtracting.load()) return;

        if (streamTotalBytes > 0) {
            double prevTot = downloadTotal.load();
            if (streamTotalBytes > prevTot) {
                downloadTotal.store(streamTotalBytes);
            }
        }
        downloadCurrent.store(currentBytes);

        if (downloadCurrent.load() > downloadTotal.load() && downloadTotal.load() > 0) {
            downloadTotal.store(downloadCurrent.load());
        }
    }
};

class DownloadProgressBar {
private:
    static void EnableVTMode() {
#if defined(EXTRACTOR_OS_WINDOWS)
        static bool enabled = false;
        if (!enabled) {
            SetConsoleOutputCP(CP_UTF8);
            SetConsoleCP(CP_UTF8);

            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE) {
                DWORD dwMode = 0;
                if (GetConsoleMode(hOut, &dwMode)) {
                    dwMode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
                    SetConsoleMode(hOut, dwMode);
                }
            }
            enabled = true;
        }
#endif
    }

public:
    static std::string FormatSize(double bytes) {
        if (bytes <= 0) return "0.00 B";
        const char* units[] = {"B", "KB", "MB", "GB"};
        int idx = 0;
        while (bytes >= 1024.0 && idx < 3) {
            bytes /= 1024.0;
            idx++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << bytes << " " << units[idx];
        return oss.str();
    }

    static std::string FormatTime(double seconds) {
        if (std::isinf(seconds) || std::isnan(seconds) || seconds < 0) return "--:--";
        int secs = static_cast<int>(seconds);
        int mins = secs / 60;
        secs %= 60;
        int hrs = mins / 60;
        mins %= 60;
        std::ostringstream oss;
        if (hrs > 0) {
            oss << std::setfill('0') << std::setw(2) << hrs << ":";
        }
        oss << std::setfill('0') << std::setw(2) << mins << ":"
            << std::setfill('0') << std::setw(2) << secs;
        return oss.str();
    }

    static std::string BuildBar(double current, double total, int width = 24) {
        double ratio = (total > 0) ? (current / total) : 0.0;
        ratio = std::clamp(ratio, 0.0, 1.0);

        double totalCells = ratio * width;
        int fullBlocks = static_cast<int>(totalCells);
        bool hasHalfBlock = (totalCells - fullBlocks) >= 0.5;
        if (fullBlocks == width) hasHalfBlock = false;

        int emptyBlocks = width - fullBlocks - (hasHalfBlock ? 1 : 0);

        std::ostringstream bar;
        bar << "\033[38;5;242m┃\033[0m";
        bar << "\033[38;5;39m\033[48;5;39m";
        for (int i = 0; i < fullBlocks; ++i) {
            bar << "█";
        }

        if (hasHalfBlock) {
            bar << "\033[38;5;39m\033[48;5;24m▌";
        }

        bar << "\033[48;5;24m";
        for (int i = 0; i < emptyBlocks; ++i) {
            bar << " ";
        }
        bar << "\033[0m";
        bar << "\033[38;5;242m┃\033[0m";

        return bar.str();
    }

    static void Render(const ProgressTracker& tracker, std::chrono::steady_clock::time_point startTime) {
        EnableVTMode();

        static int prevLines = 1;

        if (prevLines > 1) {
            for (int i = 0; i < prevLines - 1; ++i) {
                std::cout << "\033[1A\033[2K";
            }
        }
        std::cout << "\r\033[2K";

        double dlCur = tracker.downloadCurrent.load();
        double dlTot = tracker.downloadTotal.load();
        double dlRatio = (dlTot > 0) ? (dlCur / dlTot) : 0.0;
        dlRatio = std::clamp(dlRatio, 0.0, 1.0);

        auto now = std::chrono::steady_clock::now();
        double elapsedSec = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / 1000.0;
        double speed = (elapsedSec > 0) ? (dlCur / elapsedSec) : 0.0;
        double remainingBytes = (dlTot > dlCur) ? (dlTot - dlCur) : 0.0;
        double eta = (speed > 0) ? (remainingBytes / speed) : 0.0;

        std::string dlBarStr = BuildBar(dlCur, dlTot, 24);

        std::cout << "[" << std::left << std::setw(18) << tracker.downloadLabel << "] "
                  << dlBarStr << " "
                  << std::right << std::setw(5) << std::fixed << std::setprecision(1) << (dlRatio * 100.0) << "% | "
                  << FormatSize(dlCur) << " / " << FormatSize(dlTot)
                  << " | Speed: " << FormatSize(speed) << "/s"
                  << " | ETA: " << FormatTime(eta);

        if (tracker.isExtracting.load()) {
            double extCur = tracker.extractCurrent.load();
            double extTot = tracker.extractTotal.load();
            double extRatio = (extTot > 0) ? (extCur / extTot) : 0.0;
            extRatio = std::clamp(extRatio, 0.0, 1.0);

            std::string extBarStr = BuildBar(extCur, extTot, 24);

            std::cout << "\n\033[2K"
                      << "[" << std::left << std::setw(18) << tracker.extractLabel << "] "
                      << extBarStr << " "
                      << std::right << std::setw(5) << std::fixed << std::setprecision(1) << (extRatio * 100.0) << "% | Processing...";
            prevLines = 2;
        } else {
            prevLines = 1;
        }
        std::cout << std::flush;
    }
};

// ============================================================================
// YouTube Extractor Engine
// ============================================================================

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
        return system(cmd.c_str());
    }

    static bool EnsureDependencies() {
        fs::path modulesDir = GetModulesDir();
        fs::path ytDlpPath = modulesDir / YT_DLP_NAME;
        fs::path ffmpegPath = modulesDir / FFMPEG_NAME;

        // 1. Check & Download yt-dlp
        if (!fs::exists(ytDlpPath) || fs::file_size(ytDlpPath) < 10000) {
            std::cout << "[Dependency Check] Downloading yt-dlp..." << std::endl;
            
#if defined(EXTRACTOR_OS_WINDOWS)
            const char* ytDlpUrl = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp.exe";
            HRESULT hr = URLDownloadToFileA(NULL, ytDlpUrl, ytDlpPath.string().c_str(), 0, NULL);
            if (FAILED(hr) || !fs::exists(ytDlpPath)) return false;
#elif defined(EXTRACTOR_OS_MAC)
            std::string curlCmd = "curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp_macos -o \"" + ytDlpPath.generic_string() + "\" && chmod +x \"" + ytDlpPath.generic_string() + "\"";
            if (RunSystemCommand(curlCmd) != 0 || !fs::exists(ytDlpPath)) return false;
#else // Linux
            std::string curlCmd = "curl -L https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp -o \"" + ytDlpPath.generic_string() + "\" && chmod +x \"" + ytDlpPath.generic_string() + "\"";
            if (RunSystemCommand(curlCmd) != 0 || !fs::exists(ytDlpPath)) return false;
#endif
            std::cout << "[Dependency Check] yt-dlp installed successfully." << std::endl;
        }

        // 2. Check & Download FFmpeg
        if (!fs::exists(ffmpegPath) || fs::file_size(ffmpegPath) < 10000) {
            // Check if system-installed ffmpeg is available on POSIX
#if !defined(EXTRACTOR_OS_WINDOWS)
            if (RunSystemCommand("which ffmpeg > /dev/null 2>&1") == 0) {
                // System ffmpeg exists, create symlink or copy to modules
                std::string linkCmd = "ln -sf $(which ffmpeg) \"" + ffmpegPath.generic_string() + "\"";
                RunSystemCommand(linkCmd);
                if (fs::exists(ffmpegPath)) return true;
            }
#endif

            std::cout << "[Dependency Check] Downloading FFmpeg build..." << std::endl;

#if defined(EXTRACTOR_OS_WINDOWS)
            fs::path zipPath = modulesDir / "ffmpeg_temp.zip";
            fs::path tempExtractDir = modulesDir / "ffmpeg_temp";

            std::string downloadCmd = "powershell -Command \"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
                                      "Invoke-WebRequest -Uri 'https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip' "
                                      "-OutFile '" + zipPath.generic_string() + "'\"";
            
            if (RunSystemCommand(downloadCmd) != 0 || !fs::exists(zipPath)) return false;

            std::string extractCmd = "powershell -Command \"Expand-Archive -Path '" + zipPath.generic_string() + "' -DestinationPath '" + tempExtractDir.generic_string() + "' -Force; "
                                     "Get-ChildItem -Path '" + tempExtractDir.generic_string() + "' -Recurse -Filter 'ffmpeg.exe' | Copy-Item -Destination '" + ffmpegPath.generic_string() + "'; "
                                     "Remove-Item -Recurse -Force '" + tempExtractDir.generic_string() + "', '" + zipPath.generic_string() + "'\"";

            RunSystemCommand(extractCmd);
#elif defined(EXTRACTOR_OS_MAC)
            std::string fetchCmd = "curl -L https://evermeet.cx/ffmpeg/getrelease/zip -o modules/ffmpeg.zip && unzip -o modules/ffmpeg.zip -d modules/ && chmod +x modules/ffmpeg && rm modules/ffmpeg.zip";
            RunSystemCommand(fetchCmd);
#else // Linux
            std::string fetchCmd = "curl -L https://johnvansickle.com/ffmpeg/releases/ffmpeg-release-amd64-static.tar.xz -o modules/ffmpeg.tar.xz && tar -xf modules/ffmpeg.tar.xz -C modules/ --strip-components=1 && chmod +x modules/ffmpeg && rm modules/ffmpeg.tar.xz";
            RunSystemCommand(fetchCmd);
#endif

            if (!fs::exists(ffmpegPath)) {
                std::cerr << "[Error] Failed to install ffmpeg binary!" << std::endl;
                return false;
            }
            std::cout << "[Dependency Check] FFmpeg converter installed successfully." << std::endl;
        }

        return true;
    }

    static bool ExecuteYtDlpStream(const std::string& cmd, ProgressTracker& tracker) {
        std::string pipeCmd;
#if defined(EXTRACTOR_OS_WINDOWS)
        pipeCmd = "\"" + cmd + " --newline --progress-template \"dl_data:%(progress.downloaded_bytes)s/%(progress.total_bytes)s/%(progress.total_bytes_estimate)s\"\"";
#else
        pipeCmd = cmd + " --newline --progress-template \"dl_data:%(progress.downloaded_bytes)s/%(progress.total_bytes)s/%(progress.total_bytes_estimate)s\"";
#endif

        FILE* pipe = CROSS_POPEN(pipeCmd.c_str(), "r");
        if (!pipe) return false;

        char buffer[512];
        std::regex templateRegex(R"(dl_data:(\d+|NA|none)/(\d+|NA|none)/(\d+|NA|none))");
        std::regex legacyRegex(R"(\[download\]\s+([\d\.]+)%\s+of\s+~?\s*([\d\.]+)\s*(\w+))");
        std::smatch match;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);

            if (line.find("[ExtractAudio]") != std::string::npos || line.find("[ffmpeg]") != std::string::npos) {
                if (!tracker.isExtracting.load()) {
                    double finalTot = tracker.downloadTotal.load();
                    if (finalTot <= 0) finalTot = 1.0;
                    tracker.downloadCurrent.store(finalTot);
                    tracker.isExtracting.store(true);
                }
            }

            if (!tracker.isExtracting.load()) {
                if (std::regex_search(line, match, templateRegex)) {
                    std::string curStr = match[1].str();
                    std::string totStr = match[2].str();
                    std::string estStr = match[3].str();

                    double curBytes = (curStr != "NA" && curStr != "none") ? std::stod(curStr) : 0.0;
                    double totBytes = 0.0;

                    if (totStr != "NA" && totStr != "none") {
                        totBytes = std::stod(totStr);
                    } else if (estStr != "NA" && estStr != "none") {
                        totBytes = std::stod(estStr);
                    }

                    if (curBytes > 0) {
                        tracker.UpdateDownload(curBytes, totBytes);
                    }
                }
                else if (std::regex_search(line, match, legacyRegex)) {
                    double percent = std::stod(match[1].str());
                    double totalVal = std::stod(match[2].str());
                    std::string unit = match[3].str();

                    double mult = 1.0;
                    if (unit.find("KB") != std::string::npos || unit.find("KiB") != std::string::npos) mult = 1024.0;
                    else if (unit.find("MB") != std::string::npos || unit.find("MiB") != std::string::npos) mult = 1024.0 * 1024.0;
                    else if (unit.find("GB") != std::string::npos || unit.find("GiB") != std::string::npos) mult = 1024.0 * 1024.0 * 1024.0;

                    double totalBytes = totalVal * mult;
                    double currentBytes = (percent / 100.0) * totalBytes;

                    tracker.UpdateDownload(currentBytes, totalBytes);
                }
            }
        }

        int status = CROSS_PCLOSE(pipe);

        if (status == 0) {
            double finalSize = tracker.downloadTotal.load();
            if (finalSize <= 0) finalSize = 1.0;
            tracker.downloadCurrent.store(finalSize);
        }

        return status == 0;
    }

public:
    template <typename TaskFunc>
    static std::future<bool> ExecuteProcedureWithProgressAsync(
        std::string downloadLabel,
        std::string extractLabel,
        TaskFunc task) 
    {
        return std::async(std::launch::async, [dlLabel = std::move(downloadLabel),
                                                extLabel = std::move(extractLabel),
                                                task = std::move(task)]() -> bool {
            ProgressTracker tracker;
            tracker.downloadLabel = dlLabel;
            tracker.extractLabel = extLabel;

            std::atomic<bool> taskFinished{false};
            bool result = false;

            std::thread worker([&]() {
                result = task(tracker);
                taskFinished.store(true);
            });

            auto startTime = std::chrono::steady_clock::now();

            while (!taskFinished.load()) {
                DownloadProgressBar::Render(tracker, startTime);
                std::this_thread::sleep_for(std::chrono::milliseconds(80));
            }

            if (worker.joinable()) {
                worker.join();
            }

            DownloadProgressBar::Render(tracker, startTime);
            std::cout << "\n";
            if (tracker.isExtracting.load()) {
                std::cout << "\n";
            }
            std::cout << std::flush;

            return result;
        });
    }

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

    static std::string ResolveToLocalFile(const std::string& inputUrl, const fs::path& cacheDir) {
        if (!EnsureDependencies()) {
            std::cerr << "[Error] Dependent binaries could not be downloaded/resolved." << std::endl;
            return "";
        }

        if (!IsYouTubeUrl(inputUrl)) {
            return inputUrl;
        }

        fs::path absoluteCacheDir = cacheDir.empty() ? (fs::current_path() / "cache") : fs::absolute(cacheDir);
        if (!fs::exists(absoluteCacheDir)) {
            std::error_code ec;
            fs::create_directories(absoluteCacheDir, ec);
        }

        std::string videoId = ExtractVideoId(inputUrl);
        if (videoId.empty()) {
            return "";
        }

        std::string cleanUrl = "https://www.youtube.com/watch?v=" + videoId;

        fs::path outputTemplatePath = absoluteCacheDir / ("yt_" + videoId);
        fs::path expectedMp3 = absoluteCacheDir / ("yt_" + videoId + ".mp3");

        if (fs::exists(expectedMp3) && fs::file_size(expectedMp3) > 1024) {
            return expectedMp3.generic_string();
        }

        fs::path ytDlpPath = GetModulesDir() / YT_DLP_NAME;
        fs::path ffmpegDir = GetModulesDir();

        auto futureProgress = ExecuteProcedureWithProgressAsync(
            "Downloading Stream", "MP3 Conversion",
            [&](ProgressTracker& tracker) -> bool {
                std::string cmd = "\"" + ytDlpPath.generic_string() + "\" --no-warnings --no-playlist "
                                  "--ffmpeg-location \"" + ffmpegDir.generic_string() + "\" "
                                  "-x --audio-format mp3 --audio-quality 0 "
                                  "-o \"" + outputTemplatePath.generic_string() + ".%(ext)s\" \"" + cleanUrl + "\"";

                bool success = ExecuteYtDlpStream(cmd, tracker);

                tracker.isExtracting.store(true);
                tracker.extractCurrent.store(100.0);

                return success;
            }
        );

        futureProgress.get();

        std::string videoPrefix = "yt_" + videoId;
        std::error_code ec;
        if (fs::exists(absoluteCacheDir, ec)) {
            for (const auto& entry : fs::directory_iterator(absoluteCacheDir, ec)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename.rfind(videoPrefix, 0) == 0 && entry.path().extension() != ".mp3") {
                        fs::remove(entry.path(), ec);
                    }
                }
            }
        }

        if (fs::exists(expectedMp3) && fs::file_size(expectedMp3) > 1024) {
            return expectedMp3.generic_string();
        }

        return "";
    }
};

#endif
