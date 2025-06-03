/*
* This file and associated .cpp file are licensed under the GPLv3 License Copyright (c) 2025 Sam Groveman
* 
* External libraries needed:
* ArduinoJSON: https://arduinojson.org/
*
* Contributors: Sam Groveman
*/
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <array>
#include <Actor.h>
#include <ActionTrigger.h>
#include <DigitalInputTrigger.h>

/// @brief Class providing action triggers at regular intervals
class InterruptActionTrigger : public Actor, public DigitalInputTrigger {
	protected:
		/// @brief Output configuration
		struct {
			/// @brief Name of the actor/action to use
			String action = "";

			/// @brief The payload to deliver to the action
			String payload = "";
		} trigger_config;

		/// @brief Path to configuration file
		String config_path;

		/// @brief Trigger to access actions
		ActionTrigger actionTrigger;

		/// @brief Stores the action that's to be triggered
		std::array<String, 2> action;

		/// @brief Thread handle for trigger processor
		TaskHandle_t triggerProcessorTask = nullptr;
		
		/// @brief Mutex for task name changes
		portMUX_TYPE taskMux = portMUX_INITIALIZER_UNLOCKED;

		bool configureOutput();
		bool triggerAction(String payload);
		JsonDocument addAdditionalConfig();

		/// @brief Fast check if interrupt triggered without clearing
		inline bool isTriggered() const noexcept {
			return triggered.load(std::memory_order_acquire);
		}

		static void processTrigger(void* context);
		bool startTriggerProcessor();
		bool updateTaskName();

	public:
		InterruptActionTrigger(String Name, int Pin, String configFile = "PeriodicActionTrigger.json");
		bool begin();
		std::tuple<bool, String> receiveAction(int action, String payload = "");
		String getConfig();
		bool setConfig(String config, bool save);
};