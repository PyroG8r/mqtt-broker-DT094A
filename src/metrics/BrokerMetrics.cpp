#include "../../include/metrics/BrokerMetrics.h"
#include <iostream>

namespace mqtt {

BrokerMetrics::BrokerMetrics() 
    : registry_(std::make_shared<prometheus::Registry>()) {
    
    // Initialize gauge families and gauges
    active_connections_family_ = &prometheus::BuildGauge()
        .Name("mqtt_active_connections")
        .Help("Number of currently active MQTT connections")
        .Register(*registry_);
    active_connections_ = &active_connections_family_->Add({});
    
    active_subscriptions_family_ = &prometheus::BuildGauge()
        .Name("mqtt_active_subscriptions")
        .Help("Number of currently active topic subscriptions")
        .Register(*registry_);
    active_subscriptions_ = &active_subscriptions_family_->Add({});
    
    // Initialize counter families and counters
    total_connections_family_ = &prometheus::BuildCounter()
        .Name("mqtt_total_connections")
        .Help("Total number of connections accepted")
        .Register(*registry_);
    total_connections_ = &total_connections_family_->Add({});
    
    messages_published_family_ = &prometheus::BuildCounter()
        .Name("mqtt_messages_published_total")
        .Help("Total number of messages published")
        .Register(*registry_);
    messages_published_ = &messages_published_family_->Add({});
    
    messages_received_family_ = &prometheus::BuildCounter()
        .Name("mqtt_messages_received_total")
        .Help("Total number of messages received")
        .Register(*registry_);
    messages_received_ = &messages_received_family_->Add({});
    
    bytes_received_family_ = &prometheus::BuildCounter()
        .Name("mqtt_bytes_received_total")
        .Help("Total number of bytes received")
        .Register(*registry_);
    bytes_received_ = &bytes_received_family_->Add({});
    
    bytes_sent_family_ = &prometheus::BuildCounter()
        .Name("mqtt_bytes_sent_total")
        .Help("Total number of bytes sent")
        .Register(*registry_);
    bytes_sent_ = &bytes_sent_family_->Add({});
    
    connection_errors_family_ = &prometheus::BuildCounter()
        .Name("mqtt_connection_errors_total")
        .Help("Total number of connection errors")
        .Register(*registry_);
    connection_errors_ = &connection_errors_family_->Add({});
    
    // Initialize histogram family and histogram
    message_size_family_ = &prometheus::BuildHistogram()
        .Name("mqtt_message_size_bytes")
        .Help("Distribution of message sizes in bytes")
        .Register(*registry_);
    message_size_ = &message_size_family_->Add({}, 
        prometheus::Histogram::BucketBoundaries{10, 50, 100, 500, 1000, 5000, 10000, 50000});
}

void BrokerMetrics::startExporter(const std::string& bind_address) {
    try {
        exposer_ = std::make_unique<prometheus::Exposer>(bind_address);
        exposer_->RegisterCollectable(registry_);
        std::cout << "Prometheus metrics exporter started on " << bind_address << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start Prometheus exporter: " << e.what() << std::endl;
    }
}

void BrokerMetrics::setActiveConnections(double value) {
    active_connections_->Set(value);
}

void BrokerMetrics::setActiveSubscriptions(double value) {
    active_subscriptions_->Set(value);
}

void BrokerMetrics::incrementTotalConnections() {
    total_connections_->Increment();
}

void BrokerMetrics::incrementMessagesPublished() {
    messages_published_->Increment();
}

void BrokerMetrics::incrementMessagesReceived() {
    messages_received_->Increment();
}

void BrokerMetrics::incrementBytesReceived(double bytes) {
    bytes_received_->Increment(bytes);
}

void BrokerMetrics::incrementBytesSent(double bytes) {
    bytes_sent_->Increment(bytes);
}

void BrokerMetrics::incrementConnectionErrors() {
    connection_errors_->Increment();
}

void BrokerMetrics::observeMessageSize(double size) {
    message_size_->Observe(size);
}

} // namespace mqtt
