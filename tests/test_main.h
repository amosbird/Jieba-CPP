#pragma once

// Minimal test helper — no external framework.
// Use CHECK(cond) / CHECK_EQ(a, b); a failing check prints context and the
// program exits non-zero at the end of main via test_failures().

#include <cstdio>
#include <string>
#include <vector>

namespace jiebacpp_test
{

inline int & failures()
{
    static int n = 0;
    return n;
}

inline void report(const char * file, int line, const char * expr)
{
    std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expr);
    ++failures();
}

template <typename A, typename B>
inline void report_eq(const char * file, int line, const char * ea, const char * eb, const A & a, const B & b)
{
    std::fprintf(stderr, "FAIL %s:%d: %s == %s\n", file, line, ea, eb);
    ++failures();
    (void)a;
    (void)b;
}

// Pretty-print a vector<string_view> as ['a','b',...] for diagnostics.
inline std::string join(const std::vector<std::string_view> & v)
{
    std::string s = "[";
    for (size_t i = 0; i < v.size(); ++i)
    {
        if (i)
            s += ',';
        s += '\'';
        s.append(v[i].data(), v[i].size());
        s += '\'';
    }
    s += ']';
    return s;
}

} // namespace jiebacpp_test

#define CHECK(cond) \
    do { if (!(cond)) ::jiebacpp_test::report(__FILE__, __LINE__, #cond); } while (0)

#define CHECK_EQ(a, b) \
    do { if (!((a) == (b))) ::jiebacpp_test::report_eq(__FILE__, __LINE__, #a, #b, (a), (b)); } while (0)

#define TEST_MAIN_RETURN() return ::jiebacpp_test::failures() == 0 ? 0 : 1
