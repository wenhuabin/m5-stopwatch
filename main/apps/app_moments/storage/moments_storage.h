/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace moments::storage {

constexpr const char* kMomentsDir = "/spiflash/moments";
constexpr std::size_t kMaxPhotos  = 5;
constexpr int kPhotoWidth         = 466;
constexpr int kPhotoHeight        = 466;

// Creates the moments directory if missing and clears any leftover
// in-progress upload temp file.
void ensure_dir();

// Returns up to kMaxPhotos valid (exact-dimension BMP) photo paths,
// sorted by filename. Any other file found in the directory is treated
// as stale/corrupt and deleted.
std::vector<std::string> load_existing_photos();

// Deletes every file in the moments directory.
void delete_all_photos();

// Absolute path a photo at `index` within batch `batch_id` should be
// stored at.
std::string make_photo_path(uint32_t batch_id, std::size_t index);

// Path used to stream an in-progress upload before it's renamed into
// place.
std::string temp_upload_path();

// Reads just the BMP header (26 bytes) to get width/height without
// loading the whole file. Returns false if `path` isn't a valid BMP.
bool read_bmp_dimensions(const std::string& path, int& width, int& height);

// Free bytes on the partition backing kMomentsDir, or -1 if it can't be
// determined.
long free_space_bytes();

}  // namespace moments::storage
