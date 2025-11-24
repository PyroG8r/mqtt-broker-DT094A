#include <iostream>
#include <csignal>
#include "broker/MqttBroker.h"

mqtt::MqttBroker* brokerInstance = nullptr;

void signalHandler(int signum) {
    std::cout << "\nInterrupt signal (" << signum << ") received." << std::endl;
    if (brokerInstance) {
        brokerInstance->stop();
    }
    exit(signum);
}

int main() {
    mqtt::MqttBroker broker;
    brokerInstance = &broker;
    
    // Register signal handler for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Start the MQTT broker
    broker.start();

    std::cout << "MQTT Broker is running... Press Ctrl+C to stop." << std::endl;

    // Event loop - handle incoming connections and messages
    broker.run();

    // Stop the MQTT broker
    broker.stop();
    return 0;
}