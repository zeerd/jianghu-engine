#include "scenario_runner.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

#include "client_core.h"
#include "game_state.h"

bool CScenarioRunner::eval_cond(const std::string& cond, std::string& detail,
                                TextHit* hit)
{
    std::string c = trim(cond);
    if (c.empty()) {
        detail = "empty condition";
        return false;
    }

    size_t sp = c.find_first_of(" \t");
    std::string key
        = lower_ascii((sp == std::string::npos) ? c : c.substr(0, sp));
    std::string val = (sp == std::string::npos) ? "" : trim(c.substr(sp + 1));

    GameState& st = core_.state();

    if (key == "always") return true;
    if (key == "login") return st.is_logged_in() && st.player_id() != 0;
    if (key == "menu") return st.in_menu();
    if (key == "dialog") return st.in_dialog();
    if (key == "battle") return st.in_battle();
    if (key == "battle_end") return !st.in_battle();

    // 忙碌起止都随 STATE 翻转推送，故两个方向都可等：
    // `busy` 等起始（起始窗可窄于轮询，锁存兜底——见 GameState::busy_latched），
    // `not_busy` 等结束（只看实时：结束 STATE 缺 bsy 即翻回不忙）。
    // wait_cond 在两者的 wait 通过时消费锁存，防止串进下一次命令。
    if (key == "busy") return st.player_attr().busy || st.busy_latched();
    if (key == "not_busy") return !st.player_attr().busy;

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
        if (hit) hit->valid = false;
        // 按**绝对行号**从消费游标往后扫，取最早命中的那条：`consume` 要靠
        // 命中行的行号把游标推进过去，也要靠行文本喂给随后的 capture。
        size_t first_abs = 0;
        auto lines = core_.recent_output(ClientCore::kOutputWindow, &first_abs);
        bool found = false;
        for (size_t i = 0; i < lines.size(); ++i) {
            for (const auto& needle : needles) {
                if (lines[i].find(needle) != std::string::npos) {
                    found = true;
                    if (hit && key == "text") {
                        hit->valid = true;
                        hit->abs   = first_abs + i;
                        hit->line  = lines[i];
                    }
                    break;
                }
            }
            if (found) break;
        }
        // notext：任一 needle 出现即为假（配合 wait_cond 监视"不出现"）
        return key == "text" ? found : !found;
    }

    if (key == "object") {
        for (const auto& [container, objs] : st.room_objects()) {
            (void)container;
            for (const auto& o : objs) {
                if (o.name == val || o.internal_id == val
                    || o.name.find(val) != std::string::npos) {
                    return true;
                }
            }
        }
        return false;
    }

    if (key == "item") {
        for (const auto& o : st.inventory()) {
            if (o.name == val || o.internal_id == val
                || o.name.find(val) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    if (key == "attr") return eval_attr_cond(val, detail);

    // wield <物品> <槽位>：等待某物品真的被装备到指定槽位（如 wield 铁剑
    // 右手）。 只断言「回执文本提到装备」是不够的——回执不说明落在哪个槽位；
    // 物品名子串匹配（同 item/object），槽位按 .info
    // 的显示名比对，也接受数字槽位。
    if (key == "wield") {
        size_t slot_pos = val.find_first_of(" \t");
        if (slot_pos == std::string::npos) {
            detail = "wield condition needs: wield <item> <slot>";
            return false;
        }
        std::string item_name = trim(val.substr(0, slot_pos));
        std::string slot      = trim(val.substr(slot_pos + 1));
        for (const auto& o : st.inventory()) {
            if (o.wield == 0) continue;  // 0 = 未装备，槽位名只会是「未知」
            bool name_hit = (o.name == item_name || o.internal_id == item_name
                             || o.name.find(item_name) != std::string::npos);
            if (!name_hit) continue;
            if (wield_position_name(o.wield) == slot
                || std::to_string(o.wield) == slot) {
                return true;
            }
        }
        return false;
    }

    detail = "unknown condition key: " + key;
    return false;
}

// 取属性当前值。属性名单只此一份：eval_attr_cond 的判定与 wait_cond 的超时
// 诊断都读它，避免两处各写一份而漂移。
// 之所以需要它：剧本失败时只回显条件、不回显当前值，就没法区分
// 「状态还没到位」与「状态变了却没同步到客户端」——后者才是真 bug。
bool CScenarioRunner::attr_value(const std::string& name, long long& out) const
{
    const PlayerAttr& a = core_.state().player_attr();
    if (name == "exp")
        out = a.exp;
    else if (name == "combat_exp")
        out = a.combat_exp;
    else if (name == "hp")
        out = a.hp;
    else if (name == "max_hp")
        out = a.max_hp;
    else if (name == "eff_hp")  // 有效气血上限：升级抬高 max_hp 后它会滞后，
        out = a.eff_hp;         // 正是「金创药补上限」的验证点
    else if (name == "mp")
        out = a.mp;
    else if (name == "max_mp")
        out = a.max_mp;
    else if (name == "level")
        out = a.level;
    else if (name == "shen")
        out = a.shen;
    else if (name == "str")  // RTNINFO E=：四维含 apply/* 装备修正与技能加成
        out = a.str;
    else if (name == "con")  // RTNINFO F=
        out = a.con;
    else if (name == "int")  // RTNINFO G=
        out = a.intel;
    else if (name == "dex")  // RTNINFO H=
        out = a.dex;
    else if (name == "damage")  // RTNINFO J=：apply/damage 临时修正
        out = a.damage;
    else
        return false;
    return true;
}

// 把 `attr <name> <op> <value>` 中该属性的当前值拼成 "<name>=<值>"
std::string CScenarioRunner::attr_snapshot(const std::string& cond) const
{
    static const std::string prefix = "attr ";
    if (cond.compare(0, prefix.size(), prefix) != 0) return "";
    const size_t begin = prefix.size();
    const size_t end   = cond.find_first_of(" \t", begin);
    if (end == std::string::npos) return "";

    const std::string name = cond.substr(begin, end - begin);
    long long value        = 0;
    if (!attr_value(name, value)) return "";
    return name + "=" + std::to_string(value);
}

bool CScenarioRunner::eval_attr_cond(const std::string& val,
                                     std::string& detail)
{
    std::istringstream iss(val);
    std::string name, op;
    long long want = 0;
    if (!(iss >> name >> op >> want)) {
        detail = "attr condition syntax: attr <name> <op> <value>";
        return false;
    }
    long long have = 0;
    if (!attr_value(name, have)) {
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

bool CScenarioRunner::wait_cond(const std::string& cond, int timeout_ms,
                                bool consume)
{
    // notext 语义相反：全程监视"不该出现的文本"，一旦出现立即失败，
    // 坚持到期满无出现才算通过；其余条件为"等到出现"。
    std::string c           = trim(cond);
    size_t sp               = c.find_first_of(" \t");
    std::string token       = (sp == std::string::npos) ? c : c.substr(0, sp);
    const bool watch_absent = (lower_ascii(token) == "notext");

    // consume 的适用面：它推的是**输出行的消费游标**，只有「等到出现某行」
    // 的 text 条件才有命中行可推。notext 是全程监视、别的条件没有行号，
    // 带上 consume 只会是笔误，早失败好过静默不生效。
    if (consume) {
        if (watch_absent) {
            return fail("consume is not allowed on notext: " + c);
        }
        if (lower_ascii(token) != "text") {
            return fail("consume is only supported on 'wait text': " + c);
        }
    }

    // 上一条 wait 的命中行只对紧随其后的 capture 有意义，换 wait 即作废。
    consumed_line_.clear();

    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(timeout_ms);
    for (;;) {
        core_.process_received();
        std::string detail;
        TextHit hit;
        bool ok = eval_cond(cond, detail, &hit);
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
            if (ok) {
                // busy/not_busy 的 wait 通过 = 忙的起止都已走完，消费起始锁存
                if (token == "busy" || token == "not_busy")
                    core_.state().clear_busy_latched();
                // consume：命中行到此为止「看过了」。游标前移使下一次 wait
                // 只会命中**更晚**的行——这正是服务链按到达顺序逐条推进的
                // 机制；行文本同时存下来，供紧跟的 capture 读（命中行已被
                // 游标丢出缓冲，再扫缓冲必然落空）。
                if (consume && hit.valid) {
                    core_.advance_output_cursor(hit.abs + 1);
                    consumed_line_ = hit.line;
                }
                return true;
            }
            if (core_.server_closed()) {
                return fail("server closed the connection");
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                // 附带当前值：属性类断言失败时，只给条件等于没给信息——
                // 必须能看出是「还没到位」还是「变了没同步过来」。
                std::string why        = "timeout waiting for '" + cond + "'";
                const std::string snap = attr_snapshot(c);
                if (!snap.empty()) why += "（当前 " + snap + "）";
                return fail(why);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool CScenarioRunner::run_step(const Step& step)
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
        // 独立 token `consume`：命中即把该行推出消费游标（`wait text X
        // consume`）
        const bool consume = strip_token(cond, "consume");
        cond               = trim(cond);
        return wait_cond(cond, tmo, consume);
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
        if (sp == std::string::npos)
            return fail("set needs: set <name> <value>");
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

    // 文件准备动词：热重载类剧本要在运行中切换被测脚本的变体（A/B/损坏版），
    // 改文件与断言必须同处一个剧本，因果才读得出来。
    // 安全边界：写入只允许落在测试暂存区（test_tmp\ 或直跑兜底 tmp\）——
    // 落点一旦漂进 lua-sample、lua-default 等教材或生产目录，会直接污染
    // 被测世界，白名单把剧本的文件写入钉死在暂存区。
    if (verb == "filecopy" || verb == "filedelete") {
        std::string a = subst(args);
        if (a.empty()) {
            return fail(verb + " needs: " + verb + " <path>");
        }
        auto scratch_ok = [](std::string p) {
            std::replace(p.begin(), p.end(), '\\', '/');
            if (p.rfind("test_tmp/", 0) == 0 || p.rfind("tmp/", 0) == 0)
                return true;
            return p.find("/test_tmp/") != std::string::npos
                   || p.find("/tmp/") != std::string::npos;
        };

        if (verb == "filedelete") {
            if (!scratch_ok(a)) {
                return fail(
                    "file write refused, destination must be under "
                    "test_tmp/ or tmp/: "
                    + a);
            }
            std::error_code ec;
            if (!std::filesystem::exists(a)) {
                return fail("filedelete target not found: " + a);
            }
            std::filesystem::remove(a, ec);
            if (ec) return fail("filedelete failed: " + ec.message());
            return true;
        }

        // filecopy <src> <dst>：源取首个空白前的 token，目标取行尾余下
        // 全文（目标允许含空格；路径含空格时源侧不受支持，与其余动词的
        // 空白切分约定一致）
        size_t sp = a.find_first_of(" \t");
        if (sp == std::string::npos) {
            return fail("filecopy needs: filecopy <src> <dst>");
        }
        std::string src = a.substr(0, sp);
        std::string dst = trim(a.substr(sp + 1));
        if (!scratch_ok(dst)) {
            return fail(
                "file write refused, destination must be under "
                "test_tmp/ or tmp/: "
                + dst);
        }

        std::error_code ec;
        if (!std::filesystem::exists(src)) {
            return fail("filecopy source not found: " + src);
        }
        if (!dst.empty()) {
            std::filesystem::create_directories(
                std::filesystem::path(dst).parent_path(), ec);
        }
        std::filesystem::copy_file(
            src, dst, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) return fail("filecopy failed: " + ec.message());

        // 目标 mtime 盖成「单调递增的此刻」：filecopy 的语义是「现在应用这
        // 个变体」，模板的创作时间对目标毫无意义——保留模板时间戳时，刚定稿
        // 的 A/B 变体可能带同一个文件系统时间戳，被 mtime 快筛当成「没变」。
        // 单调错开 ≥1 秒保证快筛必然看见本次变更（script.mtime 只在重载
        // 成功时更新，失败重试期间新旧时间戳同样不能撞）。noop（内容不变）
        // 仍由哈希裁决：盖时间戳只让快筛放行，哈希相同照样不重载。
        static std::filesystem::file_time_type s_lastStamp;
        auto stamp = std::filesystem::file_time_type::clock::now();
        if (stamp <= s_lastStamp + std::chrono::seconds(1)) {
            stamp = s_lastStamp + std::chrono::seconds(1);
        }
        s_lastStamp = stamp;
        std::error_code tec;
        std::filesystem::last_write_time(dst, stamp, tec);
        if (tec) return fail("filecopy cannot stamp mtime: " + tec.message());
        return true;
    }

    return fail("unknown step verb: " + verb);
}

bool CScenarioRunner::strip_token(std::string& s, const std::string& tok)
{
    size_t pos = 0;
    while (pos <= s.size()) {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) ++pos;
        if (pos >= s.size()) break;
        size_t end = pos;
        while (end < s.size() && s[end] != ' ' && s[end] != '\t') ++end;
        if (s.compare(pos, end - pos, tok) == 0) {
            // 连同它前面的那个分隔符一起摘掉：token 在尾部时不留尾随空格，
            // 在中部时不产生双空格；首部则只摘 token，剩余字节原样保留。
            const size_t from = (pos > 0) ? pos - 1 : pos;
            s.erase(from, end - from);
            return true;
        }
        pos = end;
    }
    return false;
}

bool CScenarioRunner::run_menu_step(const std::string& want)
{
    if (want.empty()) return fail("menu needs an index or text");
    if (!core_.state().in_menu()) {
        return fail("menu selection attempted but no menu is open");
    }
    const auto& items = core_.state().menu_items();
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

bool CScenarioRunner::run_capture_step(const std::string& args)
{
    size_t p = args.rfind(" as ");
    if (p == std::string::npos) {
        return fail("capture needs: capture <正则> as <var>");
    }
    std::string marker = trim(args.substr(0, p));
    std::string var    = trim(args.substr(p + 4));
    if (marker.empty() || var.empty()) {
        return fail("capture needs: capture <正则> as <var>");
    }

    // marker 按 ECMAScript 正则解释（design §7 的 capture 契约）：
    //   含捕获组 → 取首个实际匹配组的文本（抓句子这类非数字变量）；
    //   无捕获组 → 取匹配位置后的第一段数字（账号 id 等数字用法）。
    // 非法正则是剧本笔误，立即失败不静默降级成字面量。
    std::regex re;
    try {
        re = std::regex(marker, std::regex::ECMAScript);
    } catch (const std::regex_error&) {
        return fail("capture invalid regex: " + marker);
    }

    auto try_line = [&](const std::string& line) -> bool {
        std::smatch m;
        if (!std::regex_search(line, m, re)) return false;

        if (re.mark_count() > 0) {
            for (size_t g = 1; g < m.size(); ++g) {
                if (m[g].matched && !m[g].str().empty()) {
                    vars_[var] = m[g].str();
                    save_vars();
                    if (opts_.echo) {
                        core_.ui().post_raw("[script] captured " + var + "="
                                            + vars_[var] + "\n");
                    }
                    return true;
                }
            }
            return false;  // 组存在但本行没匹配上，交由调用方试下一行
        }

        size_t pos = static_cast<size_t>(m.position(0) + m.length(0));
        while (pos < line.size()
               && !std::isdigit(static_cast<unsigned char>(line[pos]))) {
            ++pos;
        }
        size_t start = pos;
        while (pos < line.size()
               && std::isdigit(static_cast<unsigned char>(line[pos]))) {
            ++pos;
        }
        if (start >= pos) return false;
        vars_[var] = line.substr(start, pos - start);
        save_vars();
        if (opts_.echo) {
            core_.ui().post_raw("[script] captured " + var + "=" + vars_[var]
                                + "\n");
        }
        return true;
    };

    // consume 语义：`wait text … consume` 命中行已被消费游标丢出缓冲，只能
    // 从存下来的那一行读，而且**只读那一行**。收紧是有意的——服务链里先到的
    // 请求会滞留缓冲，退回全缓冲倒序扫的话 capture 会抓到比 wait 命中更新的
    // 请求，账号与命令就张冠李戴了。未带 consume 的存量剧本走下面的原路。
    if (!consumed_line_.empty()) {
        if (try_line(consumed_line_)) return true;
        return fail("capture marker not found in consumed line: " + marker);
    }

    auto lines = core_.recent_output(ClientCore::kOutputWindow);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (try_line(*it)) return true;
    }
    return fail("capture marker not found: " + marker);
}
