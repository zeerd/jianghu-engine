/**
 * @file CreateDump.h
 * @brief Windows 异常 Dump 文件生成器。
 */
#pragma once
#include <string>
using namespace std;
class CCreateDump {
public:
    CCreateDump();
    ~CCreateDump(void);
    static CCreateDump* instance();
    static long __stdcall UnhandleExceptionFilter(
        _EXCEPTION_POINTERS* ExceptionInfo);
    // 声明Dump文件，异常时会自动生成。会自动加入.dmp文件名后缀
    void declar_dump_file(std::string dmpFileName = "");

private:
    static std::string strDumpFile;
    static CCreateDump* __instance;
};
