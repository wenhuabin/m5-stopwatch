/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "moments_storage.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <esp_vfs_fat.h>
#include <mooncake_log.h>

namespace moments::storage {
namespace {

constexpr const char* _tag       = "MomentsStorage";
constexpr const char* _temp_name = "upload.tmp";

bool has_bmp_extension(const std::string& name)
{
    if (name.size() < 4) {
        return false;
    }
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext == ".bmp";
}

}  // namespace

std::string temp_upload_path()
{
    return std::string(kMomentsDir) + "/" + _temp_name;
}

void ensure_dir()
{
    if (mkdir(kMomentsDir, 0775) != 0 && errno != EEXIST) {
        mclog::tagError(_tag, "failed to create {}: errno={}", kMomentsDir, errno);
    }
    unlink(temp_upload_path().c_str());
}

std::string make_photo_path(uint32_t batch_id, std::size_t index)
{
    char path[96] = {};
    snprintf(path, sizeof(path), "%s/moment_%010u_%u.bmp", kMomentsDir, static_cast<unsigned>(batch_id),
              static_cast<unsigned>(index));
    return std::string(path);
}

bool read_bmp_dimensions(const std::string& path, int& width, int& height)
{
    FILE* file = fopen(path.c_str(), "rb");
    if (file == nullptr) {
        return false;
    }

    unsigned char header[26] = {};
    const std::size_t read_size = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (read_size != sizeof(header) || header[0] != 'B' || header[1] != 'M') {
        return false;
    }

    width  = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    height = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    return true;
}

std::vector<std::string> load_existing_photos()
{
    std::vector<std::string> photos;

    DIR* dir = opendir(kMomentsDir);
    if (dir == nullptr) {
        return photos;
    }

    std::vector<std::string> candidates;
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == ".." || name == _temp_name) {
            continue;
        }
        candidates.push_back(name);
    }
    closedir(dir);

    std::sort(candidates.begin(), candidates.end());

    for (const auto& name : candidates) {
        const std::string path = std::string(kMomentsDir) + "/" + name;

        int width  = 0;
        int height = 0;
        const bool valid = photos.size() < kMaxPhotos && has_bmp_extension(name) &&
                            read_bmp_dimensions(path, width, height) && width == kPhotoWidth &&
                            height == kPhotoHeight;

        if (valid) {
            photos.push_back(path);
        } else {
            mclog::tagWarn(_tag, "dropping invalid moments file: {}", path);
            unlink(path.c_str());
        }
    }

    return photos;
}

void delete_all_photos()
{
    DIR* dir = opendir(kMomentsDir);
    if (dir == nullptr) {
        return;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        unlink((std::string(kMomentsDir) + "/" + name).c_str());
    }
    closedir(dir);
}

long free_space_bytes()
{
    uint64_t total_bytes = 0;
    uint64_t free_bytes  = 0;
    if (esp_vfs_fat_info("/spiflash", &total_bytes, &free_bytes) != ESP_OK) {
        return -1;
    }
    return static_cast<long>(free_bytes);
}

}  // namespace moments::storage
