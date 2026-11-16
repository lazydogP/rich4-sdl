#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#define RICH4_PYTHON_COMMAND "python"
#else
#define RICH4_PYTHON_COMMAND "python3"
#endif

int main(void)
{
    int status = system(
        RICH4_PYTHON_COMMAND
        " tests/architecture/check_renderer_viewmodel_boundary.py");
    if (status != 0) return 1;
    puts("renderer ViewModel boundary: ok");
    return 0;
}
