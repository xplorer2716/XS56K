#pragma once

int add(int a, int b);
int multiply(int a, int b);

// Loops until told to stop; an alteration that starts it stopped already makes the caller hang, for
// the mutation tool's timeout-caught case (RQ-BLD-015).
void waitUntilDone();
