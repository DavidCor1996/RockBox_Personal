/*
 * Copyright (C) 2026 The Rockpod contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(const char *text, size_t length)
{
    size_t offset = 0;

    while (offset < length) {
        ssize_t result = write(STDOUT_FILENO, text + offset, length - offset);

        if (result < 0 && errno == EINTR)
            continue;
        if (result <= 0)
            return -1;
        offset += (size_t)result;
    }
    return 0;
}

#define EMIT(text) do { \
    static const char message[] = text "\n"; \
    if (write_all(message, sizeof(message) - 1) != 0) \
        return 20; \
} while (0)

int main(void)
{
    static char *const framework_argv[] = {
        "dalvikvm",
        "-Xverify:none",
        "-Xdexopt:none",
        "-Xbootclasspath:/system/framework/core.jar:"
            "/system/framework/ext.jar:/system/framework/framework.jar:"
            "/system/framework/android.policy.jar",
        "-cp",
        "/system/framework/ipod6g-framework-fixture.jar:"
            "/system/framework/ipod6g-dalvik-fixture.jar",
        "ipod6g.frameworktest.FrameworkHello",
        NULL,
    };
    static char *const zygote_argv[] = {
        "app_process",
        "-Xzygote",
        "-Xverify:none",
        "-Xdexopt:none",
        "/system/bin",
        "--zygote",
        "--start-system-server",
        NULL,
    };
    pid_t child;
    pid_t waited;
    int status;

    EMIT("IPOD6G_ECLAIR_ZYGOTE_GATE:BEGIN");
    child = fork();
    if (child < 0)
        return 21;
    if (child == 0) {
        execv("/system/bin/dalvikvm", framework_argv);
        _exit(127);
    }
    do {
        waited = waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited != child || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        EMIT("IPOD6G_ECLAIR_ZYGOTE_GATE:FRAMEWORK_FAILED");
        return 22;
    }
    EMIT("IPOD6G_ECLAIR_ZYGOTE_GATE:FRAMEWORK_READY");
    execv("/system/bin/app_process", zygote_argv);
    EMIT("IPOD6G_ECLAIR_ZYGOTE_GATE:EXEC_FAILED");
    return 23;
}
