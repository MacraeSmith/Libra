#pragma once
#include "Engine/Core/NamedStrings.hpp"
#include <string>
#include <vector>
#include <map>

typedef NamedStrings EventArgs; //#TODO: change to NamedProperties in SD4
typedef bool(EventCallbackFunction)(EventArgs& args);

struct EventSystemConfig
{
};

enum class EventSubscriptionType : int 
{
	STATIC,
	STANDALONE,
};
struct EventSubscription
{
	EventCallbackFunction* m_callbackFunction;
	EventSubscriptionType m_eventSubscriptionType;
};

typedef std::vector<EventSubscription> SubscriptionList;

struct EventInfo
{
	std::string m_name;
	std::string m_presentationName;
	Strings m_argumentFormats;
};

class EventSystem
{
public:
	explicit EventSystem(EventSystemConfig const& config);
	~EventSystem(){}

	void Startup();
	void ShutDown();
	void BeginFrame();
	void EndFrame();

	void SubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function);
	void SubscribeEventCallbackFunction(std::string const& eventName, Strings const& argumentFormats, EventCallbackFunction* function);
	void UnsubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function);
	bool FireEvent(std::string const& eventName, EventArgs& args);
	bool FireEvent(std::string const& eventName);

	bool IsValidEvent(std::string eventInfo) const;
	Strings GetAllRegisteredEventNames(bool getPresentationName = false) const;
	bool GetArgumentFormatsForEventName(std::string const& eventName, Strings& out_strings) const;

protected:


protected:
	EventSystemConfig m_config;
	std::map<std::string, SubscriptionList> m_subscriptionListByEventName;
	std::map<std::string, EventInfo> m_eventNameInfoPair;
};

void SubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function);
void SubscribeEventCallbackFunction(std::string const& eventName, Strings const& argumentFormats, EventCallbackFunction* function);
void UnsubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function);
bool FireEvent(std::string const& eventName, EventArgs& args);
bool FireEvent(std::string const& eventName);
