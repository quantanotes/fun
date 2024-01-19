#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
    #define WINDOWS
    #define POPEN  _popen
    #define PCLOSE _pclose
    #include <windows.h>
#elif defined(__linux__)
    #define UNIX
    #define POPEN  popen
    #define PCLOSE pclose
#else
    #error unsupported os
#endif

char* compile(const char* src) {
    const char* cmd = "gcc -x c";

    FILE* pipe = POPEN(cmd, "w");
    if (pipe == NULL) {
        return NULL;
    }

    return NULL;
}

int main() {
    const char* hello_word =
        "#include<stdio.h>\nint main(){printf(\"Hello, World!\\n\");return 0;}";
    compile(hello_word);
    return 0;
}