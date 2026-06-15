#include "agent_module_impl.h"

std::string AgentModuleImpl::greet(const std::string& name)
{
    std::string greeting = "Hello, " + name + "! Greetings from the agent module.";

    // The generated event body routes the typed payload to every subscriber.
    greeted(greeting);

    return greeting;
}

std::string AgentModuleImpl::getStatus()
{
    return "Agent module is running.";
}
