#pragma once

#include <string>
#include <vector>

// 一段可执行的行走指令：go_key 是 /go 的参数（出口目标注册名），
// arrival_name 是走完后所处房间的注册名（I=，全服唯一）。
struct MapStep {
    std::string go_key;
    std::string arrival_name;
};

// 与叛逆客户端 jx.dat 同结构的房间地图：段 [setting] 给总数，[房间号]
// 段给 Name1（注册名）/Num（出口数）/OutN（出口目标注册名）。文件里的
// Name2（显示名）不读：它全服允许重名，定位只认 Name1。出口目标名即
// /go 的导航键。
class RoomMap {
public:
    // 读 UTF-8 编码的地图文件；打不开或无房间返回 false。
    bool load(const std::string& path);
    void clear();
    bool empty() const { return rooms_.empty(); }
    size_t size() const { return rooms_.size(); }

    // 按注册名（Name1，即 door 名）查房间号，找不到返回 -1。
    // 显示名（Name2）不参与解析：全服允许重名，无法唯一定位。
    int find_room(const std::string& name) const;

    // 房间号求最短路径，返回逐段 /go 指令（不含起点；已在起点或
    // 不可达返回空）。字符串重载先按注册名定位两端再转调。
    std::vector<MapStep> find_path(int from, int to) const;
    std::vector<MapStep> find_path(const std::string& from,
                                   const std::string& to) const;

private:
    struct Room {
        std::string name1;
        std::vector<std::string> exits;
    };

    std::vector<Room> rooms_;
};
