/*
** Copyright 2019 Bloomberg Finance L.P.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/
#include <corokafka/corokafka_consumer_metadata.h>

namespace Bloomberg {
namespace corokafka {

//=============================================================================
//                             CONSUMER METADATA
//=============================================================================

ConsumerMetadata::ConsumerMetadata(const std::string& topic,
                                   Consumer& handle) :
    Metadata(topic, Topic(), handle)
{
}

ConsumerMetadata::ConsumerMetadata(const std::string& topic,
                                   const Topic& kafkaTopic,
                                   Consumer& handle) :
    Metadata(topic, kafkaTopic, handle)
{
}

Metadata::OffsetWatermarkList ConsumerMetadata::queryOffsetWatermarks() const
{
    OffsetWatermarkList offsets;
    for (const auto& partition : getPartitionAssignment()) {
        offsets.emplace_back(partition.get_partition(),
                             _handle.query_offsets(partition));
    }
    return offsets;
}

Metadata::OffsetWatermarkList ConsumerMetadata::getOffsetWatermarks() const
{
    OffsetWatermarkList offsets;
    for (const auto& partition : getPartitionAssignment()) {
        offsets.emplace_back(partition.get_partition(),
                             static_cast<const Consumer&>(_handle).get_offsets(partition));
    }
    return offsets;
}

TopicPartitionList ConsumerMetadata::queryOffsetsAtTime(Metadata::Timestamp timestamp) const
{
    KafkaHandleBase::TopicPartitionsTimestampsMap timestampMap;
    std::chrono::milliseconds epochTime = timestamp.time_since_epoch();
    for (const auto& partition : getPartitionAssignment()) {
        timestampMap[partition] = epochTime;
    }
    return _handle.get_offsets_for_times(timestampMap);
}

TopicPartitionList ConsumerMetadata::queryCommittedOffsets() const
{
    return static_cast<const Consumer&>(_handle).get_offsets_committed(getPartitionAssignment());
}

TopicPartitionList ConsumerMetadata::getOffsetPositions() const
{
    return static_cast<const Consumer&>(_handle).get_offsets_position(getPartitionAssignment());
}

const TopicPartitionList& ConsumerMetadata::getPartitionAssignment() const
{
    if (_partitions.empty()) {
        _partitions = static_cast<const Consumer&>(_handle).get_assignment();
    }
    return _partitions;
}

GroupInformation ConsumerMetadata::getGroupInformation() const
{
    return _handle.get_consumer_group(static_cast<const Consumer&>(_handle).get_member_id());
}

}
}