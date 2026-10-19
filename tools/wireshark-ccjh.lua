-- ============================================================================
-- wireshark-ccjh.lua — JJJH（江湖）协议 Wireshark 解析插件
--
-- 协议来源：jianghu-engine（src/net/GateRecvDispatch.cpp、GateInfo.cpp、
-- src/user/User.cpp、src/net/Login.cpp、src/util/StringUtil.cpp）与
-- jianghu-cli（src/protocol.cc、src/connection.cc）。
--
-- 一、链路与端口
--   客户端 ↔ 游戏服务器：TCP 6600（引擎 g_server_port 默认值，可被 ini 的
--   [server] port 覆盖；如改端口请同步修改下方 SERVER_PORT）。
--
-- 二、帧格式（两个方向均为「XOR 密文帧 + 终止符」）
--   C→S 命令帧：encode_str("<actor> <verb> [args]\t") + '\n'
--     · '\t' 为帧终止符；客户端 Connection::send() 每次发送都追加 '\n'，
--       引擎 CGateInfo::has_completion_packet() 会把流中的 '\n' 一律吃掉。
--     · 首令牌为角色对象 id（引擎 handle_command 校验必须等于本人）；
--       "0 xxx" 为立即执行的系统指令（CUser::push_command 特判）。
--   C→S 登录帧（每连接首帧，仅一次）：encode_str("login <本机IP> <版本>
--     <账号> <密码> <卡号> <卡密>") + '\n'
--     · 登录帧没有 '\t'！引擎 GateRecvDispatch 对首个 recv 整块截取，并在
--       第一个 '\n' 处截断（on_login → decode_str 解 [0..254]）。
--   S→C 消息帧：encode_str("&C=<码>&K=V&...&\t")，无追加 '\n'
--     · CUser::send_command / send_to_client_direct 编码后以 '\t' 结尾。
--     · 明文中的 '\n' 会原样穿透 XOR 出现在帧内（属载荷，不是终止符）。
--
-- 三、XOR 流密码（双向相同；StringUtil.cpp decode_str，自逆）
--   密钥 15 字节：0D 4C 16 41 01 38 4E 0A 57 15 4F 1B 4F 06 44
--   对帧内第 i 字节（0 起）：
--     · 若为 ' ' / '\n' / '\t'：原样保留；
--     · 否则 b ^= key[i % 15]；若结果落回 '\t' '\n' ' ' 0x00 之一，则再
--       异或一次还原（故编码线上除终止符外不会出现 0x09/0x0A：'\t' 必为
--       帧终止符，C→S 的 '\n' 必为帧间隔/登录终止）；
--     · 无论是否异或，密钥游标都随字节下标推进；
--     · 每个帧独立从 i=0 起用密钥（引擎按帧调用 decode_str）。
--
-- 四、TCP 拼接/拆分
--   · 帧跨段：设置 pinfo.desegment_offset/desegment_len 请求重组；
--   · 一段多帧：循环逐帧解析；
--   · 溢出规则：缓冲 ≥1024 字节仍无终止符时强制取 1024 字节为一帧（与
--     CGateInfo::has_completion_packet 一致），用于破坏后的再同步。
--
-- 已知限制
--   · 中文显示：GBK 文本经 wireshark-ccjh-gbk.lua 的映射表转 UTF-8 显示
--     （GB2312 一级区 3755 常用字 + 符号区）；生僻字（二级区/GBK 扩展）
--     回退为 \xNN 转义。两文件须同置于 Wireshark 插件目录一起加载。
--   · 登录帧与首条命令同段到达时引擎会把命令一并吞掉丢弃；本插件按线上
--     实际字节如实分别展示。
-- ============================================================================

-- 位运算兼容层：Wireshark ≤2.x 提供 bit（LuaJIT），3.x 提供 bit32
-- （Lua 5.2），4.x 内嵌 Lua 5.4 只有原生 ~ 运算符。5.1/5.2 无法解析
-- ~ 语法，故用 load() 延迟到确有原生运算符的版本再编译。
local bxor
if bit and bit.bxor then
    bxor = bit.bxor
elseif bit32 and bit32.bxor then
    bxor = bit32.bxor
else
    bxor = load("return function(a, b) return a ~ b end")()
end

-- ---------------------------------------------------------------------------
-- 常量
-- ---------------------------------------------------------------------------

local SERVER_PORT = 6600  -- 引擎 g_server_port 默认值（GameInit.cpp）

-- 引擎接收溢出阈值：缓冲 ≥1024 仍无终止符时强制截为一帧
local OVERFLOW_LEN = 1024

-- XOR 密钥（见头注释三）
local KEY = {
    0x0D, 0x4C, 0x16, 0x41, 0x01, 0x38, 0x4E, 0x0A,
    0x57, 0x15, 0x4F, 0x1B, 0x4F, 0x06, 0x44,
}
local KEY_LEN = #KEY

-- S→C 消息码（src/object/Global.h「客户端接受指令」枚举）
local MSG_CODES = {
    [10] = "GOTOROOM",      [11] = "ADDOBJECT",  [12] = "REMOVEOBJECT",
    [13] = "RTNSKILL",      [14] = "RTNITEM",    [15] = "RTNINFO",
    [16] = "DOWNSKILL",     [17] = "DOWNOBJECT", [18] = "LOGINUSER",
    [19] = "NOTIFY",        [20] = "WAKEUP",     [21] = "DOWNEMOTE",
    [22] = "COMMON_ATTACK", [23] = "STATE",      [24] = "ADDDOOR",
    [25] = "SENDUSER",      [26] = "VENDOR_LIST", [27] = "QUESTION",
    [28] = "DOWNMAGIC",     [29] = "NOTIFY_FAIL", [30] = "BULLET",
    [31] = "TELLOBJECT",    [32] = "LOCATION",   [33] = "USERLIST",
    [34] = "DOWNUSER",      [35] = "MAGICCAST",  [36] = "SYN_AP",
    [37] = "SENDTABLE",     [38] = "CONTROLNPC", [39] = "ADDLOG",
}

local FRAME_LABEL = {
    login      = "Login frame",
    command    = "Command frame",
    message    = "Message frame",
    overflow   = "Overflow frame (engine 1024-byte cutoff)",
    incomplete = "Incomplete frame (TCP reassembly disabled?)",
}

-- ---------------------------------------------------------------------------
-- 基础函数
-- ---------------------------------------------------------------------------

-- 与引擎 decode_str 完全一致的逐字节变换（编解码同函数，自逆）。
-- 注意还原分支是与 C 版相同的四个顺序 if（可级联：如输入 0x00 撞上密钥
-- 0x0A 时会二次还原），不可合并为单条件。
local function decode_str(input)
    local out = {}
    local t = 1
    for p = 1, #input do
        local b = input:byte(p)
        if b ~= 0x20 and b ~= 0x0A and b ~= 0x09 then
            b = bxor(b, KEY[t])
            if b == 0x09 then b = bxor(0x09, KEY[t]) end
            if b == 0x0A then b = bxor(0x0A, KEY[t]) end
            if b == 0x20 then b = bxor(0x20, KEY[t]) end
            if b == 0x00 then b = bxor(0x00, KEY[t]) end
        end
        out[p] = string.char(b)
        t = t + 1
        if t > KEY_LEN then t = 1 end
    end
    return table.concat(out)
end

-- 显示文本：GBK 中文经一级区映射表转真 UTF-8（表由同目录
-- wireshark-ccjh-gbk.lua 加载进全局 JJJH_GBK_L1，运行时懒读、
-- 未加载也不报错）；未命中字节回退 \xNN；'\' 转义为 '\\' 无二义。
local function display_str(s)
    local map = rawget(_G, "JJJH_GBK_L1")
    local out, i, n = {}, 1, #s
    while i <= n do
        local b = s:byte(i)
        if b == 0x5C then
            out[#out + 1] = "\\\\"
            i = i + 1
        elseif b >= 0x20 and b <= 0x7E then
            out[#out + 1] = string.char(b)
            i = i + 1
        elseif b >= 0x81 and i < n and map then
            local b2 = s:byte(i + 1)
            local u = map[s:sub(i, i + 1)]
            if u then
                out[#out + 1] = u
                i = i + 2
            elseif b2 >= 0x40 and b2 <= 0xFE and b2 ~= 0x7F then
                -- 合法双字节对但不在一级区：整对回退，避免错位再配对
                out[#out + 1] = string.format("\\x%02X\\x%02X", b, b2)
                i = i + 2
            else
                out[#out + 1] = string.format("\\x%02X", b)
                i = i + 1
            end
        else
            out[#out + 1] = string.format("\\x%02X", b)
            i = i + 1
        end
    end
    return table.concat(out)
end

-- 截断显示文本，避免把多字节 UTF-8 序列拦腰切断
local function brief(s, max)
    s = display_str(s)
    if #s <= max then return s end
    s = s:sub(1, max)
    while #s > 0 do
        local b = s:byte(#s)
        if b >= 0x80 and b < 0xC0 then
            s = s:sub(1, -2)   -- 丢续字节
        elseif b >= 0xC0 then
            s = s:sub(1, -2)   -- 丢不完整序列的起始字节
            break
        else
            break              -- ASCII，完整
        end
    end
    return s .. "..."
end

-- ---------------------------------------------------------------------------
-- 协议与字段
-- ---------------------------------------------------------------------------

local jc = Proto("JJJH", "JJJH Protocol")

local f_type   = ProtoField.string("jc.frame.type", "Frame Type")
local f_rawlen = ProtoField.uint32("jc.frame.raw_len", "Raw Frame Length",
                                   base.DEC)
local f_raw    = ProtoField.bytes("jc.frame.raw", "Encoded Payload")
local f_dec    = ProtoField.string("jc.decoded", "Decoded Payload")

local f_l_verb = ProtoField.string("jc.login.verb", "Login Verb")
local f_l_ip   = ProtoField.string("jc.login.ip", "Local IP")
local f_l_ver  = ProtoField.string("jc.login.version", "Version")
local f_l_acct = ProtoField.string("jc.login.account", "Account")
local f_l_pass = ProtoField.string("jc.login.password", "Password")
local f_l_card = ProtoField.string("jc.login.card", "Card No")
local f_l_cpw  = ProtoField.string("jc.login.cardpass", "Card Password")

local f_c_actor = ProtoField.string("jc.cmd.actor", "Actor ID (0 = system)")
local f_c_verb  = ProtoField.string("jc.cmd.verb", "Command Verb")
local f_c_args  = ProtoField.string("jc.cmd.args", "Command Args")

local f_s_kv   = ProtoField.string("jc.msg.field", "Field (K=V)")
local f_s_code = ProtoField.uint32("jc.msg.code", "Message Code", base.DEC)
local f_s_name = ProtoField.string("jc.msg.code_name", "Message Code Name")

jc.fields = {
    f_type, f_rawlen, f_raw, f_dec,
    f_l_verb, f_l_ip, f_l_ver, f_l_acct, f_l_pass, f_l_card, f_l_cpw,
    f_c_actor, f_c_verb, f_c_args,
    f_s_kv, f_s_code, f_s_name,
}

-- ---------------------------------------------------------------------------
-- 各帧类型的字段解析（decoded 与原始字节 1:1 等长，可直接映射 tvb 区间）
-- ---------------------------------------------------------------------------

-- 登录帧：login <本机IP> <版本> <账号> <密码> <卡号> <卡密>
-- （format_login，jianghu-cli/src/protocol.cc；引擎 on_login 用
-- analyse_string 按空格逐项拆取）
local function add_login_fields(ft, tvb, o0, decoded)
    local fields = { f_l_ip, f_l_ver, f_l_acct, f_l_pass, f_l_card, f_l_cpw }
    local idx, account = 0, ""
    -- 捕获返回顺序 = 模式内顺序：起始位置、令牌文本、结束位置
    for a, tok, b in decoded:gmatch("()([^ ]+)()") do
        idx = idx + 1
        local range = tvb(o0 + a - 1, b - a)
        if idx == 1 then
            ft:add(f_l_verb, range, tok)
        elseif idx - 2 < #fields then
            ft:add(fields[idx - 1], range, display_str(tok))
            if idx == 4 then account = tok end
        end
    end
    return account
end

-- 命令帧：<actor> <verb> [args...]（引擎 handle_command：首令牌为发起者
-- 对象 id 且必须等于本人；"0 xxx" 为立即执行的系统指令）。首令牌非数字
-- 时按 <verb> [args...] 显示（引擎 atol 判 0，随后丢弃该命令）。
local function add_command_fields(ft, tvb, o0, decoded)
    local toks = {}
    for a, tok, b in decoded:gmatch("()([^ ]+)()") do
        toks[#toks + 1] = { a = a, b = b, text = tok }
    end

    local i, actor, verb, args = 1, "", "", ""
    if #toks > 0 and toks[1].text:match("^%d+$") then
        actor = toks[1].text
        ft:add(f_c_actor, tvb(o0 + toks[1].a - 1, #actor), actor)
        i = 2
    end
    if i <= #toks then
        verb = toks[i].text
        ft:add(f_c_verb, tvb(o0 + toks[i].a - 1, toks[i].b - toks[i].a),
               verb)
        if i < #toks then
            local a0 = toks[i + 1].a
            args = decoded:sub(a0)
            ft:add(f_c_args, tvb(o0 + a0 - 1, #decoded - a0 + 1),
                   display_str(args))
        end
    end
    return actor, verb, args
end

-- 服务端消息帧：&C=<码>&K=V&...&（引擎 get_value 以 "&k=" 查找、到下一
-- 个 '&' 截止；值内可含 '='，不含 '&'）
local function add_message_fields(ft, tvb, o0, decoded)
    local code, name
    -- 捕获返回顺序 = 模式内顺序：'&' 位置、键值对文本、结束位置
    for a, pair, b in decoded:gmatch("()&([^&]*)()") do
        if #pair > 0 then
            local k, v = pair:match("^([^=]*)=(.*)$")
            local range = tvb(o0 + a, b - a - 1)
            if k then
                ft:add(f_s_kv, range, string.format("%s = %s", k,
                                                    display_str(v)))
                if k == "C" then
                    code = tonumber(v)
                    if code then
                        name = MSG_CODES[code] or "UNKNOWN"
                        ft:add(f_s_code, range, code)
                        ft:add(f_s_name, range, name)
                    end
                end
            else
                ft:add(f_s_kv, range, display_str(pair))
            end
        end
    end
    return code, name
end

-- ---------------------------------------------------------------------------
-- 帧输出（o0/o1 为 tvb 偏移，含 1 字节终止符；overflow/incomplete 无终止符）
-- ---------------------------------------------------------------------------

local function emit_frame(root, tvb, kind, o0, o1, decoded, infos)
    local has_term = (kind ~= "overflow" and kind ~= "incomplete")
    local plen = has_term and (o1 - o0) or (o1 - o0 + 1)
    local range = tvb(o0, o1 - o0 + 1)
    local ft = root:add(range, string.format("%s: %d byte(s)%s",
                        FRAME_LABEL[kind], o1 - o0 + 1,
                        has_term and " (incl. terminator)" or ""))
    ft:add(f_type, range, kind)
    ft:add(f_rawlen, range, o1 - o0 + 1)
    ft:add(f_raw, tvb(o0, plen))
    ft:add(f_dec, tvb(o0, plen), display_str(decoded))

    if kind == "login" then
        infos[#infos + 1] = "LOGIN acct="
            .. brief(add_login_fields(ft, tvb, o0, decoded), 20)
    elseif kind == "command" then
        local actor, verb, args = add_command_fields(ft, tvb, o0, decoded)
        infos[#infos + 1] = string.format("CMD %s %s %s", actor, verb,
                                          brief(args, 30))
    elseif kind == "message" then
        local code, name = add_message_fields(ft, tvb, o0, decoded)
        if code then
            infos[#infos + 1] = string.format("%s(%d)", name, code)
        else
            infos[#infos + 1] = "MSG " .. brief(decoded, 30)
        end
    else
        infos[#infos + 1] = string.upper(kind) .. " " .. brief(decoded, 30)
    end
end

local function flush_info(pinfo, c2s, infos)
    if #infos > 0 then
        pinfo.cols.info = (c2s and "C2S " or "S2C ")
            .. table.concat(infos, " | ")
    end
end

-- ---------------------------------------------------------------------------
-- 主解析：按方向选择终止符集合（见头注释二）
-- ---------------------------------------------------------------------------

function jc.dissector(tvb, pinfo, tree)
    pinfo.cols.protocol = "JJJH"

    local c2s = (pinfo.dst_port == SERVER_PORT)
    local raw = tvb:raw(0, tvb:len())
    local n = #raw
    local root = tree:add(jc, tvb(), "JJJH Protocol")
    local infos = {}
    local off = 1  -- Lua 字符串下标（1 起）；对应 tvb 偏移 off-1

    while off <= n do
        -- C→S：帧边界上的孤立 '\n' 是 send() 追加的帧尾，跳过
        if c2s and raw:byte(off) == 0x0A then
            off = off + 1
        else
            local tab = raw:find("\t", off, true)
            local nl = c2s and raw:find("\n", off, true) or nil
            local term, kind
            if tab and (not nl or tab < nl) then
                term, kind = tab, (c2s and "command" or "message")
            elseif nl then
                term, kind = nl, "login"  -- '\n' 先至：登录/行帧
            end

            if term == off then
                off = off + 1  -- 空帧（连续终止符）：吃掉
            elseif term then
                emit_frame(root, tvb, kind, off - 1, term - 1,
                           decode_str(raw:sub(off, term - 1)), infos)
                off = term + 1
            else
                local remain = n - off + 1
                if remain >= OVERFLOW_LEN then
                    -- 引擎溢出规则：强制 1024 字节为一帧再同步
                    emit_frame(root, tvb, "overflow", off - 1, off + 1023,
                               decode_str(raw:sub(off, off + 1023)), infos)
                    off = off + OVERFLOW_LEN
                elseif pinfo.can_desegment > 0 then
                    -- 帧跨 TCP 段：从 off 起请求重组
                    pinfo.desegment_offset = off - 1
                    pinfo.desegment_len = DESEGMENT_ONE_MORE_SEGMENT
                    flush_info(pinfo, c2s, infos)
                    return
                else
                    emit_frame(root, tvb, "incomplete", off - 1, n - 1,
                               decode_str(raw:sub(off)), infos)
                    off = n + 1
                end
            end
        end
    end

    flush_info(pinfo, c2s, infos)
    return n
end

-- 注册到服务器端口
local tcp_table = DissectorTable.get("tcp.port")
tcp_table:add(SERVER_PORT, jc)
