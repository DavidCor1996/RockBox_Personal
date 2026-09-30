#!/usr/bin/env python3
"""Behavioral checks for upstream string-search and value-formatting fixes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[2]
SOURCE = Path(os.environ.get("ROCKBOX_TEST_SOURCE", REPO))


class SmallUpstreamFixTests(unittest.TestCase):
    def execute(self, code, extra_sources=(), flags=(), rockbox_ctype=False):
        with tempfile.TemporaryDirectory(prefix="rb-small-fixes-") as directory:
            work = Path(directory)
            unit = work / "test.c"
            unit.write_text(code)
            if rockbox_ctype:
                shutil.copyfile(REPO / "firmware/libc/include/ctype.h",
                                work / "ctype.h")
                shutil.copyfile(REPO / "firmware/include/_ansi.h",
                                work / "_ansi.h")
            command = ["cc", "-std=gnu99", "-Os", "-fno-builtin",
                       "-fsanitize=undefined", "-fno-sanitize-recover=all",
                       "-I" + str(work), *flags, str(unit),
                       *map(str, extra_sources), "-o", str(work / "test")]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            tested = subprocess.run([str(work / "test")],
                                    capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stderr)

    def test_utf8_search_both_char_signednesses(self):
        code = r'''
#include <assert.h>
#include <stddef.h>
char *rockbox_strcasestr(const char *, const char *);
int main(void)
{
    const char *ascii = "Album TITLE";
    const char *accented = "xx Bj\303\266rk xx";
    const char *japanese = "xx \346\235\261\344\272\254 xx";
    assert(rockbox_strcasestr(ascii, "title") == ascii + 6);
    assert(rockbox_strcasestr(accented, "bj\303\266RK") == accented + 3);
    assert(rockbox_strcasestr(accented, "\303\266r") == accented + 5);
    assert(rockbox_strcasestr(japanese, "\346\235\261\344\272\254") == japanese + 3);
    assert(rockbox_strcasestr(accented, "") == accented);
    assert(rockbox_strcasestr("", "x") == NULL);
    assert(rockbox_strcasestr(accented, "absent") == NULL);
    /* Non-ASCII case folding is deliberately outside this byte search. */
    assert(rockbox_strcasestr("\303\266", "\303\226") == NULL);
    return 0;
}
'''
        for char_flag in ("-fsigned-char", "-funsigned-char"):
            with self.subTest(char_flag=char_flag):
                self.execute(code,
                             [SOURCE / "firmware/common/strcasestr.c",
                              REPO / "firmware/libc/ctype.c"],
                             [char_flag, "-Dstrcasestr=rockbox_strcasestr"],
                             rockbox_ctype=True)

    def test_integer_and_table_value_formatting(self):
        text = (SOURCE / "apps/gui/option_select.c").read_text()
        start = text.index("else if ((HASFLAG(setting, F_INT_SETTING))")
        end = text.index("    else if (HASFLAG(setting, F_T_SOUND))", start)
        branch = text[start:end].removeprefix("else ")
        self.execute(r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define HASFLAG(s, f) (((s)->flags & (f)) == (f))
enum { F_INT_SETTING = 1, F_TABLE_SETTING = 2, F_TIME_SETTING = 4 };
typedef const char *(*formatter_t)(char *, size_t, int, const char *);
struct int_setting { formatter_t formatter; int unit; };
struct table_setting { formatter_t formatter; int unit; };
struct settings_list {
    unsigned flags;
    const struct int_setting *int_setting;
    const struct table_setting *table_setting;
};
static const char *unit_strings_core[] = { NULL, "", "dB" };
static const char *option_get_timestring(char *buf, int size, long val, int unit)
{ (void)val; (void)unit; snprintf(buf, size, "time"); return buf; }
static const char *custom(char *buf, size_t size, int val, const char *unit)
{ (void)buf; (void)size; (void)val; (void)unit; return "custom"; }
static const char *format(const struct settings_list *setting,
                          char *buffer, int buf_len, intptr_t temp_var)
{
    const char *str = buffer;
''' + branch + r'''
    return str;
}
int main(void)
{
    char buf[32];
    struct int_setting integer = { NULL, 0 };
    struct table_setting table = { NULL, 0 };
    struct settings_list s = { F_INT_SETTING, &integer, &table };
    for (unsigned kind = F_INT_SETTING; kind <= F_TABLE_SETTING; ++kind)
    {
        s.flags = kind;
        integer.unit = table.unit = 0;
        assert(strcmp(format(&s, buf, sizeof(buf), 7), "7") == 0);
        integer.unit = table.unit = 1;
        assert(strcmp(format(&s, buf, sizeof(buf), -12), "-12") == 0);
        assert(strcmp(format(&s, buf, sizeof(buf), 0), "0") == 0);
        integer.unit = table.unit = 2;
        assert(strcmp(format(&s, buf, sizeof(buf), -12), "-12 dB") == 0);
        assert(strcmp(format(&s, buf, 3, 12345), "12") == 0);
        assert(strcmp(format(&s, buf, 1, 7), "") == 0);
        buf[0] = 'x'; format(&s, buf, 0, 7); assert(buf[0] == 'x');
        s.flags |= F_TIME_SETTING;
        assert(strcmp(format(&s, buf, sizeof(buf), 5), "time") == 0);
        integer.formatter = table.formatter = custom;
        assert(strcmp(format(&s, buf, sizeof(buf), 5), "custom") == 0);
        s.flags = kind;
        assert(strcmp(format(&s, buf, sizeof(buf), 5), "custom") == 0);
        integer.formatter = table.formatter = NULL;
    }
    return 0;
}
''')


if __name__ == "__main__":
    unittest.main()
