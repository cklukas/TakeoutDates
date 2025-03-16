#include "mac_tags.h"
#include <iostream>  // Added for std::cerr and std::endl
#include <sys/attr.h>
#include <Foundation/Foundation.h>

bool setCreationTime(const std::string &path, time_t creationTime) {
    struct attrlist attrList = {0};
    struct timespec birthTime;

    attrList.bitmapcount = ATTR_BIT_MAP_COUNT;
    attrList.commonattr = ATTR_CMN_CRTIME;

    birthTime.tv_sec = creationTime;
    birthTime.tv_nsec = 0;

    int result = setattrlist(path.c_str(), &attrList, &birthTime, sizeof(birthTime), 0);
    if (result != 0) {
        std::cerr << "Failed to set creation time for " << path << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool setFinderTags(const std::string &filePath, const std::vector<std::string> &tags) {
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filePath.c_str()]];
    NSMutableArray *tagArray = [NSMutableArray array];
    for (const auto &tag : tags) {
        [tagArray addObject:[NSString stringWithUTF8String:tag.c_str()]];
    }
    NSError *error = nil;
    [url setResourceValue:tagArray forKey:NSURLTagNamesKey error:&error];
    if (error) {
        std::cerr << "Failed to set tags for " << filePath << ": " << [[error localizedDescription] UTF8String] << std::endl;
        return false;
    }
    return true;
}

bool removeAllFinderTags(const std::string &filePath) {
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filePath.c_str()]];
    NSError *error = nil;
    [url setResourceValue:@[] forKey:NSURLTagNamesKey error:&error];
    if (error) {
        std::cerr << "Failed to remove tags from " << filePath << ": " << [[error localizedDescription] UTF8String] << std::endl;
        return false;
    }
    return true;
}

bool removeNamedFinderTags(const std::string &filePath, const std::vector<std::string> &tagsToRemove) {
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:filePath.c_str()]];
    NSError *error = nil;
    NSArray *currentTags = nil;
    [url getResourceValue:&currentTags forKey:NSURLTagNamesKey error:&error]; // Fixed typo: ¤tTags -> &currentTags
    if (error) {
        std::cerr << "Failed to get tags for " << filePath << ": " << [[error localizedDescription] UTF8String] << std::endl;
        return false;
    }
    NSMutableArray *newTags = [currentTags mutableCopy];
    for (const auto &tag : tagsToRemove) {
        [newTags removeObject:[NSString stringWithUTF8String:tag.c_str()]];
    }
    [url setResourceValue:newTags forKey:NSURLTagNamesKey error:&error];
    if (error) {
        std::cerr << "Failed to remove named tags from " << filePath << ": " << [[error localizedDescription] UTF8String] << std::endl;
        return false;
    }
    return true;
}