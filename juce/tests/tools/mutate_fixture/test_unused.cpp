#include "fixture.hpp"

// Never calls multiply(): an alteration to it is missed here on purpose, to prove the tool reports
// "missed" when the tests selected for an alteration do not exercise the changed code (RQ-BLD-015).
int main()
{
    return add(1, 1) == 2 ? 0 : 1;
}
