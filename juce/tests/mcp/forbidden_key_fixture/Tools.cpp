// A fixture for the source check, never compiled: a tool file that calls a front-panel primitive of the AKM layer, which only the gateway's keys
// unit may do (a key can answer "ENT" to a delete or save screen). `CheckNoDestructiveCalls.cmake` must fail on it. [TASK-MCP-053, RQ-MCP-054, RQ-MCP-057]
void pressFromATool(akm::Session& session, akm::CommandCompletion done)
{
    akm::holdKey(session, akm::FrontPanelKey::EntPlay, done);
}
