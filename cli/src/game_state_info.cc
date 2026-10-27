#include "game_state_info.h"

#include <cmath>
#include <sstream>

#include "color.h"
#include "game_state.h"

// =========================================================================
// detail namespace: helper functions used by panels.
// =========================================================================

namespace detail {

std::string make_header(const std::string &text)
{
    return color::convert("$CYN[ " + text + " ]$COM");
}

std::string make_line(const std::string &key, const std::string &value)
{
    size_t pad = (key.length() >= 8) ? 0 : (8 - key.length());
    return "  " + color::convert("$COM" + key + "$COM:") + std::string(pad, ' ')
           + value;
}

// Returns a block bar like "[===     ]".
std::string make_bar(double pct)
{
    int filled = static_cast<int>(std::round(pct * 8.0 / 100.0));
    if (filled < 1 && pct > 0.0) filled = 1;
    if (filled > 8) filled = 8;
    return "[" + std::string(filled, '=') + std::string(8 - filled, ' ') + "]";
}

}  // namespace detail

// =========================================================================
// Anonymous namespace: each panel is a static function.
// =========================================================================

namespace {

std::vector<std::string> panel_character(const GameState &gs)
{
    auto a = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 人物信息"));
    lines.push_back(detail::make_line("账户ID", gs.account_id()));
    lines.push_back(detail::make_line("名  字", a.name));
    if (!a.gender.empty())
        lines.push_back(detail::make_line("性  别", a.gender));
    if (!a.race.empty()) lines.push_back(detail::make_line("种  族", a.race));
    if (!a.job.empty()) lines.push_back(detail::make_line("职  业", a.job));
    lines.push_back(detail::make_line("等  级", std::to_string(a.level)));
    if (!a.title.empty()) lines.push_back(detail::make_line("头  衔", a.title));
    lines.push_back(detail::make_line("年  龄", std::to_string(a.age)));
    if (a.exp > 0) {
        std::ostringstream ss;
        ss << a.exp;
        if (a.next_exp > a.exp) ss << "/" << a.next_exp;
        lines.push_back(detail::make_line("经  验", ss.str()));
    }
    lines.push_back(detail::make_line("总经验", std::to_string(a.combat_exp)));
    if (a.idle) lines.push_back(detail::make_line("状  态", "发呆中"));
    lines.push_back(detail::make_line(
        "战  绩", "到目前为止攻击杀了" + std::to_string(a.mks) + "个人（其中"
                      + std::to_string(a.pks) + "个玩家）"));
    return lines;
}

std::vector<std::string> panel_location(const GameState &gs)
{
    auto a = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 位置与状态"));
    if (!gs.room_map().empty())
        lines.push_back(detail::make_line("地 图", gs.room_map()));
    lines.push_back(detail::make_line("名 称", gs.room_name()));
    if (gs.in_battle()) {
        std::string tag
            = detail::make_header(color::convert("$RED[战斗中]$COM"));
        lines.push_back("  " + tag);
    }
    else {
        lines.push_back(detail::make_line("状 态", "空闲"));
    }
    return lines;
}

std::vector<std::string> panel_attributes(const GameState &gs)
{
    auto a = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 四维属性"));
    lines.push_back(detail::make_line("力", std::to_string(a.str)));
    lines.push_back(detail::make_line("体", std::to_string(a.con)));
    lines.push_back(detail::make_line("智", std::to_string(a.intel)));
    lines.push_back(detail::make_line("敏", std::to_string(a.dex)));
    return lines;
}

static std::string make_vital_line(const char *label, int64_t cur,
                                   int64_t max_val, int64_t eff = 0,
                                   int64_t enforce = 0)
{
    if (max_val <= 0) return detail::make_line(label, std::to_string(cur));
    double pct = static_cast<double>(cur) / max_val * 100.0;
    return color::convert(
        "  " + std::string(label) + " [" + detail::make_bar(pct) + "] "
        + std::to_string(cur) + "/" + std::to_string(max_val)
        + (eff > 0 ? " (" + std::to_string(eff) + ")" : "")
        + (enforce > 0 ? " (+" + std::to_string(enforce) + ")" : ""));
}

std::vector<std::string> panel_vitals(const GameState &gs)
{
    auto a = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 气血真气"));
    lines.push_back(make_vital_line("HP", a.hp, a.max_hp, a.eff_hp, 0));
    lines.push_back(make_vital_line("MP", a.mp, a.max_mp, 0, a.enforce));

    return lines;
}

std::vector<std::string> panel_combat(const GameState &gs)
{
    auto a = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 攻守战力"));
    lines.push_back(detail::make_line("攻击", std::to_string(a.attack)));
    lines.push_back(detail::make_line("防御", std::to_string(a.defense)));
    if (a.combat_exp != 0) {
        std::ostringstream ss;
        ss << " 战斗力:" << a.combat_exp;
        lines.push_back(ss.str());
    }
    return lines;
}

// Helper: true if item has valid metadata (for .info display filtering).
static bool inv_has_meta(const ObjectInfo &item)
{
    return !item.name.empty() || item.internal_id.length() > 0
           || item.wield != 0;
}

std::vector<std::string> panel_skills(const GameState &gs)
{
    const auto &skills = gs.skills();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(
        " 武功 (共" + std::to_string(skills.size()) + "个)"));
    if (skills.empty()) {
        lines.push_back(
            color::convert("  " + detail::make_header("$YEL(未习得)$COM")));
    }
    else {
        for (const auto &sk : skills) {
            std::ostringstream ss;
            bool mapped = false;
            for (const auto &m : skills) {
                if (sk.base_name == m.mapped_skill) {
                    mapped = true;
                    break;
                }
            }
            ss << (mapped ? "[*]" : "[ ]") << sk.skill_name << "("
               << sk.base_name << ") [" << sk.level << "/" << sk.potential
               << "]";
            if (sk.prepared) ss << color::convert(" [Y]");
            lines.push_back(color::convert(ss.str()));
        }
    }
    return lines;
}

std::vector<std::string> panel_inventory(const GameState &gs)
{
    const auto &inv = gs.inventory();
    auto a          = gs.player_attr();
    std::vector<std::string> lines;
    lines.push_back(detail::make_header(" 携带物品"));
    if (inv.empty()) {
        lines.push_back(
            color::convert("  " + detail::make_header("$YEL(空)$COM")));
    }
    else {
        // Show all items uniformly (like xclient's unified ItemList).
        // Format:  "名称(internal_id)" or  "名称(internal_id)[槽位名]" for
        // equipped.
        for (const auto &item : inv) {
            if (!inv_has_meta(item)) continue;

            std::string line = item.name;

            // All items: "名称(internal_id)" for identification
            if (item.internal_id.length() > 0) {
                line += "(" + item.internal_id + ")";
            }

            // For equipped items, also show the slot name
            if (item.wield != 0) {
                std::string slot_name = wield_position_name(item.wield);
                if (slot_name != "未知") {
                    line += "[" + slot_name + "]";
                }
            }

            // Items with quantity: show count suffix
            if (item.qty > 1) {
                line += " x" + std::to_string(item.qty);
            }

            lines.push_back(line);
        }

        int64_t total = 0;
        for (const auto &item : inv) total += static_cast<int64_t>(item.weight);
        double pct = a.max_carry > 0
                         ? static_cast<double>(total) / a.max_carry * 100.0
                         : 0.0;
        lines.push_back(make_vital_line("负重", total, a.max_carry));
        if (pct < 50.0) {
            lines.push_back(
                color::convert("  " + detail::make_header("$GRN未占满$COM")));
        }
        else if (pct > 80.0) {
            lines.push_back(
                color::convert("  " + detail::make_header("$RED已拥挤$COM")));
        }
    }
    return lines;
}

}  // anonymous namespace

// =========================================================================
// Public API — declared in game_state_info.h
// =========================================================================

std::vector<std::string> player_info_panels(const GameState &gs)
{
    std::vector<std::string> all;
    auto push = [&all, &gs](auto fn) {
        for (auto &&l : fn(gs)) all.push_back(std::move(l));
    };
    push(panel_character);
    push(panel_location);
    push(panel_attributes);
    push(panel_vitals);
    push(panel_combat);
    push(panel_skills);
    push(panel_inventory);
    return all;
}

std::vector<std::string> player_info_panel(const GameState &gs,
                                           const std::string &panel_name)
{
    if (panel_name == "character") return panel_character(gs);
    if (panel_name == "location") return panel_location(gs);
    if (panel_name == "attributes") return panel_attributes(gs);
    if (panel_name == "vitals") return panel_vitals(gs);
    if (panel_name == "combat") return panel_combat(gs);
    if (panel_name == "skills") return panel_skills(gs);
    if (panel_name == "inventory") return panel_inventory(gs);
    return {};
}
