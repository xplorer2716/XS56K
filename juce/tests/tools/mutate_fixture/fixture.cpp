#include "fixture.hpp"

int add(int a, int b)
{
    return a + b;
}

int multiply(int a, int b)
{
    return a * b;
}

namespace
{
    bool done = true;
}

// [RQ-BLD-015]
void waitUntilDone()
{
    while (!done)
    {
    }
}
