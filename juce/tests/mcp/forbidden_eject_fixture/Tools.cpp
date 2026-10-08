// A fixture for the source check, never compiled: a tool file that ejects a disk, which nothing in the server may do (the owner's decision of
// 2026-10-07). `CheckNoDestructiveCalls.cmake` must fail on it. [TASK-MCP-054, RQ-MCP-055]
void ejectFromATool(akm::Session& session, akm::CommandCompletion done)
{
    akm::ejectDisk(session, 0, done);
}
