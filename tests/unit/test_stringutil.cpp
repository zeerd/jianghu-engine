// test_stringutil.cpp - Unit tests for StringUtil functions.
//
// Build+Run: .\run-coverage.ps1
//
#include "pch.h"
#include <cstring>
#include <string>

#include "IniParser.h"

// --- Forward declare functions under test ---
void analyse_string(char* Comm, char* arg, int len);
void analyse_string(std::string& comm, std::string& arg, size_t len);
char* get_value(const char* cmd_str, const char* keyword, char* pValue,
                int nValueLen);
void normalize_password(char* pass);
char* decode_str(char* msg);
std::string truncated_copy(const char* src, size_t max_len);

namespace {

// 内置默认密钥（与 StringUtil 的 kDefaultCommKey 同值）
const char kDefaultCommKey[] = "\15L\26A\18N\12W\25O\33O\6D";

// 镜像 decode_str 的 XOR 变换：空格/换行/制表符原样保留、同样推进密钥
// 游标——用它构造"按某密钥编码"的帧，再交给 decode_str 验证用的是哪把键
std::string EncodeWithKey(const std::string& input, const std::string& key)
{
    std::string out = input;
    int t           = 0;
    for (size_t p = 0; p < out.size(); p++) {
        if (out[p] != ' ' && out[p] != '\n' && out[p] != '\t') {
            out[p] ^= key[t];
            if (out[p] == '\t') out[p] = '\t' ^ key[t];
            if (out[p] == '\n') out[p] = '\n' ^ key[t];
            if (out[p] == ' ') out[p] = ' ' ^ key[t];
            if (out[p] == '\0') out[p] = '\0' ^ key[t];
        }
        t++;
        if (t >= (int)key.size()) t = 0;
    }
    return out;
}

// 把 C 字符串按 decode_str 的原地契约解码，返回结果
std::string Decode(const std::string& frame)
{
    std::string buf = frame;
    decode_str(&buf[0]);
    return buf;
}

const char* const kSampleFrame = "login 127.0.0.1 v1 1001 secret1 x";

}  // namespace

// ====================================================================
// analyse_string: split command and arguments
// ====================================================================
TEST(StringUtil, AnalyseString_Basic)
{
    char buf[]   = "kill monster";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "kill");
    EXPECT_STREQ(arg, "monster");
}

TEST(StringUtil, AnalyseString_NoArg)
{
    char buf[]   = "look";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "look");
    EXPECT_STREQ(arg, "");
}

TEST(StringUtil, AnalyseString_MultipleArgs)
{
    char buf[]   = "say hello world";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "say");
    EXPECT_STREQ(arg, "hello world");
}

TEST(StringUtil, AnalyseString_EmptyString)
{
    char buf[]   = "";
    char arg[64] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "");
    EXPECT_STREQ(arg, "");
}

TEST(StringUtil, AnalyseString_ArgTruncated)
{
    char buf[]  = "cmd shortarg";
    char arg[4] = "";
    analyse_string(buf, arg, sizeof(arg));
    EXPECT_STREQ(buf, "cmd");
    EXPECT_STREQ(arg, "sho");  // truncated to nLen-1
}

// ====================================================================
// analyse_string（std::string 形态）：同一劈分契约，arg 截到 len-1
// ====================================================================

TEST(AnalyseStringStd, SplitsFirstSpace)
{
    std::string comm = "kill monster";
    std::string arg;
    analyse_string(comm, arg, 64);
    EXPECT_EQ(comm, "kill");
    EXPECT_EQ(arg, "monster");
}

TEST(AnalyseStringStd, NoSpaceKeepsCommAndClearsArg)
{
    std::string comm = "look";
    std::string arg  = "stale";
    analyse_string(comm, arg, 64);
    EXPECT_EQ(comm, "look");
    EXPECT_EQ(arg, "");
}

TEST(AnalyseStringStd, KeepsRemainingArgsIntact)
{
    std::string comm = "say hello world";
    std::string arg;
    analyse_string(comm, arg, 64);
    EXPECT_EQ(comm, "say");
    EXPECT_EQ(arg, "hello world");
}

TEST(AnalyseStringStd, ArgTruncatedAtLenMinusOne)
{
    std::string comm = "cmd shortarg";
    std::string arg;
    analyse_string(comm, arg, 4);
    EXPECT_EQ(comm, "cmd");
    EXPECT_EQ(arg, "sho") << "与就地版同轨：arg 截到 len-1";
}

TEST(AnalyseStringStd, LeadingSpaceSplitsToEmptyComm)
{
    std::string comm = " look";
    std::string arg;
    analyse_string(comm, arg, 64);
    EXPECT_EQ(comm, "");
    EXPECT_EQ(arg, "look");
}

TEST(AnalyseStringStd, ZeroLenClearsArg)
{
    std::string comm = "cmd tail";
    std::string arg  = "x";
    analyse_string(comm, arg, 0);
    EXPECT_EQ(comm, "cmd") << "头部照常切出";
    EXPECT_EQ(arg, "");
}

// ====================================================================
// GetValue: parse &key=value& from a command string
// ====================================================================
TEST(StringUtil, GetValue_Basic)
{
    char val[64];
    const char* cmd = "&C=10&M=hello&";
    get_value(cmd, "C", val, sizeof(val));
    EXPECT_STREQ(val, "10");
    get_value(cmd, "M", val, sizeof(val));
    EXPECT_STREQ(val, "hello");
}

TEST(StringUtil, GetValue_MissingKey)
{
    char val[64];
    const char* cmd = "&C=10&M=hello&";
    get_value(cmd, "X", val, sizeof(val));
    EXPECT_STREQ(val, "");
}

TEST(StringUtil, GetValue_NumericValue)
{
    char val[64];
    const char* cmd = "&P=256&N=test_item&";
    get_value(cmd, "P", val, sizeof(val));
    EXPECT_STREQ(val, "256");
}

TEST(StringUtil, GetValue_Truncated)
{
    char val[4];
    const char* cmd = "&M=longvalue&";
    get_value(cmd, "M", val, sizeof(val));
    EXPECT_STREQ(val, "lon");  // truncated to nValueLen-1
}

// ====================================================================
// normalize_password: 密码契约——剥除单引号（与登录口剥除语义一致）
// ====================================================================
TEST(StringUtil, NormalizePassword_RemovesQuotes)
{
    char pass[] = "ab'c'd";
    normalize_password(pass);
    EXPECT_STREQ(pass, "abcd");
}

TEST(StringUtil, NormalizePassword_NoQuoteUnchanged)
{
    char pass[] = "plainpass";
    normalize_password(pass);
    EXPECT_STREQ(pass, "plainpass");
}

TEST(StringUtil, NormalizePassword_AllQuotes)
{
    char pass[] = "'''''";
    normalize_password(pass);
    EXPECT_STREQ(pass, "");
}

TEST(StringUtil, NormalizePassword_EmptyAndNull)
{
    char empty[] = "";
    normalize_password(empty);
    EXPECT_STREQ(empty, "");
    normalize_password(nullptr);  // 不得崩溃
}

// ====================================================================
// decode_str：通讯帧 XOR 自逆，密钥取 [security] comm_key
// ====================================================================

// 未配置密钥（无 [security] 节）→ 内置默认密钥：默认键编码的帧可解，
// 别的键编出来的帧解不开（否则"回退默认"只是句空话）
TEST(StringUtil, DecodeStr_FallsBackToDefaultKeyWithoutConfig)
{
    ini_load("tests/unit/test_config.ini");

    const std::string plain = kSampleFrame;
    EXPECT_EQ(Decode(EncodeWithKey(plain, kDefaultCommKey)), plain);
    EXPECT_NE(Decode(EncodeWithKey(plain, "ConfigKey123")), plain)
        << "配置键不得在未配置时生效";
}

// 配置了 comm_key → 收发两侧都按配置键变换
TEST(StringUtil, DecodeStr_UsesConfiguredCommKey)
{
    ini_load("tests/unit/test_comm_key.ini");

    const std::string plain = kSampleFrame;
    EXPECT_EQ(Decode(EncodeWithKey(plain, "ConfigKey123")), plain);
    EXPECT_NE(Decode(EncodeWithKey(plain, kDefaultCommKey)), plain)
        << "配置后默认键必须失效，否则密钥形同虚设";

    ini_load("tests/unit/test_config.ini");  // 恢复，不污染后续用例
}

// 空值（comm_key = ）等同于未配置：空密钥会让密钥游标恒停在 [0] 越界
TEST(StringUtil, DecodeStr_EmptyCommKeyFallsBackToDefault)
{
    ini_load("tests/unit/test_config.ini");
    g_ini_data["security"]["comm_key"] = "";

    const std::string plain = kSampleFrame;
    EXPECT_EQ(Decode(EncodeWithKey(plain, kDefaultCommKey)), plain);

    g_ini_data["security"].erase("comm_key");
}

// 空格/换行/制表符不参与异或、但照常推进密钥游标——换一把键后这些位置
// 与后续字节全部错位，往返必须仍然自逆
TEST(StringUtil, DecodeStr_RoundTripsFrameWithWhitespace)
{
    ini_load("tests/unit/test_comm_key.ini");

    const std::string plain = "look\tsay hello world\nnorth";
    EXPECT_EQ(Decode(EncodeWithKey(plain, "ConfigKey123")), plain);

    ini_load("tests/unit/test_config.ini");
}

// ====================================================================
// truncated_copy：定宽字段装配——超长按字段宽度截尾（宽度是协议契约）
// ====================================================================

// 短于/等于宽度的源串原样返回（宽度边界本身不构成截断）
TEST(StringUtil, TruncatedCopy_WithinWidthUnchanged)
{
    EXPECT_EQ(truncated_copy("百姓", 19), "百姓");
    EXPECT_EQ(truncated_copy("exactly-10", 10), "exactly-10");
    EXPECT_EQ(truncated_copy("", 19), "");
}

// 超宽源串截到字段宽度为止
TEST(StringUtil, TruncatedCopy_TruncatesBeyondWidth)
{
    EXPECT_EQ(truncated_copy("longer than ten", 10), "longer tha");
}

// 空指针按空串处理：ini/dbase 取值可能给出空指针，不得解引用崩溃
TEST(StringUtil, TruncatedCopy_NullptrYieldsEmpty)
{
    EXPECT_EQ(truncated_copy(nullptr, 19), "");
}

// 按字节截尾，可能截在多字节字符中间——与历史定宽字段口径一致。
// 输入用显式字节构造：窄字面量按 /execution-charset:gbk 落二进制，断言
// 只谈字节语义才不受编码取值影响。
TEST(StringUtil, TruncatedCopy_TruncatesByBytes)
{
    const std::string two_multibyte("\xE7\x99\xBE\xE5\xA8\x83", 6);
    EXPECT_EQ(truncated_copy(two_multibyte.c_str(), 4),
              two_multibyte.substr(0, 4))
        << "必须按字节截到 4，而不是按字符边界截到 3";
}

// ====================================================================
// cc_strcmp：两侧任一为空的定序
// ====================================================================
int cc_strcmp(const char* str1, const char* str2);

TEST(StringUtil, CcStrcmp_NullSides)
{
    EXPECT_EQ(cc_strcmp(nullptr, "b"), -1);
    EXPECT_EQ(cc_strcmp("a", nullptr), 1);
    EXPECT_EQ(cc_strcmp("a", "a"), 0);
    EXPECT_LT(cc_strcmp("a", "b"), 0);
    EXPECT_GT(cc_strcmp("b", "a"), 0);
}
