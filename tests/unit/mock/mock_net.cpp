// mock_net.cpp — Test-only stubs
//
// Message.cpp is now compiled directly (not mocked).
// Only test-specific stubs live here.
//
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>

// Forward declarations
class CContainer;
class CChar;
class CUser;

// ====================================================================
// Test control variables
// ====================================================================
bool g_test_suppress_output = false;
