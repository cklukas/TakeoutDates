#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <set>
#include <vector>
#include <sstream>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

#ifdef __APPLE__
#include "mac_tags.h"
#endif

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
 * Joins a vector of strings with a separator, escaping each element for CSV.
 * @param items The vector of strings to join.
 * @param separator The separator to use.
 * @return The joined string.
 */
std::string joinCSV(const std::vector<std::string> &items, const std::string &separator)
{
    std::string result;
    for (size_t i = 0; i < items.size(); ++i)
    {
        result += escapeCSV(items[i]);
        if (i < items.size() - 1)
            result += separator;
    }
    return escapeCSV(result);
}

/**
 * Prints the command-line usage help message.
 */
void printHelp()
{
    std::cout << "Usage: takeout_photos_date_setter <folder> [options]\n"
              << "Options:\n"
              << "  --help                    Display this help message\n"
              << "  --list                    List files with creation, upload times, and people as CSV\n"
              << "  --set-file-dates          Set file dates based on metadata\n"
#ifdef __APPLE__
              << "  --assign-people-tags \"tag1;...\" Assign specified Finder Tags from JSON 'people' names (macOS only, semicolon-separated)\n"
              << "  --assign-all-people-tags  Assign all 'people' names as Finder Tags (macOS only)\n"
              << "  --remove-all-tags         Remove all Finder Tags from files (macOS only)\n"
              << "  --remove-named-tags \"tag1;...\" Remove specific Finder Tags (macOS only, semicolon-separated)\n"
#endif
              << "  --list-tags               List unique 'people' names from JSON files\n";
}

/**
 * Sets the creation and modification times of a file (platform-specific).
 * @param filePath The path to the file.
 * @param photoTakenTime The timestamp for the creation time.
 * @param creationTime The timestamp for the modification time (upload time).
 * @return True if successful, false otherwise.
 */
bool setFileTimes(const fs::path &filePath, time_t photoTakenTime, time_t creationTime)
{
#ifdef _WIN32
    // Windows-specific: Use SetFileTime
    HANDLE hFile = CreateFileA(filePath.string().c_str(), GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open " << filePath << ": " << GetLastError() << std::endl;
        return false;
    }
    FILETIME ftCreation, ftModification;
    LONGLONG llCreation = Int32x32To64(photoTakenTime, 10000000) + 116444736000000000LL;
    LONGLONG llModification = Int32x32To64(creationTime, 10000000) + 116444736000000000LL;
    ftCreation.dwLowDateTime = (DWORD)llCreation;
    ftCreation.dwHighDateTime = (DWORD)(llCreation >> 32);
    ftModification.dwLowDateTime = (DWORD)llModification;
    ftModification.dwHighDateTime = (DWORD)(llModification >> 32);
    if (!SetFileTime(hFile, &ftCreation, NULL, &ftModification))
    {
        std::cerr << "Failed to set times for " << filePath << ": " << GetLastError() << std::endl;
        CloseHandle(hFile);
        return false;
    }
    CloseHandle(hFile);
    return true;
#else
    // POSIX (Linux/macOS)
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
#ifdef __APPLE__
    // macOS-specific: Set creation time
    if (!setCreationTime(filePath.string(), photoTakenTime))
    {
        close(fd);
        return false;
    }
#endif
    close(fd);
    return true;
#endif
}

/**
 * Processes a Google Photos metadata JSON file.
 * Supports .supplemental-metadata.json and .suppl.json suffixes.
 * Handles date setting, tag listing, and tag assignment/removal based on mode.
 * @param jsonPath Path to the metadata JSON file.
 * @param listOnly If true, lists files with times and people.
 * @param setDates If true, sets file dates.
 * @param listTags If true, lists unique people tags.
 * @param assignPeopleTags If true, assigns specified tags (macOS only).
 * @param peopleTagsToAssign Tags to assign (if assignPeopleTags is true).
 * @param assignAllPeopleTags If true, assigns all people names as tags (macOS only).
 * @param removeAllTags If true, removes all tags (macOS only).
 * @param removeNamedTags If true, removes specified tags (macOS only).
 * @param tagsToRemove Tags to remove (if removeNamedTags is true).
 * @param allPeopleTags Accumulates unique people tags (for --list-tags).
 */
void processFile(const fs::path &jsonPath, bool listOnly, bool setDates, bool listTags,
                 bool assignPeopleTags, const std::vector<std::string> &peopleTagsToAssign,
                 bool assignAllPeopleTags, bool removeAllTags, bool removeNamedTags,
                 const std::vector<std::string> &tagsToRemove, std::set<std::string> &allPeopleTags)
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
    std::string baseFileName;

    if (jsonFileName.find(".supplemental-metadata.json") != std::string::npos)
    {
        baseFileName = jsonFileName.substr(0, jsonFileName.find(".supplemental-metadata.json"));
    }
    else if (jsonFileName.find(".suppl.json") != std::string::npos)
    {
        baseFileName = jsonFileName.substr(0, jsonFileName.find(".suppl.json"));
    }
    else
    {
        return; // Not a recognized metadata file
    }

    fs::path parentDir = jsonPath.parent_path();
    fs::path primaryPath = parentDir / baseFileName;

    if (!fs::exists(primaryPath) && !listTags)
    {
        std::cerr << "Primary file " << primaryPath << " does not exist" << std::endl;
        return;
    }

    std::string primaryStem = primaryPath.stem().string();
    time_t photoTakenTime = std::stol(j["photoTakenTime"]["timestamp"].get<std::string>());
    time_t creationTime = std::stol(j["creationTime"]["timestamp"].get<std::string>());

    // Extract people names from JSON
    std::vector<std::string> peopleNames;
    if (j.contains("people") && j["people"].is_array())
    {
        for (const auto &person : j["people"])
        {
            if (person.contains("name") && person["name"].is_string())
            {
                peopleNames.push_back(person["name"].get<std::string>());
                if (listTags)
                {
                    allPeopleTags.insert(person["name"].get<std::string>());
                }
            }
        }
    }

    if (listOnly)
    {
        std::cout << escapeCSV(primaryPath.string()) << ","
                  << escapeCSV(formatTime(photoTakenTime)) << ","
                  << escapeCSV(formatTime(creationTime)) << ","
                  << joinCSV(peopleNames, ";") << "\n";

        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
        {
            std::cout << escapeCSV(mp4Path.string()) << ","
                      << escapeCSV(formatTime(photoTakenTime)) << ","
                      << escapeCSV(formatTime(creationTime)) << ","
                      << joinCSV(peopleNames, ";") << "\n";
        }
    }
    else if (setDates)
    {
        setFileTimes(primaryPath, photoTakenTime, creationTime);

        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
        {
            setFileTimes(mp4Path, photoTakenTime, creationTime);
        }

        fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
        fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
        fs::path mp4LowerSupplJsonPath = parentDir / (primaryStem + ".mp4.suppl.json");
        if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::exists(mp4LowerSupplJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
        {
            setFileTimes(mp4LowerPath, photoTakenTime, creationTime);
        }
    }
#ifdef __APPLE__
    else if (assignPeopleTags)
    {
        std::vector<std::string> tagsToApply;
        for (const auto &tag : peopleTagsToAssign)
        {
            if (std::find(peopleNames.begin(), peopleNames.end(), tag) != peopleNames.end())
            {
                tagsToApply.push_back(tag);
            }
        }
        if (!tagsToApply.empty())
        {
            setFinderTags(primaryPath.string(), tagsToApply);

            fs::path mp4Path = parentDir / (primaryStem + ".MP4");
            fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
            fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
            if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
            {
                setFinderTags(mp4Path.string(), tagsToApply);
            }

            fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
            fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
            fs::path mp4LowerSupplJsonPath = parentDir / (primaryStem + ".mp4.suppl.json");
            if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::exists(mp4LowerSupplJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
            {
                setFinderTags(mp4LowerPath.string(), tagsToApply);
            }
        }
    }
    else if (assignAllPeopleTags)
    {
        if (!peopleNames.empty())
        {
            setFinderTags(primaryPath.string(), peopleNames);

            fs::path mp4Path = parentDir / (primaryStem + ".MP4");
            fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
            fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
            if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
            {
                setFinderTags(mp4Path.string(), peopleNames);
            }

            fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
            fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
            fs::path mp4LowerSupplJsonPath = parentDir / (primaryStem + ".mp4.suppl.json");
            if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::exists(mp4LowerSupplJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
            {
                setFinderTags(mp4LowerPath.string(), peopleNames);
            }
        }
    }
    else if (removeAllTags)
    {
        removeAllFinderTags(primaryPath.string());

        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
        {
            removeAllFinderTags(mp4Path.string());
        }

        fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
        fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
        fs::path mp4LowerSupplJsonPath = parentDir / (primaryStem + ".mp4.suppl.json");
        if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::exists(mp4LowerSupplJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
        {
            removeAllFinderTags(mp4LowerPath.string());
        }
    }
    else if (removeNamedTags)
    {
        removeNamedFinderTags(primaryPath.string(), tagsToRemove);

        fs::path mp4Path = parentDir / (primaryStem + ".MP4");
        fs::path mp4JsonPath = parentDir / (primaryStem + ".MP4.supplemental-metadata.json");
        fs::path mp4SupplJsonPath = parentDir / (primaryStem + ".MP4.suppl.json");
        if (fs::exists(mp4Path) && !fs::exists(mp4JsonPath) && !fs::exists(mp4SupplJsonPath))
        {
            removeNamedFinderTags(mp4Path.string(), tagsToRemove);
        }

        fs::path mp4LowerPath = parentDir / (primaryStem + ".mp4");
        fs::path mp4LowerJsonPath = parentDir / (primaryStem + ".mp4.supplemental-metadata.json");
        fs::path mp4LowerSupplJsonPath = parentDir / (primaryStem + ".mp4.suppl.json");
        if (fs::exists(mp4LowerPath) && !fs::exists(mp4LowerJsonPath) && !fs::exists(mp4LowerSupplJsonPath) && !fs::equivalent(mp4LowerPath, mp4Path))
        {
            removeNamedFinderTags(mp4LowerPath.string(), tagsToRemove);
        }
    }
#endif
}

/**
 * Main function to parse command-line arguments and process Google Photos Takeout files.
 * Recognizes both .supplemental-metadata.json and .suppl.json metadata files.
 * Supports date setting, tag listing, and tag management (macOS only for tags).
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
    bool listTags = false;
    bool assignPeopleTags = false;
    bool assignAllPeopleTags = false;
    bool removeAllTags = false;
    bool removeNamedTags = false;
    std::vector<std::string> peopleTagsToAssign;
    std::vector<std::string> tagsToRemove;
    std::set<std::string> allPeopleTags;

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
        else if (arg == "--list-tags")
        {
            listTags = true;
        }
        else if (arg == "--assign-people-tags" && i + 1 < argc)
        {
            assignPeopleTags = true;
            std::string tagsArg = argv[++i];
            std::stringstream ss(tagsArg);
            std::string tag;
            while (std::getline(ss, tag, ';'))
            {
                if (!tag.empty())
                    peopleTagsToAssign.push_back(tag);
            }
        }
        else if (arg == "--assign-all-people-tags")
        {
            assignAllPeopleTags = true;
        }
        else if (arg == "--remove-all-tags")
        {
            removeAllTags = true;
        }
        else if (arg == "--remove-named-tags" && i + 1 < argc)
        {
            removeNamedTags = true;
            std::string tagsArg = argv[++i];
            std::stringstream ss(tagsArg);
            std::string tag;
            while (std::getline(ss, tag, ';'))
            {
                if (!tag.empty())
                    tagsToRemove.push_back(tag);
            }
        }
        else
        {
            std::cerr << "Unknown option or missing argument: " << arg << std::endl;
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
        std::cout << "File,PhotoTakenTime,UploadTime,People\n";
    }

    for (const auto &entry : fs::recursive_directory_iterator(folder))
    {
        std::string filename = entry.path().filename().string();
        if (entry.path().extension() == ".json" &&
            (filename.find(".supplemental-metadata.json") != std::string::npos ||
             filename.find(".suppl.json") != std::string::npos))
        {
            processFile(entry.path(), listOnly, setDates, listTags, assignPeopleTags, peopleTagsToAssign,
                        assignAllPeopleTags, removeAllTags, removeNamedTags, tagsToRemove, allPeopleTags);
        }
    }

    if (listTags)
    {
        std::cout << "Unique People Tags:\n";
        for (const auto &tag : allPeopleTags)
        {
            std::cout << tag << "\n";
        }
    }

    return 0;
}