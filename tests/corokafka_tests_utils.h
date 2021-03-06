#ifndef BLOOMBERG_COROKAFKA_TESTS_UTILS_H
#define BLOOMBERG_COROKAFKA_TESTS_UTILS_H

#include <corokafka_tests_topics.h>
#include <corokafka_tests_callbacks.h>
#include <gtest/gtest.h>
#include <utility>
#include <vector>
#include <map>
#include <mutex>
#if (__cplusplus < 201703L)
    #include <experimental/optional>
    template <typename T>
    using Optional = std::experimental::fundamentals_v1::optional<T>;
    using NullOptType = std::experimental::fundamentals_v1::nullopt_t;
#else
    #include <optional>
    template <typename T>
    using Optional = std::optional<T>;
    using NullOptType = std::nullopt_t;
#endif
constexpr NullOptType NullOpt { NullOptType::_Construct::_Token };

namespace Bloomberg {
namespace corokafka {
namespace tests {

enum class SenderId : uint16_t {
    SyncWithoutHeaders = 0,
    Sync,
    SyncFirstHeaderMissing,
    SyncSecondHeaderMissing,
    SyncBothHeadersMissing,
    SyncUnordered,
    SyncIdempotent,
    Async,
    AsyncUnordered,
    Callbacks
};

enum class OffsetPoint : int {
    AtBeginning = -2,
    AtEnd = -1,
    FromStoredOffset = -1000,
    Invalid = -1001,
    AtEndRelative = -2000
};

int partition(const cppkafka::Buffer& key,
              int32_t partitionCount);

inline
int partition(const SenderId& key)
{
    ByteArray serialKey = Serialize<Key>()((Key)key);
    return partition({serialKey.data(), serialKey.size()}, 4);
}

inline const char* getSenderStr(SenderId id)
{
    std::map<SenderId, const char*> messages = {
        { SenderId::SyncWithoutHeaders, "SyncWithoutHeaders"},
        { SenderId::Sync, "Sync"},
        { SenderId::SyncFirstHeaderMissing, "SyncFirstHeaderMissing" },
        { SenderId::SyncSecondHeaderMissing, "SyncSecondHeaderMissing" },
        { SenderId::SyncBothHeadersMissing, "SyncBothHeadersMissing" },
        { SenderId::SyncUnordered, "SyncUnordered" },
        { SenderId::SyncIdempotent, "SyncIdempotent" },
        { SenderId::Async, "Async" },
        { SenderId::AsyncUnordered, "AsyncUnordered" },
        { SenderId::Callbacks, "Callbacks" }
    };
    return messages[id];
}

struct MessageTracker
{
    struct Info
    {
        Info(SenderId id, const Header1& h1, const Header2& h2, const Message& m) :
            _id(id),
            _partition(partition(id)),
            _header1(h1),
            _header2(h2),
            _message(m)
        {}
        Info(SenderId id, const Header1& h1, const Message& m) :
            _id(id),
            _partition(partition(id)),
            _header1(h1),
            _message(m)
        {}
        Info(SenderId id, const Header2& h2, const Message& m) :
            _id(id),
            _partition(partition(id)),
            _header2(h2),
            _message(m)
        {}
        Info(SenderId id, const Message& m) :
            _id(id),
            _partition(partition(id)),
            _message(m)
        {}
        bool operator==(const Info& other) const {
            //everything but offset
            return std::tie(_id, _partition, _header1, _header2, _message) ==
                   std::tie(other._id, other._partition, other._header1, other._header2, other._message);
        }
        SenderId                _id;
        int                     _partition{0};
        int                     _offset{EnumValue(OffsetPoint::Invalid)};
        Optional<Header1>       _header1;
        Optional<Header2>       _header2;
        Message                 _message;
    };
    
    MessageTracker(const std::string& topic) :
        _topic(topic)
    {}
    
    size_t totalMessages() const {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t total{0};
        for (auto&& entry : _messages) {
           for (auto&& v : entry.second) {
               total += v.size();
           }
        }
        return total;
    }
    void add(Info info) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_messages[info._id].empty() || (info._message._num == 0)) {
            //start a new serie
            _messages[info._id].emplace_back();
        }
        _offsets[{_topic, info._partition}] = info._offset;
        _messages[info._id].back().push_back(std::move(info));
    }
    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _messages.clear();
    }
    bool operator==(const MessageTracker& other) const {
        std::lock_guard<std::mutex> lock(_mutex);
        return std::tie(_messages, _topic) == std::tie(other._messages, other._topic);
    }
    using MessageList = std::vector<Info>;
    using MessageLists = std::vector<MessageList>;
    
    const std::string                _topic;
    mutable std::mutex               _mutex;
    std::map<SenderId, MessageLists> _messages;
    std::map<cppkafka::TopicPartition, int> _offsets;
};

inline
MessageTracker& messageTracker()
{
    static MessageTracker tracker(topicWithHeaders().topic());
    return tracker;
}

inline
MessageTracker& messageWithoutHeadersTracker()
{
    static MessageTracker tracker(topicWithoutHeaders().topic());
    return tracker;
}

inline
MessageTracker& consumerMessageTracker()
{
    static MessageTracker tracker(topicWithHeaders().topic());
    return tracker;
}

inline
MessageTracker& consumerMessageWithoutHeadersTracker()
{
    static MessageTracker tracker(topicWithoutHeaders().topic());
    return tracker;
}

using ValueTestList = std::vector<std::pair<std::string,bool>>;

template <typename EX>
void testConnectorOption(const char* exName, const char* opName, const ValueTestList& values)
{
    for (auto&& value : values) {
        if (value.second) { //throws
            ASSERT_THROW(ConnectorConfiguration({{opName, value.first}}), EX);
            try { ConnectorConfiguration connector({{opName, value.first}}); }
            catch (const EX& ex) {
                ASSERT_STREQ(exName, ex.name());
                std::string op = opName;
                trim(op);
                ASSERT_TRUE(StringEqualCompare()(op, ex.option()));
            }
        }
        else {
            ASSERT_NO_THROW(ConnectorConfiguration({{opName, value.first}}));
        }
    }
}

template <typename EX, typename CONFIG>
void testConnectorOption(CONFIG&& config, const char* exName, const char* opName, bool throws)
{
    ConfigurationBuilder builder; builder(config);
    if (throws) { //throws
        ASSERT_THROW(Connector connector(builder), EX);
        try { Connector connector(builder); }
        catch (const EX& ex) {
            ASSERT_STREQ(exName, ex.name());
            std::string op = opName;
            trim(op);
            ASSERT_TRUE(StringEqualCompare()(op, ex.option()));
        }
    }
    else {
        ASSERT_NO_THROW(Connector connector(builder));
    }
}

template <typename EX>
void testProducerOption(const char* exName, const char* opName, const ValueTestList& values)
{
    for (auto&& value : values) {
        if (value.second) { //throws
            try {
                ProducerConfiguration config(
                    topicWithHeaders(),
                    {{opName, value.first},
                    {"metadata.broker.list", programOptions()._broker}}, {});
                FAIL(); //should have thrown
            }
            catch (const EX& ex) {
                ASSERT_STREQ(exName, ex.name());
                std::string op = opName;
                trim(op);
                ASSERT_TRUE(StringEqualCompare()(op, ex.option()));
            }
            catch(...) {
                FAIL();
            }
        }
        else {
            ASSERT_NO_THROW(
                ProducerConfiguration config(
                    topicWithHeaders(),
                    {{opName, value.first},
                    {"metadata.broker.list", programOptions()._broker}}, {});
            );
        }
    }
}

template <typename EX>
void testConsumerOption(const char* exName, const char* opName, const ValueTestList& values)
{
    for (auto&& value : values) {
        if (value.second) { //throws
            try {
                ConsumerConfiguration config(topicWithHeaders(),
                    {{opName, value.first},
                     {"metadata.broker.list", programOptions()._broker},
                     {"group.id","test-group"},
                     {ConsumerConfiguration::Options::pauseOnStart, true},
                     {ConsumerConfiguration::Options::readSize, 1}}, {}, Callbacks::messageReceiverWithHeaders);
                FAIL(); //should have thrown
            }
            catch (const EX& ex) {
                ASSERT_STREQ(exName, ex.name());
                std::string op = opName;
                trim(op);
                ASSERT_TRUE(StringEqualCompare()(op, ex.option()));
            }
            catch(...) {
                FAIL();
            }
        }
        else {
            ASSERT_NO_THROW(
                ConsumerConfiguration config(topicWithHeaders(),
                    {{opName, value.first},
                     {"metadata.broker.list", programOptions()._broker},
                     {"group.id","test-group"},
                     {ConsumerConfiguration::Options::pauseOnStart, true},
                     {ConsumerConfiguration::Options::readSize, 1}}, {}, Callbacks::messageReceiverWithHeaders);
            );
        }
    }
}

template <typename TOPIC>
Connector makeProducerConnector(const Configuration::OptionList& ops, const TOPIC& topic)
{
    Configuration::OptionList options = ops;
    options.push_back({"metadata.broker.list", programOptions()._broker});
    ProducerConfiguration config(topic, options, {});
    config.setPartitionerCallback(Callbacks::partitioner);
    ConfigurationBuilder builder;
    ConnectorConfiguration connConfig({
        {ConnectorConfiguration::Options::pollIntervalMs, 10}
    });
    builder(config)(connConfig);
    return {builder, dispatcher()};
}

template <typename TOPIC, typename RECV>
Connector makeConsumerConnector(const Configuration::OptionList& ops,
                                const std::string& groupId,
                                TOPIC&& topic,
                                RECV&& receiver,
                                PartitionStrategy strategy = PartitionStrategy::Static,
                                cppkafka::TopicPartitionList partitions = {})
{
    Configuration::OptionList options = ops;
    options.push_back({"metadata.broker.list", programOptions()._broker});
    options.push_back({"group.id", groupId});
    ConsumerConfiguration config(std::forward<TOPIC>(topic), options, {}, std::forward<RECV>(receiver));
    config.setPreprocessorCallback(Callbacks::messagePreprocessor);
    config.setOffsetCommitCallback(Callbacks::handleOffsetCommit);
    config.setRebalanceCallback(Callbacks::handleRebalance);
    config.assignInitialPartitions(strategy, std::move(partitions));
    ConnectorConfiguration connConfig({
        {ConnectorConfiguration::Options::pollIntervalMs, 10}
    });
    ConfigurationBuilder builder;
    builder(config)(connConfig);
    return {builder, dispatcher()};
}


}}}

#endif //BLOOMBERG_COROKAFKA_TESTS_UTILS_H
