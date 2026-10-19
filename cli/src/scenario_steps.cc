#include "scenario_runner.h"

#include <cctype>
#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

#include "client_core.h"
#include "game_state.h"

bool CScenarioRunner::eval_cond(const std::string &cond, std::string &detail)
{
    std::string c = trim(cond);
    if (c.empty()) {
        detail = "empty condition";
        return false;
    }

    size_t sp         = c.find_first_of(" \t");
    std::string key   = lower_ascii((sp == std::string::npos) ? c : c.substr(0, sp));
    std::string val   = (sp == std::string::npos) ? "" : trim(c.substr(sp + 1));

    GameState &st = core_.state();

    if (key == "always") return true;
    if (key == "login") return st.is_logged_in() && st.player_id() != 0;
    if (key == "menu") return st.in_menu();
    if (key == "dialog") return st.in_dialog();
    if (key == "battle") return st.in_battle();
    if (key == "battle_end") return !st.in_battle();

    if (key == "room") {
        return st.room_name() == val || st.room_map() == val
               || std::to_string(st.current_room()) == val;
    }

    if (key == "text" || key == "notext") {
        if (val.empty()) {
            detail = key + " condition needs a needle";
            return false;
        }
        // Split alternatives by '|' (e.g., "进步了|退步了|没有学到")
        std::vector<std::string> needles;
        size_t start = 0;
        while (start < val.size()) {
            size_t pipe = val.find('|', start);
            if (pipe == std::string::npos) pipe = val.size();
            std::string needle = trim(val.substr(start, pipe - start));
            if (!needle.empty()) needles.push_back(std::move(needle));
            start = pipe + 1;
        }
        if (needles.empty()) {
            detail = key + " condition needs at least one non-empty needle";
            return false;
        }
        bool found = false;
        for (const auto &line : core_.recent_output(2000)) {
            for (const auto &needle : needles) {
                if (line.find(needle) != std::string::npos) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        // notext：任一 needle 出现即为假（配合 wait_cond 监视"不出现"）
        return key == "text" ? found : !found;
    }

    if (key == "object") {
        for (const auto &[container, objs] : st.room_objects()) {
            (void)container;
            for (const auto &o : objs) {
                if (o.name == val || o.internal_id == val
                    || o.name.find(val) != std::string::npos) {
                    return true;
                }
            }
        }
        return false;
    }

    if (key == "item") {
        for (const auto &o : st.inventory()) {
            if (o.name == val || o.internal_id == val
                || o.name.find(val) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    if (key == "attr") return eval_attr_cond(val, detail);

    detail = "unknown condition key: " + key;
    return false;
}

bool CScenarioRunner::eval_attr_cond(const std::string &val,
                                     std::string &detail)
{
    std::istringstream iss(val);
    std::string name, op;
    long long want = 0;
    if (!(iss >> name >> op >> want)) {
        detail = "attr condition syntax: attr <name> <op> <value>";
        return false;
    }
    const PlayerAttr &a = core_.state().player_attr();
    long long have      = 0;
    if (name == "exp")
        have = a.exp;
    else if (name == "combat_exp")
        have = a.combat_exp;
    else if (name == "hp")
        have = a.hp;
    else if (name == "max_hp")
        have = a.max_hp;
    else if (name == "mp")
        have = a.mp;
    else if (name == "max_mp")
        have = a.max_mp;
    else if (name == "level")
        have = a.level;
    else if (name == "shen")
        have = a.shen;
    else {
        detail = "unknown attr: " + name;
        return false;
    }

    if (op == "==") return have == want;
    if (op == "!=") return have != want;
    if (op == ">=") return have >= want;
    if (op == "<=") return have <= want;
    if (op == ">") return have > want;
    if (op == "<") return have < want;
    detail = "unknown attr operator: " + op;
    return false;
}

bool CScenarioRunner::wait_cond(const std::string &cond, int timeout_ms)
{
    // notext 语义相反：全程监视"不该出现的文本"，一旦出现立即失败，
    // 坚持到期满无出现才算通过；其余条件为"等到出现"。
    std::string c     = trim(cond);
    size_t sp         = c.find_first_of(" \t");
    std::string token = (sp == std::string::npos) ? c : c.substr(0, sp);
    const bool watch_absent = (lower_ascii(token) == "notext");

    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        core_.process_received();
        std::string detail;
        bool ok = eval_cond(cond, detail);
        if (!detail.empty()) {
            return fail("bad condition '" + cond + "': " + detail);
        }
        if (watch_absent) {
            if (!ok) return fail("unexpected text matched '" + cond + "'");
            if (core_.server_closed()) {
                return fail("server closed the connection");
            }
            if (std::chrono::steady_clock::now() >= deadline) return true;
        }
        else {
            if (ok) return true;
            if (core_.server_closed()) {
                return fail("server closed the connection");
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return fail("timeout waiting for '" + cond + "'");
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool CScenarioRunner::run_step(const Step &step)
{
    echo_step(step);

    const std::string verb = step.verb;
    const std::string args = step.args;

    if (verb == "login") {
        std::istringstream iss(args);
        std::string account, password;
        if (!(iss >> account >> password)) {
            return fail("login needs: login <account> <password>");
        }
        core_.input().process_line("/login " + subst(account) + " "
                                   + subst(password));
        return true;
    }

    if (verb == "cmd" || verb == "menuinput") {
        core_.input().process_line(subst(args));
        return true;
    }

    if (verb == "menu") return run_menu_step(subst(args));

    if (verb == "wait" || verb == "assert") {
        // 支持在条件后追加 "timeout=<ms>" 覆盖本步超时：
        //   wait battle_end timeout=90000
        std::string cond = subst(args);
        int tmo          = opts_.default_timeout_ms;
        size_t tp        = cond.find("timeout=");
        if (tp != std::string::npos) {
            size_t vstart = tp + 8;
            size_t vend   = vstart;
            while (vend < cond.size()
                   && std::isdigit(static_cast<unsigned char>(cond[vend]))) {
                ++vend;
            }
            if (vend > vstart) {
                tmo = std::stoi(cond.substr(vstart, vend - vstart));
                cond.erase(tp, vend - tp);
                cond = trim(cond);
            }
        }
        return wait_cond(cond, tmo);
    }

    if (verb == "sleep") {
        std::string v = subst(args);
        int ms        = 0;
        try {
            ms = std::stoi(v);
        } catch (...) {
            return fail("sleep needs milliseconds");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
        return true;
    }

    if (verb == "set") {
        size_t sp = args.find_first_of(" \t");
        if (sp == std::string::npos) return fail("set needs: set <name> <value>");
        std::string name = trim(args.substr(0, sp));
        vars_[name]      = subst(trim(args.substr(sp + 1)));
        save_vars();
        return true;
    }

    if (verb == "clear") {
        core_.clear_output();
        return true;
    }

    if (verb == "capture") return run_capture_step(subst(args));

    return fail("unknown step verb: " + verb);
}

bool CScenarioRunner::run_menu_step(const std::string &want)
{
    if (want.empty()) return fail("menu needs an index or text");
    if (!core_.state().in_menu()) {
        return fail("menu selection attempted but no menu is open");
    }
    const auto &items = core_.state().menu_items();
    int idx           = -1;
    if (all_digits(want)) {
        idx = std::stoi(want) - 1;
    }
    else {
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].text.find(want) != std::string::npos) {
                idx = static_cast<int>(i);
                break;
            }
        }
    }
    if (idx < 0 || static_cast<size_t>(idx) >= items.size()) {
        return fail("menu item not found: " + want);
    }
    core_.input().process_line(std::to_string(idx + 1));
    return true;
}

bool CScenarioRunner::run_capture_step(const std::string &args)
{
    size_t p = args.rfind(" as ");
    if (p == std::string::npos) {
        return fail("capture needs: capture <marker> as <var>");
    }
    std::string marker = trim(args.substr(0, p));
    std::string var    = trim(args.substr(p + 4));
    if (marker.empty() || var.empty()) {
        return fail("capture needs: capture <marker> as <var>");
    }

    auto lines = core_.recent_output(2000);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        size_t m = it->find(marker);
        if (m == std::string::npos) continue;
        size_t pos = m + marker.size();
        while (pos < it->size()
               && !std::isdigit(static_cast<unsigned char>((*it)[pos]))) {
            ++pos;
        }
        size_t start = pos;
        while (pos < it->size()
               && std::isdigit(static_cast<unsigned char>((*it)[pos]))) {
            ++pos;
        }
        if (start < pos) {
            vars_[var] = it->substr(start, pos - start);
            save_vars();
            if (opts_.echo) {
                std::cout << "[script] captured " << var << "=" << vars_[var]
                          << std::endl;
            }
            return true;
        }
    }
    return fail("capture marker not found: " + marker);
}
