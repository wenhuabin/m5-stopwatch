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

namespace todo::storage {

constexpr const char* kTodoDir       = "/spiflash/todo";
constexpr std::size_t kMaxItems      = 100;
constexpr std::size_t kMaxTextLength = 120;

struct TodoItem {
    uint32_t id          = 0;
    std::string text;
    bool completed        = false;
    // Monotonic sequence number assigned when the item is checked; used to
    // order the completed section by check time. 0 while still active.
    uint32_t checked_seq  = 0;
};

void ensure_dir();

// Returns all items ordered for display: active items by id ascending
// (creation order, oldest first), followed by completed items by
// checked_seq ascending (earliest checked first).
std::vector<TodoItem> load_items();

// Adds a new item and returns its id, or 0 if `text` is empty, too long,
// or the list is already at kMaxItems.
uint32_t add_item(const std::string& text);

// Replaces an item's text. Returns false if `id` doesn't exist or `text`
// is empty/too long.
bool edit_item(uint32_t id, const std::string& text);

// Removes an item. Returns false if `id` doesn't exist.
bool delete_item(uint32_t id);

// Flips an item between active and completed. Assigns a fresh
// checked_seq when transitioning to completed; clears it when
// transitioning back to active. Returns false if `id` doesn't exist.
bool toggle_item(uint32_t id);

}  // namespace todo::storage
