#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <ctime>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/attr.h>
#endif
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * Formats a time_t value as "YYYY-MM-DD HH:MM:SS" in UTC.
 * @param time The Unix timestamp to format.
 * @return A string representation of the time, or "Invalid Time" if formatting fails.
 */
std::string formatTime(time_t time)
{
    std::tm *tm = std::gmtime(&time);
    if (!tm)
        return "Invalid Time";
    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buffer);
}

/**
 * Escapes a string for CSV output by wrapping it in quotes if it contains commas, quotes, or newlines.
 * @param input The string to escape.
 * @return The escaped string.
 */
std::string escapeCSV(const std::string &input)
{
    if (input.find_first_of(",\"\n") == std::string::npos)
    {
        return input;
    }
    std::string escaped = "\"";
    for (char c : input)
    {
        if (c == '"')
            escaped += "\"\"";
        else
            escaped += c;
    }
    escaped += "\"";
    return escaped;
}

/**
 * Prints the command-line usage help message.
 */
void printHelp()
{
    std::cout << "Usage: google_photos_date_setter <folder> [options]\n"
              << "Options:\n"
              << "  --help           Display this help message\n"
              << "  --list           List files with creation and upload times as CSV\n"
              << "  --set-file-dates Set file dates based on metadata\n";
}

#ifdef __APPLE__
/**
 * Sets the creation time (birth time) of a file on macOS using setattrlist.
 * @param path The file path.
 * @param creationTime The Unix timestamp to set as the creation time.
 * @return True if successful, false otherwise.
 */
bool setCreationTime(const std::string &path, time_t creationTime)
{
    struct attrlist attrList = {0};
    struct timespec birthTime;

    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.commonattr = ATTR_CMN_CRTIME;

    birthTime.tv_sec = creationTime;
    birthTime.tv_nsec = 0;

    int result = setattrlist(path.c_str(), &attrList, &birthTime, sizeof(birthTime), 0);
    if (result != 0)
    {
        std::cerr << "Failed to set creation time for " << path << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}
#endif

/**
 * Sets the creation and modification times of a file.
 * - Creation time (photoTakenTime) is set as the birth time (macOS only).
 * - Modification time (creationTime) is set as the mtime.
 * @param filePath The path to the file.
 * @param photoTakenTime The timestamp for the creation time.
 * @param creationTime The timestamp for the modification time (upload time).
 * @return True if successful, false otherwise.
 */
bool setFileTimes(const fs::path &filePath, time_t photoTakenTime, time_t creationTime)
{
    struct timespec times[2];
    times[0].tv_sec = UTIME_OMIT; // Leave access time unchanged
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = creationTime; // Modification time (upload time)
    times[1].tv_nsec = 0;

    int fd = open(filePath.string().c_str(), O_WRONLY);
    if (fd == -1)
    {
        std::cerr << "Failed to open " << filePath << ": " << strerror(errno) << std::endl;
        return false;
    }

    if (utimensat(AT_FDCWD, filePath.string().c_str(), times, 0) != 0)
    {
        std::cerr << "Failed to set modification time for " << filePath << ": " << strerror(errno) << std::endl;
        close(fd);
        return false;
    }
    close(fd);

#ifdef __APPLE__
    if (!setCreationTime(filePath.string(), photoTakenTime))
    {
        return false;
    }
#endif

    return true;
}

/**
 * Processes a Google Photos metadata JSON file, updating associated media files’ timestamps.
 * Updates the primary file (e.g., IMG_7014.HEIC) and an optional .MP4 file (e.g., IMG_7014.MP4)
 * if it exists and lacks its own metadata. For --list, includes primary file and one .MP4 file in CSV output.
 * @param jsonPath Path to the .supplemental-metadata.json file.
 * @param listOnly If true, outputs CSV instead of setting dates.
 * @param setDates If true, sets file dates based on metadata.
 */
void processFile(const fs::path &jsonPath, bool listOnly, bool setDates)
{
    std::ifstream jsonFile(jsonPath);
    if (!jsonFile.is_open())
        return;

    json j;
    try
    {
        jsonFile >> j;
    }
    catch (const json::exception &e)
    {
        std::cerr << "Error parsing JSON " << jsonPath << ": " << e.what() << std::endl;
        return;
    }

    std::string jsonFileName = jsonPath.filename().string();
    std::string baseFileName = jsonFileName.substr(0, jsonFileName.find(".supplemental-metadata.json"));
    fs::path parentDir = jsonPath.parent_path();
    fs::path primaryPath = parentDir / baseFileName;

    if (!fs::exists(primaryPath))
    {
        std::cerr << "Primary file " << primaryPath << " does not exist" << std::endl;
        return;
    }

    std::string primaryStem = primaryPath.stem().string();
    time_t photoTakenTime = std::stol(j["photoTakenTime"]["timestamp"].get<std::string>());
    time_t creationTime = std::stol(j["creationTime"]["timestamp"].get<std::string>());

    if (listOnly)
    {
        // List the primary file
        std::cout << escapeCSV(primaryPath.string()) << ","
                  << escapeCSV(formatTime(photoTakenTime)) << ","
                  << escapeCSV(formatTime(creationTime)) << "\n";

        // List associated .MP4 file if it exists and has no own metadata
        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath))
        {
            std::cout << escapeCSV(mp4Path.string()) << ","
                      << escapeCSV(formatTime(photoTakenTime)) << ","
                      << escapeCSV(formatTime(creationTime)) << "\n";
        }
    }
    else if (setDates)
    {
        // Update the primary file
        setFileTimes(primaryPath, photoTakenTime, creationTime);

        // Update associated .MP4 file if it exists and has no own metadata
        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath))
        {
            setFileTimes(mp4Path, photoTakenTime, creationTime);
        }

        // Check lowercase .mp4 extension for case-insensitive file systems, only if distinct
        fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
        fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
        if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
        {
            setFileTimes(mp4LowerPath, photoTakenTime, creationTime);
        }
    }
}

/**
 * Main function to parse command-line arguments and process Google Photos Takeout files.
 * @param argc Number of arguments.
 * @param argv Argument array.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printHelp();
        return 1;
    }

    std::string folder = argv[1];
    bool listOnly = false;
    bool setDates = false;

    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--help")
        {
            printHelp();
            return 0;
        }
        else if (arg == "--list")
        {
            listOnly = true;
        }
        else if (arg == "--set-file-dates")
        {
            setDates = true;
        }
        else
        {
            std::cerr << "Unknown option: " << arg << std::endl;
            printHelp();
            return 1;
        }
    }

    if (!fs::exists(folder))
    {
        std::cerr << "Folder does not exist: " << folder << std::endl;
        return 1;
    }

    if (listOnly)
    {
        std::cout << "File,PhotoTakenTime,UploadTime\n";
    }

    for (const auto &entry : fs::recursive_directory_iterator(folder))
    {
        if (entry.path().extension() == ".json" &&
            entry.path().filename().string().find(".supplemental-metadata.json") != std::string::npos)
        {
            processFile(entry.path(), listOnly, setDates);
        }
    }

    return 0;
}