#include "room_map.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <queue>
#include <utility>

namespace {

std::string trim(const std::string& s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return std::string();
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

bool all_digits(const std::string& s)
{
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

struct ParsedRoom {
    std::string name1;
    std::string name2;
    std::vector<std::pair<int, std::string>> exits;
};

}  // namespace

bool RoomMap::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::vector<ParsedRoom> parsed;
    int current = -1;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] == '[') {
            size_t close = line.find(']');
            if (close == std::string::npos) continue;
            std::string section = trim(line.substr(1, close - 1));
            if (all_digits(section)) {
                current = static_cast<int>(parsed.size());
                parsed.emplace_back();
            }
            else {
                current = -1;
            }
            continue;
        }
        if (current < 0) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "Name1") {
            parsed[current].name1 = val;
        }
        else if (key == "Name2") {
            parsed[current].name2 = val;
        }
        else if (key.rfind("Out", 0) == 0 && all_digits(key.substr(3))) {
            int index = std::stoi(key.substr(3));
            parsed[current].exits.emplace_back(index, val);
        }
    }

    rooms_.clear();
    for (auto& item : parsed) {
        std::sort(
            item.exits.begin(), item.exits.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        Room room;
        room.name1 = std::move(item.name1);
        room.name2 = std::move(item.name2);
        for (auto& exit : item.exits)
            room.exits.push_back(std::move(exit.second));
        rooms_.push_back(std::move(room));
    }
    return !rooms_.empty();
}

void RoomMap::clear() { rooms_.clear(); }

int RoomMap::find_room(const std::string& name) const
{
    std::string wanted = trim(name);
    if (wanted.empty()) return -1;
    for (size_t i = 0; i < rooms_.size(); ++i) {
        if (rooms_[i].name1 == wanted || rooms_[i].name2 == wanted) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::vector<MapStep> RoomMap::find_path(int from, int to) const
{
    if (from < 0 || to < 0 || from >= static_cast<int>(rooms_.size())
        || to >= static_cast<int>(rooms_.size()) || from == to) {
        return {};
    }

    const int n = static_cast<int>(rooms_.size());
    std::vector<int> prev(n, -1);
    std::vector<std::string> prev_key(n);
    std::vector<char> visited(n, 0);
    std::queue<int> pending;
    visited[from] = 1;
    pending.push(from);

    while (!pending.empty() && !visited[to]) {
        int room = pending.front();
        pending.pop();
        for (const auto& key : rooms_[room].exits) {
            int next = find_room(key);
            if (next < 0 || visited[next]) continue;
            visited[next]  = 1;
            prev[next]     = room;
            prev_key[next] = key;
            pending.push(next);
        }
    }
    if (!visited[to]) return {};

    std::vector<MapStep> steps;
    for (int at = to; at != from; at = prev[at]) {
        const Room& room = rooms_[at];
        steps.push_back({prev_key[at], {room.name1, room.name2}});
    }
    std::reverse(steps.begin(), steps.end());
    return steps;
}

std::vector<MapStep> RoomMap::find_path(const std::string& from,
                                        const std::string& to) const
{
    return find_path(find_room(from), find_room(to));
}
