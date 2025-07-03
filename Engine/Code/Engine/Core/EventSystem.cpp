#include "Engine/Core/EventSystem.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/StringUtils.hpp"

EventSystem::EventSystem(EventSystemConfig const& config)
	:m_config(config)
{
}

void EventSystem::Startup()
{
}

void EventSystem::ShutDown()
{
}

void EventSystem::BeginFrame()
{
}

void EventSystem::EndFrame()
{
}

void EventSystem::SubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function)
{
	EventSubscription newSubscription;
	newSubscription.m_callbackFunction = function;
	std::string eventNameLowerCase = GetLowercase(eventName);
	m_subscriptionListByEventName[eventNameLowerCase].push_back(newSubscription);

	EventInfo eventInfo;
	eventInfo.m_name = eventNameLowerCase;
	eventInfo.m_presentationName = eventName;
	m_eventNameInfoPair[eventNameLowerCase] = eventInfo;
}

void EventSystem::SubscribeEventCallbackFunction(std::string const& eventName, Strings const& argumentFormats, EventCallbackFunction* function)
{
	EventSubscription newSubscription;
	newSubscription.m_callbackFunction = function;
	std::string eventNameLowerCase = GetLowercase(eventName);
	m_subscriptionListByEventName[eventNameLowerCase].push_back(newSubscription);

	EventInfo eventInfo;
	eventInfo.m_name = eventNameLowerCase;
	eventInfo.m_presentationName = eventName;
	for (int argumentNum = 0; argumentNum < (int)argumentFormats.size(); ++argumentNum)
	{
		eventInfo.m_argumentFormats.push_back(argumentFormats[argumentNum]);
	}
	m_eventNameInfoPair[eventNameLowerCase] = eventInfo;
}

void EventSystem::UnsubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function)
{
	std::string eventNameLowerCase = GetLowercase(eventName);
	auto subscriptionListIter = m_subscriptionListByEventName.find(eventNameLowerCase);
	if (subscriptionListIter == m_subscriptionListByEventName.end())
		return;
	
	SubscriptionList& subList = subscriptionListIter->second;
	for (int functionNum = 0; functionNum < 
		(subList.size()); ++functionNum)
	{
		if (subList[functionNum].m_callbackFunction == function)
		{
			subList.erase(subList.begin() + functionNum);
			return;
		}
	}
}

bool EventSystem::FireEvent(std::string const& eventName, EventArgs& args)
{
	std::string eventNameLowerCase = GetLowercase(eventName);
	auto subscriptionListIter = m_subscriptionListByEventName.find(eventNameLowerCase);
	if (subscriptionListIter == m_subscriptionListByEventName.end())
		return false;

	SubscriptionList& subList = subscriptionListIter->second;
	for (int functionNum = 0; functionNum < (int)(subList.size()); ++functionNum)
	{
		bool callbackReturn = subList[functionNum].m_callbackFunction(args);
		if (callbackReturn)
			return true;
	}

	return false;
}

bool EventSystem::FireEvent(std::string const& eventName)
{
	std::string eventNameLowerCase = GetLowercase(eventName);
	auto subscriptionListIter = m_subscriptionListByEventName.find(eventNameLowerCase);
	if (subscriptionListIter == m_subscriptionListByEventName.end())
		return false;
		

	SubscriptionList& subList = subscriptionListIter->second;
	EventArgs args;
	for (int functionNum = 0; functionNum < (int)(subList.size()); ++functionNum)
	{
		bool callbackReturn = subList[functionNum].m_callbackFunction(args);
		if (callbackReturn)
			return true;
	}

	return false;
}

bool EventSystem::IsValidEvent(std::string eventInfo) const
{
	Strings eventInfoSplit = SplitStringOnDelimiter(eventInfo, ' ', false);
	if (eventInfoSplit.size() <= 0)
		return false;

	std::string eventNameLowerCase = GetLowercase(eventInfoSplit[0]);
	
	auto subscriptionListIter = m_subscriptionListByEventName.find(eventNameLowerCase);
	return (subscriptionListIter != m_subscriptionListByEventName.end());
}

Strings EventSystem::GetAllRegisteredEventNames(bool getPresentationName) const
{
	std::vector<std::string> eventNames;

	for (const auto& [key, value] : m_subscriptionListByEventName)
	{
		auto found = m_eventNameInfoPair.find(key);
		if (found != m_eventNameInfoPair.end())
		{
			if (getPresentationName)
			{
				eventNames.push_back(found->second.m_presentationName);
			}

			else
			{
				eventNames.push_back(found->second.m_name);
			}
			continue;
		}

		eventNames.push_back(key);
	}

	return eventNames;
}

bool EventSystem::GetArgumentFormatsForEventName(std::string const& eventName, Strings& out_strings) const
{
	std::string eventNameLowerCase = GetLowercase(eventName);
	auto found = m_eventNameInfoPair.find(eventNameLowerCase);
	if (found != m_eventNameInfoPair.end())
	{
		Strings argumentFormats = found->second.m_argumentFormats;
		if (argumentFormats.size() > 0)
		{
			for (int argumentNum = 0; argumentNum < (int)argumentFormats.size(); ++argumentNum)
			{
				argumentFormats[argumentNum].insert(0, " ");
				argumentFormats[argumentNum].append(" ");
			}

			out_strings = argumentFormats;
			return true;
		}
		return false;
	}
	return false;
}




//Standalone Functions
//----------------------------------------------------------------------------------------------------------------
void SubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function)
{
	if (g_eventSystem != nullptr)
	{
		g_eventSystem->SubscribeEventCallbackFunction(eventName, function);
	}
}

void SubscribeEventCallbackFunction(std::string const& eventName, Strings const& argumentFormats, EventCallbackFunction* function)
{
	if (g_eventSystem != nullptr)
	{
		g_eventSystem->SubscribeEventCallbackFunction(eventName, argumentFormats, function);
	}
}

void UnsubscribeEventCallbackFunction(std::string const& eventName, EventCallbackFunction* function)
{
	if (g_eventSystem != nullptr)
	{
		g_eventSystem->UnsubscribeEventCallbackFunction(eventName, function);
	}
}

bool FireEvent(std::string const& eventName, EventArgs& args)
{
	if (g_eventSystem != nullptr)
	{
		return g_eventSystem->FireEvent(eventName, args);
	}

	return false;
}

bool FireEvent(std::string const& eventName)
{
	if (g_eventSystem != nullptr)
	{
		return g_eventSystem->FireEvent(eventName);
	}
	
	return false;
}
