#include "bot.h"

#include <chrono>
#include <thread>
#include <vector>

#include "client_core.h"
#include "color.h"
#include "game_state.h"

namespace bot {

Action decide(const WorldView& w, std::mt19937& rng)
{
    // 硬门槛在权重表之前：这四种状态下任何动作要么被服务端拒绝、要么
    // 白白计入限流，统一 Wait（战斗由服务端回合自行推进，bot 让开通道）。
    if (!w.logged_in || w.in_battle || w.busy || w.hp <= 0) {
        return Action::Wait;
    }

    struct Entry {
        Action act;
        int weight;
        bool available;
    };

    const bool hp_ok
        = w.max_hp > 0 && w.hp * 5 > w.max_hp * 2;  // 血过四成才开新战
    const Entry table[] = {
        {Action::Move, 25, w.exit_count > 0},
        {Action::Look, 10, true},
        {Action::Say, 10, true},
        {Action::Chat, 8, true},
        {Action::Ask, 8, w.npc_count > 0},
        {Action::Fight, 3, w.npc_count > 0 && hp_ok},
        {Action::Get, 6, w.ground_item_count > 0},
        {Action::Probe, 5, w.npc_count > 0},
        {Action::Sell, 4, w.vendor_item_count > 0 && w.inventory_count > 0},
        {Action::Buy, 3, w.vendor_item_count > 0},
    };

    int total = 0;
    for (const auto& e : table) {
        if (e.available) total += e.weight;
    }
    // 一个可用动作都没有（极端场景：空房 + 空出口）→ Wait 兜底。
    if (total <= 0) return Action::Wait;

    int roll = std::uniform_int_distribution<int>(1, total)(rng);
    for (const auto& e : table) {
        if (!e.available) continue;
        roll -= e.weight;
        if (roll <= 0) return e.act;
    }
    return Action::Wait;
}

MenuChoice decide_menu(const WorldView& w, int menu_rounds)
{
    // 连续 6 轮仍陷在菜单态：Cancel 兜底（本地关菜单，不发包、不计限流）。
    if (menu_rounds >= 6) return MenuChoice::Cancel;
    if (w.waiting_menu_input) return MenuChoice::Input;
    if (w.in_menu && w.menu_item_count > 0) return MenuChoice::Select;
    return MenuChoice::Cancel;
}

int next_delay_ms(std::mt19937& rng, int min_ms, int max_ms)
{
    return std::uniform_int_distribution<int>(min_ms, max_ms)(rng);
}

int menu_delay_ms(std::mt19937& rng, int min_ms, int max_ms)
{
    return std::uniform_int_distribution<int>(min_ms, max_ms)(rng);
}

}  // namespace bot

namespace {

// 闲聊语料（UTF-8，发送前由 Connection 统一转 GBK）。
const std::vector<std::string> kSayPool = {
    "你好", "今天天气不错", "有人组队吗", "路过大侠请了",
    "哈哈", "江湖路远",     "刚练完功",   "此地甚好",
};

const std::vector<std::string> kChatPool = {
    "收徒啦",
    "出售装备",
    "新人求带",
    "世界之大无奇不有",
};

const std::vector<std::string> kAskPool = {
    "请问",
    "这儿怎么走",
    "有什么消息吗",
    "最近还好吗",
};

// 自由文本菜单回灌的无害词（菜单 $N 占位符补位用）。
const std::vector<std::string> kMenuInputPool = {"无", "嗯", "算了", "再见"};

template <typename T>
const T& pick(const std::vector<T>& v, std::mt19937& rng)
{
    return v[std::uniform_int_distribution<size_t>(0, v.size() - 1)(rng)];
}

// 从 GameState 抽出决策快照。bot 主循环每轮调一次。
bot::WorldView snapshot(const GameState& s)
{
    bot::WorldView w;
    w.logged_in          = s.is_logged_in();
    w.in_battle          = s.in_battle();
    w.busy               = s.player_attr().busy;
    w.in_menu            = s.in_menu();
    w.waiting_menu_input = s.is_waiting_for_menu_input();
    w.menu_item_count    = static_cast<int>(s.menu_items().size());
    w.exit_count         = static_cast<int>(s.exits().size());
    w.npc_count          = static_cast<int>(s.room_npcs().size());
    w.ground_item_count  = static_cast<int>(s.room_items().size());
    w.inventory_count    = static_cast<int>(s.inventory().size());
    w.vendor_item_count  = static_cast<int>(s.vendor_items().size());
    w.hp                 = s.player_attr().hp;
    w.max_hp             = s.player_attr().max_hp;
    return w;
}

// /t 定位：NPC/物品用会话对象号（find_target 数字路径）。
void set_target(ClientCore& core, ObjectID id)
{
    core.input().process_line("/t " + std::to_string(id));
}

// 执行一个行动。返回值 true=本轮发了命令（供日志区分）。
// vendor_id：已探明的商人会话号（0=还没探到）；Sell/Buy 直接定向它，
// 不受其后 /t 漂移影响。
bool execute(ClientCore& core, bot::Action act, std::mt19937& rng,
             ObjectID vendor_id)
{
    GameState& s = core.state();
    auto& input  = core.input();

    switch (act) {
        case bot::Action::Wait:
            return false;

        case bot::Action::Move: {
            const auto& exits = s.exits();
            if (exits.empty()) return false;
            const auto& d = exits[std::uniform_int_distribution<size_t>(
                0, exits.size() - 1)(rng)];
            const std::string key
                = d.door_key.empty() ? d.dir_label : d.door_key;
            if (key.empty()) return false;
            input.process_line("/go " + key);
            core.ui().add_line(color::convert("$CYN[bot] go $COM" + key));
            return true;
        }

        case bot::Action::Look:
            input.process_line("/look");
            return true;

        case bot::Action::Say: {
            const std::string& text = pick(kSayPool, rng);
            input.process_line("/say " + text);
            return true;
        }

        case bot::Action::Chat: {
            // 纯文本走当前频道（GameState 缺省 chat）。
            input.process_line(pick(kChatPool, rng));
            return true;
        }

        case bot::Action::Ask: {
            const auto npcs = s.room_npcs();
            if (npcs.empty()) return false;
            set_target(core, npcs[std::uniform_int_distribution<size_t>(
                                      0, npcs.size() - 1)(rng)]
                                 .id);
            input.process_line("/ask " + pick(kAskPool, rng));
            return true;
        }

        case bot::Action::Fight: {
            const auto npcs = s.room_npcs();
            if (npcs.empty()) return false;
            set_target(core, npcs[std::uniform_int_distribution<size_t>(
                                      0, npcs.size() - 1)(rng)]
                                 .id);
            input.process_line("/fight");
            core.ui().add_line(color::convert("$CYN[bot] fight$COM"));
            return true;
        }

        case bot::Action::Get: {
            const auto items = s.room_items();
            if (items.empty()) return false;
            set_target(core, items[std::uniform_int_distribution<size_t>(
                                       0, items.size() - 1)(rng)]
                                 .id);
            input.process_line("/get");
            return true;
        }

        case bot::Action::Probe: {
            const auto npcs = s.room_npcs();
            if (npcs.empty()) return false;
            set_target(core, npcs[std::uniform_int_distribution<size_t>(
                                      0, npcs.size() - 1)(rng)]
                                 .id);
            input.process_line("/list 0");
            return true;
        }

        case bot::Action::Sell: {
            if (vendor_id == 0) return false;
            const auto& inv = s.inventory();
            if (inv.empty()) return false;
            const auto& item = inv[std::uniform_int_distribution<size_t>(
                0, inv.size() - 1)(rng)];
            set_target(core, vendor_id);
            const std::string key
                = item.internal_id.empty() ? item.name : item.internal_id;
            if (key.empty()) return false;
            input.process_line("/sell " + key);
            return true;
        }

        case bot::Action::Buy: {
            if (vendor_id == 0) return false;
            const auto& goods = s.vendor_items();
            if (goods.empty()) return false;
            const auto& good = goods[std::uniform_int_distribution<size_t>(
                0, goods.size() - 1)(rng)];
            set_target(core, vendor_id);
            input.process_line("/buy " + std::to_string(good.code));
            return true;
        }
    }
    return false;
}

int sleep_ms_for(const bot::WorldView& w, std::mt19937& rng)
{
    // 濒死等待回血（heal_up 每拍结算），间隔放宽，少打无效命令。
    if (w.hp <= 0) return 3000;
    return bot::next_delay_ms(rng);
}

}  // namespace

int run_bot(ClientCore& core)
{
    std::mt19937 rng(std::random_device{}());
    GameState& s = core.state();

    // 等自动登录落地（配置里的 account/password 在 start() 已发出）。
    // 30 秒没等到视为凭据/网络问题，交调用方决定是否重试。
    auto login_deadline
        = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!s.is_logged_in()) {
        core.process_received();
        if (core.server_closed()) return 3;
        if (std::chrono::steady_clock::now() >= login_deadline) return 2;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    core.ui().add_line(color::convert("$GRN[bot] login ok, running$COM"));

    ObjectID vendor_id = 0;  // /list 探明的商人会话号
    bool probe_pending = false;
    int menu_rounds    = 0;

    while (core.running() && !core.server_closed()) {
        core.process_received();

        // Probe 的异步回执：货架表到了就把商人钉下来；换房后表被清空
        // （vendor_items 的 GOTOROOM 语义），商人随之作废。
        if (probe_pending && !s.vendor_items().empty()) {
            vendor_id     = s.target_id();
            probe_pending = false;
            core.ui().add_line(color::convert("$GRN[bot] vendor found$COM"));
        }
        if (s.vendor_items().empty()) vendor_id = 0;

        bot::WorldView w = snapshot(s);

        int delay = 0;
        if (w.in_menu || w.waiting_menu_input) {
            // 菜单态优先：NPC 对话弹出的菜单继续选，6 轮不脱出即关。
            bot::MenuChoice choice = bot::decide_menu(w, ++menu_rounds);
            if (choice == bot::MenuChoice::Select) {
                int idx = std::uniform_int_distribution<int>(
                    1, w.menu_item_count)(rng);
                core.input().process_line(std::to_string(idx));
                core.ui().add_line(color::convert(
                    "$CYN[bot] menu " + std::to_string(idx) + "$COM"));
            }
            else if (choice == bot::MenuChoice::Input) {
                static const std::vector<std::string>* pool = &kMenuInputPool;
                core.input().process_line(
                    (*pool)[std::uniform_int_distribution<size_t>(
                        0, pool->size() - 1)(rng)]);
            }
            else {
                s.cancel_menu();
                menu_rounds = 0;
            }
            delay = bot::menu_delay_ms(rng);
        }
        else {
            menu_rounds     = 0;
            bot::Action act = bot::decide(w, rng);
            if (act == bot::Action::Probe) probe_pending = true;
            execute(core, act, rng, vendor_id);
            delay = sleep_ms_for(w, rng);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }

    return core.server_closed() ? 3 : 0;
}
