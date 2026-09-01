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

namespace fs = std::filesystem;

#if defined(_WIN32) || defined(_WIN64)
    #define YT_OS_WINDOWS
    #define YT_EXE_EXT ".exe"
    #define YT_DEV_NULL "NUL"
    #include <windows.h>
#elif defined(__APPLE__)
    #define YT_OS_MACOS
    #define YT_EXE_EXT ""
    #define YT_DEV_NULL "/dev/null"
#else
    #define YT_OS_LINUX
    #define YT_EXE_EXT ""
    #define YT_DEV_NULL "/dev/null"
#endif

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
        if (isExtracting.load()) return; // Freeze download stats during extraction

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
        #if defined(YT_OS_WINDOWS)
            static bool enabled = false;
            if (!enabled) {
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

    // 3D Depth Bar: Solid Cyan blocks for filled portion, Dark Cyan background for track
    static std::string BuildBar(double current, double total, int width = 24) {
        double ratio = (total > 0) ? (current / total) : 0.0;
        ratio = std::clamp(ratio, 0.0, 1.0);

        double totalCells = ratio * width;
        int fullBlocks = static_cast<int>(totalCells);
        bool hasHalfBlock = (totalCells - fullBlocks) >= 0.5;
        if (fullBlocks == width) hasHalfBlock = false;

        int emptyBlocks = width - fullBlocks - (hasHalfBlock ? 1 : 0);

        std::ostringstream bar;

        // Left Cap: Heavy Pipe (┃)
        bar << "\033[38;5;242m┃\033[0m";

        // Solid Bright Cyan Filled Portion (Foreground 39 / Background 39)
        bar << "\033[38;5;39m\033[48;5;39m";
        for (int i = 0; i < fullBlocks; ++i) {
            bar << "█";
        }

        // Sub-block Transition Edge (Bright Cyan FG 39 / Dark Cyan BG 24)
        if (hasHalfBlock) {
            bar << "\033[38;5;39m\033[48;5;24m▌";
        }

        // Unfilled Track (Dark Cyan Background 24)
        bar << "\033[48;5;24m";
        for (int i = 0; i < emptyBlocks; ++i) {
            bar << " ";
        }

        bar << "\033[0m"; // Reset ANSI attributes

        // Right Cap: Heavy Pipe (┃)
        bar << "\033[38;5;242m┃\033[0m";

        return bar.str();
    }

    static void Render(const ProgressTracker& tracker, std::chrono::steady_clock::time_point startTime) {
        EnableVTMode();

        static int prevLines = 1;

        // Move cursor up and clear previously rendered lines to prevent vertical line stacking
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

        // Download Line
        std::cout << "[" << std::left << std::setw(18) << tracker.downloadLabel << "] "
                  << dlBarStr << " "
                  << std::right << std::setw(5) << std::fixed << std::setprecision(1) << (dlRatio * 100.0) << "% | "
                  << FormatSize(dlCur) << " / " << FormatSize(dlTot)
                  << " | Speed: " << FormatSize(speed) << "/s"
                  << " | ETA: " << FormatTime(eta);

        // Conversion Line
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
        #if defined(YT_OS_WINDOWS)
            std::string wrapped = "\"" + cmd + "\"";
            return system(wrapped.c_str());
        #else
            return system(cmd.c_str());
        #endif
    }

    static bool ExecuteYtDlpStream(const std::string& cmd, ProgressTracker& tracker) {
        std::string pipeCmd = cmd + " --newline --progress-template \"dl_data:%(progress.downloaded_bytes)s/%(progress.total_bytes)s/%(progress.total_bytes_estimate)s\"";

        #if defined(YT_OS_WINDOWS)
            FILE* pipe = _popen(pipeCmd.c_str(), "r");
        #else
            FILE* pipe = popen(pipeCmd.c_str(), "r");
        #endif

        if (!pipe) return false;

        char buffer[512];
        std::regex templateRegex(R"(dl_data:(\d+|NA|none)/(\d+|NA|none)/(\d+|NA|none))");
        std::regex legacyRegex(R"(\[download\]\s+([\d\.]+)%\s+of\s+~?\s*([\d\.]+)\s*(\w+))");
        std::smatch match;

        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);

            // Detect transition into post-processing (extraction / ffmpeg encoding)
            if (line.find("[ExtractAudio]") != std::string::npos || line.find("[ffmpeg]") != std::string::npos) {
                if (!tracker.isExtracting.load()) {
                    // Lock download bar at 100% when extraction starts
                    double finalTot = tracker.downloadTotal.load();
                    if (finalTot <= 0) finalTot = 1.0;
                    tracker.downloadCurrent.store(finalTot);
                    tracker.isExtracting.store(true);
                }
            }

            // Only update download byte counts if extraction has NOT started yet
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

        #if defined(YT_OS_WINDOWS)
            int status = _pclose(pipe);
        #else
            int status = pclose(pipe);
        #endif

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
            return "";
        }

        fs::path outputTemplatePath = absoluteCacheDir / ("yt_" + videoId);
        fs::path expectedMp3 = absoluteCacheDir / ("yt_" + videoId + ".mp3");

        if (fs::exists(expectedMp3) && fs::file_size(expectedMp3) > 1024) {
            return expectedMp3.string();
        }

        fs::path ytDlpPath = GetModulesDir() / ("yt-dlp" YT_EXE_EXT);
        fs::path ffmpegDir = GetModulesDir();

        auto futureProgress = ExecuteProcedureWithProgressAsync(
            "Downloading Stream", "MP3 Conversion",
            [&](ProgressTracker& tracker) -> bool {
                std::string cmd = "\"" + ytDlpPath.string() + "\" --no-warnings --no-playlist "
                                  "--ffmpeg-location \"" + ffmpegDir.string() + "\" "
                                  "-x --audio-format mp3 --audio-quality 0 "
                                  "-o \"" + outputTemplatePath.string() + ".%(ext)s\" \"" + inputUrl + "\"";

                bool success = ExecuteYtDlpStream(cmd, tracker);

                tracker.isExtracting.store(true);
                tracker.extractCurrent.store(100.0);

                return success;
            }
        );

        futureProgress.get();

        // Cleanup temporary intermediate files (.webm, .m4a, .part, .ytdl)
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
            return expectedMp3.string();
        }

        return "";
    }
};

#endif
