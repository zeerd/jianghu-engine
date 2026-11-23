#pragma once

#include <string>
#include <vector>

// 一段可执行的行走指令：go_key 是 /go 的参数，arrival_names 是走完后
// 所处房间的注册名与显示名（两者都可能被服务端下发作房间标题）。
struct MapStep {
    std::string go_key;
    std::vector<std::string> arrival_names;
};

// 与叛逆客户端 jx.dat 同结构的房间地图：段 [setting] 给总数，[房间号]
// 段给 Name1（注册名）/Name2（显示名）/Num（出口数）/OutN（出口目标
// 注册名）。出口目标名即 /go 的导航键。
class RoomMap {
public:
    // 读 UTF-8 编码的地图文件；打不开或无房间返回 false。
    bool load(const std::string& path);
    void clear();
    bool empty() const { return rooms_.empty(); }
    size_t size() const { return rooms_.size(); }

    // 按注册名或显示名查房间号，找不到返回 -1。
    int find_room(const std::string& name) const;

    // 房间号求最短路径，返回逐段 /go 指令（不含起点；已在起点或
    // 不可达返回空）。名字重载在同 Name1/Name2 上定位后转调。
    std::vector<MapStep> find_path(int from, int to) const;
    std::vector<MapStep> find_path(const std::string& from,
                                   const std::string& to) const;

private:
    struct Room {
        std::string name1;
        std::string name2;
        std::vector<std::string> exits;
    };

    std::vector<Room> rooms_;
};
