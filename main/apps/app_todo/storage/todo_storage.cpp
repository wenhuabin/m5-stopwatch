/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "todo_storage.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <sys/stat.h>
#include <unistd.h>

#include <ArduinoJson.h>
#include <mooncake_log.h>

namespace todo::storage {
namespace {

constexpr const char* _tag        = "TodoStorage";
constexpr const char* _items_path = "/spiflash/todo/items.json";

struct State {
    std::vector<TodoItem> items;
    uint32_t next_id           = 1;
    uint32_t next_checked_seq  = 1;
};

std::string read_whole_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr) {
        return {};
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return {};
    }
    const long size = ftell(file);
    if (size <= 0) {
        fclose(file);
        return {};
    }
    rewind(file);

    std::string content(static_cast<std::size_t>(size), '\0');
    const std::size_t read_size = fread(content.data(), 1, content.size(), file);
    fclose(file);
    content.resize(read_size);
    return content;
}

State load_state()
{
    State state;

    const std::string content = read_whole_file(_items_path);
    if (content.empty()) {
        return state;
    }

    JsonDocument doc;
    if (deserializeJson(doc, content) != DeserializationError::Ok) {
        mclog::tagWarn(_tag, "failed to parse {}, starting fresh", _items_path);
        return state;
    }

    state.next_id          = doc["next_id"] | 1;
    state.next_checked_seq = doc["next_checked_seq"] | 1;

    for (JsonObject item : doc["items"].as<JsonArray>()) {
        TodoItem parsed;
        parsed.id          = item["id"] | 0;
        parsed.text         = item["text"] | "";
        parsed.completed    = item["completed"] | false;
        parsed.checked_seq  = item["checked_seq"] | 0;
        if (parsed.id != 0 && !parsed.text.empty()) {
            state.items.push_back(parsed);
        }
    }

    return state;
}

bool save_state(const State& state)
{
    JsonDocument doc;
    doc["next_id"]           = state.next_id;
    doc["next_checked_seq"]  = state.next_checked_seq;

    JsonArray items_array = doc["items"].to<JsonArray>();
    for (const auto& item : state.items) {
        JsonObject item_obj      = items_array.add<JsonObject>();
        item_obj["id"]           = item.id;
        item_obj["text"]         = item.text;
        item_obj["completed"]    = item.completed;
        item_obj["checked_seq"]  = item.checked_seq;
    }

    std::string output;
    serializeJson(doc, output);

    const std::string temp_path = std::string(_items_path) + ".tmp";
    FILE* file                  = fopen(temp_path.c_str(), "wb");
    if (file == nullptr) {
        mclog::tagError(_tag, "failed to open {} for writing", temp_path);
        return false;
    }
    const std::size_t written = fwrite(output.data(), 1, output.size(), file);
    fclose(file);
    if (written != output.size()) {
        unlink(temp_path.c_str());
        mclog::tagError(_tag, "failed to write {}", temp_path);
        return false;
    }

    if (rename(temp_path.c_str(), _items_path) != 0) {
        unlink(temp_path.c_str());
        mclog::tagError(_tag, "failed to finalize {}", _items_path);
        return false;
    }

    return true;
}

}  // namespace

void ensure_dir()
{
    if (mkdir(kTodoDir, 0775) != 0 && errno != EEXIST) {
        mclog::tagError(_tag, "failed to create {}: errno={}", kTodoDir, errno);
    }
}

std::vector<TodoItem> load_items()
{
    std::vector<TodoItem> items = load_state().items;

    std::stable_sort(items.begin(), items.end(), [](const TodoItem& a, const TodoItem& b) {
        if (a.completed != b.completed) {
            return !a.completed;  // active items first
        }
        if (!a.completed) {
            return a.id < b.id;  // active: creation order, oldest first
        }
        return a.checked_seq < b.checked_seq;  // completed: earliest checked first
    });

    return items;
}

uint32_t add_item(const std::string& text)
{
    if (text.empty() || text.size() > kMaxTextLength) {
        return 0;
    }

    State state = load_state();
    if (state.items.size() >= kMaxItems) {
        return 0;
    }

    TodoItem item;
    item.id   = state.next_id++;
    item.text = text;

    state.items.push_back(item);
    if (!save_state(state)) {
        return 0;
    }

    return item.id;
}

bool edit_item(uint32_t id, const std::string& text)
{
    if (text.empty() || text.size() > kMaxTextLength) {
        return false;
    }

    State state = load_state();
    for (auto& item : state.items) {
        if (item.id == id) {
            item.text = text;
            return save_state(state);
        }
    }

    return false;
}

bool delete_item(uint32_t id)
{
    State state          = load_state();
    const std::size_t before = state.items.size();
    state.items.erase(std::remove_if(state.items.begin(), state.items.end(),
                                     [id](const TodoItem& item) { return item.id == id; }),
                      state.items.end());
    if (state.items.size() == before) {
        return false;
    }

    return save_state(state);
}

bool toggle_item(uint32_t id)
{
    State state = load_state();
    for (auto& item : state.items) {
        if (item.id == id) {
            item.completed = !item.completed;
            item.checked_seq = item.completed ? state.next_checked_seq++ : 0;
            return save_state(state);
        }
    }

    return false;
}

}  // namespace todo::storage
