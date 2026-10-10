// A fixture for the source check, never compiled: a tool file that calls a delete primitive of the AKM layer, which only the gateway's
// lists unit may do. `CheckNoDestructiveCalls.cmake` must fail on it. [TASK-MCP-048, RQ-MCP-049, RQ-MCP-057]
void deleteFromATool(akm::Session& session, akm::CommandCompletion done)
{
    akm::deleteSetList(session, 0, done);
}
