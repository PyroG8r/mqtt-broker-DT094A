// include/metrics/BrokerMetrics.h
#ifndef BROKER_METRICS_H
#define BROKER_METRICS_H

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/exposer.h>
#include <memory>

namespace mqtt {

class BrokerMetrics {
public:
    BrokerMetrics();
    void startExporter(const std::string& bind_address = "0.0.0.0:9090");
    
    // Gauges
    void setActiveConnections(double value);
    void setActiveSubscriptions(double value);
    
    // Counters
    void incrementTotalConnections();
    void incrementMessagesPublished();
    void incrementMessagesReceived();
    void incrementBytesReceived(double bytes);
    void incrementBytesSent(double bytes);
    void incrementConnectionErrors();
    
    // Histograms
    void observeMessageSize(double size);
    
private:
    std::shared_ptr<prometheus::Registry> registry_;
    std::unique_ptr<prometheus::Exposer> exposer_;
    
    // Metrics
    prometheus::Family<prometheus::Gauge>* active_connections_family_;
    prometheus::Gauge* active_connections_;
    
    prometheus::Family<prometheus::Gauge>* active_subscriptions_family_;
    prometheus::Gauge* active_subscriptions_;
    
    prometheus::Family<prometheus::Counter>* total_connections_family_;
    prometheus::Counter* total_connections_;
    
    prometheus::Family<prometheus::Counter>* messages_published_family_;
    prometheus::Counter* messages_published_;
    
    prometheus::Family<prometheus::Counter>* messages_received_family_;
    prometheus::Counter* messages_received_;
    
    prometheus::Family<prometheus::Counter>* bytes_received_family_;
    prometheus::Counter* bytes_received_;
    
    prometheus::Family<prometheus::Counter>* bytes_sent_family_;
    prometheus::Counter* bytes_sent_;
    
    prometheus::Family<prometheus::Counter>* connection_errors_family_;
    prometheus::Counter* connection_errors_;
    
    prometheus::Family<prometheus::Histogram>* message_size_family_;
    prometheus::Histogram* message_size_;
};

} // namespace mqtt

#endif