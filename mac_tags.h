#ifndef MAC_TAGS_H
#define MAC_TAGS_H

#include <string>
#include <vector>

bool setCreationTime(const std::string &path, time_t creationTime);
bool setFinderTags(const std::string &filePath, const std::vector<std::string> &tags);
bool removeAllFinderTags(const std::string &filePath);
bool removeNamedFinderTags(const std::string &filePath, const std::vector<std::string> &tagsToRemove);

#endif