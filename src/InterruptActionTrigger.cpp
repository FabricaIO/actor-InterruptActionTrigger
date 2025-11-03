#include"InterruptActionTrigger.h"

extern bool POSTSuccess;

/// @brief Creates a periodic action trigger
/// @param Name The device name
/// @param Pin Pin to use
/// @param configFile Name of the config file to use
InterruptActionTrigger::InterruptActionTrigger(String Name, int Pin, String configFile) : Actor(Name), DigitalInputTrigger(Pin) {
	config_path = "/settings/act/" + configFile;
}

/// @brief Starts a periodic trigger 
/// @return True on success
bool InterruptActionTrigger::begin() {
	// Set description
	Description.type = "trigger";
	Description.actions = {{"triggeraction", 0}};
	actionTrigger.actions_config.Enabled = true;
	bool success = false;
	if (DigitalInputTrigger::begin()) {
		// Create settings directory if necessary
		if (!checkConfig(config_path)) {
			// Set defaults
			digital_config.id = 0;
			digital_config.mode = "INPUT";
			digital_config.taskEnabled = false;
			digital_config.trigger = "NONE";
			success = setConfig(getConfig(), true);
		} else {
			// Load settings
			success = setConfig(Storage::readFile(config_path), false);
		}
	}
	if (success) {
		if (!startTriggerProcessor()) {
			Logger.println("Failed to start trigger processor thread");
			return false;
		}
	}
	return success;
}

/// @brief Receives an action
/// @param action The action to process (only option is 0 for set output)
/// @param payload Any payload to be passed to the action
/// @return JSON response with OK
std::tuple<bool, String> InterruptActionTrigger::receiveAction(int action, String payload) {
	if (action == 0) {
		if (triggerAction(payload)) {
			return { true, R"({"success": true})" };
		} else {
			return { true, R"({"success": false})" };
		}
	}	
	return { true, R"({"success": false})" };
}

/// @brief Gets the current config
/// @return A JSON string of the config
String InterruptActionTrigger::getConfig() {
	// Allocate the JSON document
	JsonDocument doc = addAdditionalConfig();

	// Add name
	doc["Name"] = Description.name;

	// Create string to hold output
	String output;
	
	// Serialize to string
	serializeJson(doc, output);
	return output;
}

/// @brief Sets the configuration for this device
/// @param config A JSON string of the configuration settings
/// @param save If the configuration should be saved to a file
/// @return True on success
bool InterruptActionTrigger::setConfig(String config, bool save) {
	if (DigitalInputTrigger::setConfig(config)) {
		// Allocate the JSON document
		JsonDocument doc;
		// Deserialize file contents
		DeserializationError error = deserializeJson(doc, config);
		// Test if parsing succeeds.
		if (error) {
			Logger.print(F("Deserialization failed: "));
			Logger.println(error.f_str());
			return false;
		}
		
		// Get new name
		String newName = doc["Name"].as<String>();
		
		// Update name and task if changed
		if (newName != Description.name) {
			Description.name = newName;
			if (triggerProcessorTask != nullptr && !updateTaskName()) {
				Logger.println("Failed to update task name");
				return false;
			}
		}

		// Parse actor and action
		trigger_config.action = doc["Action"]["current"].as<String>();
		int colon;
		if ((colon = trigger_config.action.indexOf(':')) != -1) {
			std::pair<String, String> chosen {trigger_config.action.substring(0, colon), trigger_config.action.substring(colon + 1)};
			action[0] = chosen.first;
			action[1] = chosen.second;
		}
		if (save) {
			return saveConfig(config_path, config);
		}
		return true;
	}
	return false;
}

/// @brief Triggers the set action
/// @param payload A payload to pass to the action
/// @return True on success
bool InterruptActionTrigger::triggerAction(String payload) {
	if (POSTSuccess) {
		return actionTrigger.triggerActions({{action[0], {{action[1], payload}}}});
	}
	return false;
}

/// @brief Executes the action if interrupt is triggered
/// @param context Pointer to the object where the interrupt occurred
void InterruptActionTrigger::processTrigger(void* context) {
	InterruptActionTrigger* self = static_cast<InterruptActionTrigger*>(context);    
	while (true) {
		if (self->isTriggered()) {
			// Handle action trigger in thread context
			if (self->triggerAction(self->trigger_config.payload)) {
				Logger.println("Interrupt triggered in " + self->Description.name);
			}
			self->clearTrigger();
		}
		delay(5);
	}
}

/// @brief Starts the trigger processor thread
/// @return True on success
bool InterruptActionTrigger::startTriggerProcessor() {
	if (triggerProcessorTask == nullptr) {
		String taskName = "Trig_" + Description.name;
		BaseType_t result = xTaskCreate(processTrigger, taskName.c_str(), 2048, this, 1, &triggerProcessorTask);
		return result == pdPASS;
	}
	return false;
}

/// @brief Updates the name of the processor task
/// @return True on success
bool InterruptActionTrigger::updateTaskName() {
	portENTER_CRITICAL(&taskMux);
	TaskHandle_t oldTask = triggerProcessorTask;
	String taskName = "Trig_" + Description.name;
	BaseType_t result = xTaskCreate(processTrigger, taskName.c_str(), 2048, this, 1, &triggerProcessorTask);
	
	if (result == pdPASS) {
		// Delete old task if it exists
		if (oldTask != nullptr) {
			vTaskDelete(oldTask);
		}
		portEXIT_CRITICAL(&taskMux);
		return true;
	}
	// Restore old task handle on failure
	triggerProcessorTask = oldTask;
	portEXIT_CRITICAL(&taskMux);
	return false;
}

/// @brief Collects all the base class parameters and additional parameters
/// @return A JSON document with all the parameters
JsonDocument InterruptActionTrigger::addAdditionalConfig() {
	// Allocate the JSON document
	JsonDocument doc;
	// Deserialize file contents
	DeserializationError error = deserializeJson(doc, DigitalInputTrigger::getConfig());
	// Test if parsing succeeds.
	if (error) {
		Logger.print(F("Deserialization failed: "));
		Logger.println(error.f_str());
		return doc;
	}
	// Remove unneeded settings
	doc.remove("id");
	doc.remove("taskName");
	doc.remove("taskPeriod");
	doc.remove("taskEnabled");
	// Add all actors/actions to dropdown
	doc["Action"]["current"] = trigger_config.action;
	std::map<String, std::map<int, String>> actions = actionTrigger.listAllActions();
	if (actions.size() > 0) {
	int i = 0;
		for (std::map<String, std::map<int, String>>::iterator actor = actions.begin(); actor != actions.end(); actor++) {
			if (actor->first != Description.name) {
				for (const auto& action : actor->second) {
					doc["Action"]["options"][i] = actor->first + ":" + action.second;
					i++;
				}
			}
		}
	} else {
		doc["Action"]["options"][0] = "";
	}
	doc["Payload"] = trigger_config.payload;
	return doc;
}