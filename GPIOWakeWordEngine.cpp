#include "GPIOWakeWordEngine.h"
#include "Logger.h"
#include "WakeWordUtils.h"
#include "WakeWordException.h"
#include <iostream>
#include <unistd.h>

namespace AlexaWakeWord {

static const int SECONDS_BETWEEN_READINGS = 1;

GPIOWakeWordEngine::GPIOWakeWordEngine(WakeWordDetectedInterface* interface) :
    WakeWordEngine(interface), m_isRunning { false } {

  try {
    init();
  } catch (WakeWordException& e) {
    log(Logger::ERROR,
        std::string("GPIOWakeWordEngine: Initialization error:") + e.what());
    throw;
  }

}

void GPIOWakeWordEngine::pause() {

  log(Logger::INFO, "GPIOWakeWordEngine: handling pause");
}

void GPIOWakeWordEngine::resume() {

  log(Logger::INFO, "GPIOWakeWordEngine: handling resume");
}

void GPIOWakeWordEngine::init() {

  log(Logger::DEBUG, "GPIOWakeWordEngine: initializing");
  log(Logger::DEBUG, "Starting GPIO reading thread");
  m_isRunning = true;
  m_thread = make_unique<std::thread>(&GPIOWakeWordEngine::mainLoop, this);
}

void GPIOWakeWordEngine::mainLoop() {

  bool gpioReady { true };

  while (m_isRunning) {
	std::cin.get();
	wakeWordDetected();
    sleep(SECONDS_BETWEEN_READINGS);
  }
}

} // namespace AlexaWakeWord
