#include <iostream>
#include <librdkafka/rdkafkacpp.h>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <boost/endian/buffers.hpp>
#include <future>
#include <set>

using boost::asio::ip::udp;

using RdKafka::ErrorCode;
using RdKafka::Conf;
typedef std::unique_ptr<Conf> ConfPtr;
using RdKafka::Producer;
typedef std::unique_ptr<Producer> ProducerPtr;
using RdKafka::Consumer;
typedef std::unique_ptr<Consumer> ConsumerPtr;
using RdKafka::Topic;
typedef std::unique_ptr<Topic> TopicPtr;
using RdKafka::Message;
typedef std::unique_ptr<Message> MessagePtr;

struct PacketKey
{
    boost::asio::ip::address_v4::bytes_type source;
    boost::endian::big_int32_buf_t padding;
    boost::endian::big_int64_buf_t utc;

    PacketKey(boost::asio::ip::address source, std::chrono::high_resolution_clock::time_point utc)
            : source(source.to_v4().to_bytes())
            , padding(0)
            , utc(utc.time_since_epoch().count()) { }
};

struct Job
{
    typedef std::shared_ptr<Job> Pointer;

    bool cancel;
    std::future<size_t> result;
};

typename Job::Pointer start_job(std::function<size_t(std::function<bool()>)> execute)
{
    auto job = std::make_shared<Job>();
    job->cancel = false;
    auto isCanceled = [=]() { return job->cancel; };
    job->result = std::async(std::launch::async, execute, isCanceled);
    return job;
};

struct ServiceConfiguration
{
    ConfPtr global;
    ConfPtr topic;

    static ServiceConfiguration Load()
    {
        std::string error;
        ConfPtr global(Conf::create(RdKafka::Conf::CONF_GLOBAL));
        ConfPtr topic(Conf::create(RdKafka::Conf::CONF_TOPIC));

        global->set("metadata.broker.list", "192.168.99.100:9092", error);

        if (!error.empty()) {
            std::cerr << "Failed to load Kafka configuration: " << error << std::endl;
            exit(1);
        }

        return ServiceConfiguration {
            std::move(global),
            std::move(topic)
        };
    }
};

size_t IngestAllTehUdpPaketz(const ServiceConfiguration *configuration, udp::endpoint endpoint, std::function<bool()> isCanceled) {
    try {
        std::string error;
        boost::asio::io_service io_service;
        auto socket = udp::socket(io_service, endpoint);

        auto producer = ProducerPtr(Producer::create(configuration->global.get(), error));
        if (!producer) {
            std::cerr << "Failed to create producer: " << error << std::endl;
            exit(1);
        }

        auto topicName =
                (boost::format("ingest-udp-%1%-%2%")
                % endpoint.address()
                % endpoint.port())
                .str();
        auto topic = TopicPtr(RdKafka::Topic::create(producer.get(), topicName, nullptr, error));
        if (!error.empty()) {
            std::cerr << "Failed to subscribe to " << topicName.size() << std::endl;
            exit(1);
        }

        bool stop = false;
        auto result = producer->produce(
                topic.get(), 0, Producer::RK_MSG_COPY,
                (void*)"start", sizeof("start") - 1,
                nullptr, 0,
                nullptr);
        switch (result) {
            case ErrorCode::ERR_NO_ERROR:
                break;
                // maximum number of outstanding messages has been reached: queue.buffering.max.message
            case ErrorCode::ERR__QUEUE_FULL:
                std::cout
                        << "maximum number of outstanding messages has been reached: queue.buffering.max.message"
                        << std::endl;
                break;
                // message is larger than configured max size: messages.max.bytes
            case ErrorCode::ERR_MSG_SIZE_TOO_LARGE:
                std::cout << "message is larger than configured max size: messages.max.bytes" << std::endl;
                break;
                // requested partition is unknown in the Kafka cluster.
            case ErrorCode::ERR__UNKNOWN_PARTITION:
                std::cout << "requested partition is unknown in the Kafka cluster" << std::endl;
                break;
                // topic is unknown in the Kafka cluster.
            case ErrorCode::ERR__UNKNOWN_TOPIC:
                std::cout << "topic is unknown in the Kafka cluster" << std::endl;
                break;
            default:
                stop = true;
                break;
        }
        producer->flush(5000);

        std::cout << "Ingest started udp:" << endpoint << " -> kafka:" << topicName << std::endl;
        while (!stop) {
            auto payload = std::array<uint8_t, 0xFFFF>();
            auto sourceEndpoint = udp::endpoint();
            auto length = socket.receive_from(boost::asio::buffer(payload, payload.size()), sourceEndpoint);

            auto key = PacketKey(sourceEndpoint.address(), std::chrono::high_resolution_clock::now());
            auto result = producer->produce(
                    topic.get(), 0, Producer::RK_MSG_COPY,
                    payload.data(), length,
                    &key, sizeof(key),
                    nullptr);
            switch (result) {
                case ErrorCode::ERR_NO_ERROR:
                    break;
                    // maximum number of outstanding messages has been reached: queue.buffering.max.message
                case ErrorCode::ERR__QUEUE_FULL:
                    std::cout
                            << "maximum number of outstanding messages has been reached: queue.buffering.max.message"
                            << std::endl;
                    break;
                    // message is larger than configured max size: messages.max.bytes
                case ErrorCode::ERR_MSG_SIZE_TOO_LARGE:
                    std::cout << "message is larger than configured max size: messages.max.bytes" << std::endl;
                    break;
                    // requested partition is unknown in the Kafka cluster.
                case ErrorCode::ERR__UNKNOWN_PARTITION:
                    std::cout << "requested partition is unknown in the Kafka cluster" << std::endl;
                    break;
                    // topic is unknown in the Kafka cluster.
                case ErrorCode::ERR__UNKNOWN_TOPIC:
                    std::cout << "topic is unknown in the Kafka cluster" << std::endl;
                    break;
                default:
                    stop = true;
                    break;
            }
            producer->flush(5000);
            stop = stop || isCanceled();
        }
        return (size_t) 0;
    }
    catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        return (size_t) 1;
    }
}

size_t PrintAllTehUdpPaketz(const ServiceConfiguration *configuration, udp::endpoint endpoint, std::function<bool()> isCanceled) {
    std::string error;

    auto consumer = ConsumerPtr(RdKafka::Consumer::create(configuration->global.get(), error));
    if (!consumer) {
        std::cerr << "Failed to create consumer: " << error << std::endl;
        exit(1);
    }

    auto topicName =
            (boost::format("ingest-udp-%1%-%2%")
             % endpoint.address()
             % endpoint.port())
                    .str();

    auto topic = TopicPtr(RdKafka::Topic::create(consumer.get(), topicName, nullptr, error));
    if (!error.empty()) {
        std::cerr << "Failed to subscribe to " << topicName << std::endl;
        exit(1);
    }

    auto errorCode = consumer->start(topic.get(), 0, Topic::OFFSET_END);
    if (errorCode != ErrorCode::ERR_NO_ERROR)
    {
        std::cerr << "Consumer start failed " << errorCode << std::endl;
    }

    bool cancel = false;
    size_t count = 0;
    size_t bytes = 0;
    while (!cancel) {
        auto message = MessagePtr(consumer->consume(topic.get(), 0, 1000));
        switch (message->err()) {
            case RdKafka::ERR__TIMED_OUT:
                break;

            case RdKafka::ERR_NO_ERROR:
                /* Real message */
                count++;
                bytes += message->len();
                RdKafka::MessageTimestamp ts;
                ts = message->timestamp();
                if(ts.type != RdKafka::MessageTimestamp::MSG_TIMESTAMP_NOT_AVAILABLE) {
                    std::string tsname = "?";
                    if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_CREATE_TIME)
                        tsname = "create time";
                    else if (ts.type == RdKafka::MessageTimestamp::MSG_TIMESTAMP_LOG_APPEND_TIME)
                        tsname = "log append time";
                    std::cout << "Timestamp: " << tsname << " " << ts.timestamp << std::endl;
                }
                std::cout << "topic: " << topicName << " offset: " << message->offset() << " payload: ";
                printf("%.*s\n",
                       static_cast<int>(message->len()),
                       static_cast<const char *>(message->payload()));
                break;

            case RdKafka::ERR__PARTITION_EOF:
                /* Last message */
                // std::cerr << "%% EOF reached for all " << topicName << std::endl;
                break;

            case RdKafka::ERR__UNKNOWN_TOPIC:
            case RdKafka::ERR__UNKNOWN_PARTITION:
                std::cerr << "Consume failed: " << message->errstr() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                break;

            default:
                /* Errors */
                std::cerr << "Consume failed: " << message->errstr() << std::endl;
                cancel = true;
        }

        cancel = cancel || isCanceled();
    }

    consumer->stop(topic.get(), 0);
    return 0;
}

int main() {
//    signal(SIGINT, sigterm);
//    signal(SIGTERM, sigterm);

    auto serviceConfiguration = ServiceConfiguration::Load();

    using boost::asio::ip::address_v4;
    std::map<boost::asio::ip::address_v4, std::set<uint16_t>> endpoints =
        {
            { { address_v4::from_string("10.0.2.15") }, { 2000, 2001 } },
            { { address_v4::from_string("127.0.0.1") }, { 3000 } }
        };

    std::map<udp::endpoint, Job::Pointer> ingestJobs;
    for (auto& pair : endpoints)
    {
        auto& address = pair.first;
        for (auto port : pair.second)
        {
            auto endpoint = udp::endpoint(address, port);
            auto ingest = std::bind(IngestAllTehUdpPaketz, &serviceConfiguration, endpoint, std::placeholders::_1);
            ingestJobs[endpoint] = start_job(ingest);
        }
    }

    std::map<udp::endpoint, Job::Pointer> printTehPaketzJobs;
    for (auto& pair : endpoints)
    {
        auto& address = pair.first;
        for (auto port : pair.second)
        {
            auto endpoint = udp::endpoint(address, port);
            auto ingest = std::bind(PrintAllTehUdpPaketz, &serviceConfiguration, endpoint, std::placeholders::_1);
            printTehPaketzJobs[endpoint] = start_job(ingest);
        }
    }

    for (auto& job : ingestJobs)
    {
        job.second->result.get();
    }
    return 0;
}