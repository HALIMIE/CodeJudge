#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <unistd.h>
#include <errno.h>
#include "../defineshit.h"

#define TEMP_OUTPUT "temp/temp_output"
#define COMPILE_ERROR_FILE "temp/compile_error.txt"
#define IO_DIR "io"

/**
 * @brief Replace all occurrences of substring 'old' in 'str' with 'new_str'.
 *      The result is heap-allocated and should be freed by the caller.
 * @param str input string.
 * @param old substring to replace.
 * @param new_str new substring.
 * @return heap-allocated string with all occurrences of 'old' replaced by 'new_str'.
 */
char *replace_substring(const char *str, const char *old, const char *new_str)
{
    if (!str || !old || !*old)
        return strdup(str);

    size_t old_len = strlen(old);
    size_t new_len = strlen(new_str);
    size_t count = 0;
    const char *p = str;
    while ((p = strstr(p, old)) != NULL)
    {
        count++;
        p += old_len;
    }
    size_t new_size = strlen(str) + count * (new_len - old_len) + 1;
    char *result = malloc(new_size);
    if (!result)
        return NULL;

    char *r = result;
    while (*str)
    {
        const char *pos = strstr(str, old);
        if (pos)
        {
            size_t len = pos - str;
            memcpy(r, str, len);
            r += len;
            memcpy(r, new_str, new_len);
            r += new_len;
            str = pos + old_len;
        }
        else
        {
            strcpy(r, str);
            break;
        }
    }
    return result;
}

/**
 * @brief Sanitize an error message by masking file paths and file names.
 * @param msg error message to sanitize.
 * @return sanitized error message (heap-allocated), or NULL on error.
 */
char *sanitize_error_message(const char *msg)
{
    if (!msg)
        return NULL;

    const char *patterns[] = {
        "build/src/",     // program build path
        "files/receive/", // received file path
        "temp/",          // temporary file path
        NULL
    };

    char *sanitized = strdup(msg);
    if (!sanitized)
        return NULL;

    for (int i = 0; patterns[i] != NULL; i++)
    {
        char *temp = replace_substring(sanitized, patterns[i], "");
        free(sanitized);
        if (!temp)
            return NULL;
        sanitized = temp;
    }
    return sanitized;
}

/**
 * @brief Compile the submission.
 * @param source_path path to the source file.
 * @param executable_path path to the compiled executable.
 * @return 0 on success, non-zero on compile error.
 */
int compile_submission(const char *source_path, const char *executable_path)
{
    char command[512];
    // stderr to COMPILE_ERROR_FILE
    snprintf(command, sizeof(command), "gcc %s -o %s 2>%s", source_path, executable_path, COMPILE_ERROR_FILE);
    // printf("compile command: %s\n", command);
    int ret = system(command);
    return ret;
}

/**
 * @brief Run the compiled submission against a test case.
 *
 * @param in_path path to the input file.
 * @param expected_out path to the expected output file.
 * @param exec_time execution time in ms (output).
 * @param max_rss used memory (output).
 * @param executable_path path to the compiled executable.
 * @return 2 if test passed (Accepted),
 *         1 if output does not match (Wrong Answer),
 *        -1 if runtime error occurred.
 */
int run_test(const char *in_path, const char *expected_out, int *exec_time, long *max_rss, const char *executable_path)
{
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork failed");
        return -1;
    }
    else if (pid == 0)
    {
        FILE *fin = fopen(in_path, "r");
        if (!fin)
        {
            perror("fopen failed");
            exit(1);
        }
        int fd_in = fileno(fin);
        if (dup2(fd_in, STDIN_FILENO) == -1)
        {
            perror("dup2(stdin) failed");
            exit(1);
        }
        fclose(fin);

        FILE *fout = fopen(TEMP_OUTPUT, "w");
        if (!fout)
        {
            perror("fopen failed");
            exit(1);
        }
        int fd_out = fileno(fout);
        if (dup2(fd_out, STDOUT_FILENO) == -1)
        {
            perror("dup2(stdout) failed");
            exit(1);
        }
        if (dup2(fd_out, STDERR_FILENO) == -1)
        {
            perror("dup2(stderr) failed");
            exit(1);
        }
        fclose(fout);

        execl(executable_path, "solution", (char *)NULL);
        perror("execl failed");
        exit(1);
    }
    else
    {
        struct rusage usage;
        int status;
        if (wait4(pid, &status, 0, &usage) == -1)
        {
            perror("wait4 failed");
            return -1;
        }

        // check runtime error
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            fprintf(stderr, "Child process terminated abnormally: status = %d\n", status);
            return -1;
        }

        *max_rss = usage.ru_maxrss;
        int utime_ms = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000;
        int stime_ms = usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
        *exec_time = utime_ms + stime_ms;

        FILE *f1 = fopen(expected_out, "r");
        FILE *f2 = fopen(TEMP_OUTPUT, "r");
        if (!f1 || !f2)
        {
            perror("fopen failed");
            return -1;
        }
        int result = 1; // 1: output matches, 0: does not match.
        char buf1[1024], buf2[1024];
        while (fgets(buf1, sizeof(buf1), f1) && fgets(buf2, sizeof(buf2), f2))
        {
            if (strcmp(buf1, buf2) != 0)
            {
                result = 0;
                break;
            }
        }
        if (fgets(buf1, sizeof(buf1), f1) || fgets(buf2, sizeof(buf2), f2))
        {
            result = 0;
        }
        fclose(f1);
        fclose(f2);
        return (result ? 2 : 1);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <source_file_path>\n", argv[0]);
        return 1;
    }
    const char *source_path = argv[1];

    // extract base name from source file path
    const char *base = strrchr(source_path, '/');
    if (base)
        base++;
    else
        base = source_path;

    char base_name[256];
    strncpy(base_name, base, sizeof(base_name));
    base_name[sizeof(base_name) - 1] = '\0';
    char *dot = strrchr(base_name, '.');
    if (dot)
    {
        *dot = '\0';
    }
    char executable_path[256];
    snprintf(executable_path, sizeof(executable_path), "temp/%s", base_name);

    // compile the submission
    if (compile_submission(source_path, executable_path) != 0)
    {
        FILE *err_fp = fopen(COMPILE_ERROR_FILE, "r");
        if (err_fp)
        {
            char err_msg[4096] = {0};
            size_t n = fread(err_msg, 1, sizeof(err_msg) - 1, err_fp);
            fclose(err_fp);
            char *masked_msg = sanitize_error_message(err_msg);
            if (masked_msg)
            {
                printf("Compile Error:\n%s\n", masked_msg);
                free(masked_msg);
            }
            else
            {
                printf("Compile Error: (Could not sanitize error message)\n");
            }
        }
        else
        {
            printf("Compile Error: (Could not capture error message)\n");
        }
        return 1;
    }

    DIR *dir = opendir(IO_DIR);
    if (!dir)
    {
        perror("opendir failed");
        remove(executable_path);
        return 1;
    }

    struct dirent *entry;
    int max_total_time = 0;
    long max_total_rss = 0;
    int overall = 2; // 2: Accepted, 1: Wrong Answer, -1: Runtime Error
    char runtime_error_msg[4096] = {0};

    while ((entry = readdir(dir)) != NULL)
    {
        if (entry->d_type == DT_REG)
        {
            char *ext = strrchr(entry->d_name, '.');
            if (ext && strcmp(ext, ".in") == 0)
            {
                char in_path[256];
                snprintf(in_path, sizeof(in_path), "%s/%s", IO_DIR, entry->d_name);

                char expected_output[256];
                strncpy(expected_output, entry->d_name, sizeof(expected_output));
                expected_output[sizeof(expected_output) - 1] = '\0';
                char *dot = strrchr(expected_output, '.');
                if (dot)
                {
                    strcpy(dot, ".out");
                }
                char expected_path[256];
                snprintf(expected_path, sizeof(expected_path), "%s/%s", IO_DIR, expected_output);

                int exec_time = 0;
                long mem_usage = 0;
                int test_result = run_test(in_path, expected_path, &exec_time, &mem_usage, executable_path);
                if (test_result == -1)
                {
                    overall = -1;
                    FILE *rt_fp = fopen(TEMP_OUTPUT, "r");
                    if (rt_fp)
                    {
                        fread(runtime_error_msg, 1, sizeof(runtime_error_msg) - 1, rt_fp);
                        fclose(rt_fp);
                    }
                    break;
                }
                else if (test_result == 1)
                {
                    overall = (overall != -1 ? 1 : overall);
                }
                else  // test_result == 2 (Accepted)
                {
                    if (exec_time > max_total_time)
                        max_total_time = exec_time;
                    if (mem_usage > max_total_rss)
                        max_total_rss = mem_usage;
                }
            }
        }
    }
    closedir(dir);

    if (remove(executable_path) != 0)
    {
        perror("remove compiled executable failed");
    }

    if (overall == -1)
    {
        char *masked_msg = sanitize_error_message(runtime_error_msg);
        if (masked_msg)
        {
            printf("\nRuntime Error:\n%s\n", masked_msg);
            free(masked_msg);
        }
        else
        {
            printf("\nRuntime Error: (Could not sanitize error message)\n");
        }
    }
    else if (overall == 1)
    {
        printf("\nWrong Answer\n");
    }
    else if (overall == 2)
    {
        printf("\nAccepted\n");
        printf("time: %d ms, memory: %ld KB\n", max_total_time, max_total_rss);
    }

    return 0;
}
