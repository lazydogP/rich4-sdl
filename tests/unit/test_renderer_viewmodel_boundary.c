#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int status = system(
        "python tests/architecture/check_renderer_viewmodel_boundary.py");
    if (status != 0) return 1;
    puts("renderer ViewModel boundary: ok");
    return 0;
}
