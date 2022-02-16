// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").  You may not use this product except in
// compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license terms. Your use of
// these subcomponents is subject to the terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <cstdint>

#include "PrimitiveTypes.hpp"
#include "assertUtils.hpp"
#include "Digest.hpp"
#include "MessageBase.hpp"
#include "ReplicaConfig.hpp"

namespace bftEngine {
namespace impl {
class RequestsIterator;

class PrePrepareMsg : public MessageBase {
 protected:
  template <typename MessageT>
  friend size_t sizeOfHeader();

#pragma pack(push, 1)
  struct Header {
    MessageBase::Header header;
    ViewNum viewNum;
    SeqNum seqNum;
    EpochNum epochNum;
    uint16_t flags;
    uint64_t batchCidLength;
    uint32_t timeDataLength;
    Digest digestOfRequests;

    uint16_t numberOfRequests;
    uint32_t endLocationOfLastRequest;

    // bits in flags
    // bit 0: 0=null , 1=non-null
    // bit 1: 0=not ready , 1=ready
    // bits 2-3: represent the first commit path that should be tried (00 = OPTIMISTIC_FAST, 01 = FAST_WITH_THRESHOLD,
    // 10 = SLOW) bits 4-15: zero
    // bits 4-5: represents (00 = LegacyConsensusPP, 01 = ConsensusPPDataHashOnly, 10 = DataPPMsg )
  };
#pragma pack(pop)
  static_assert(sizeof(Header) == (6 + 8 + 8 + 8 + 2 + 4 + DIGEST_SIZE + 2 + 4 + 8), "Header is 82B");

  static const size_t prePrepareHeaderPrefix =
      sizeof(Header) - sizeof(Header::numberOfRequests) - sizeof(Header::endLocationOfLastRequest);

 public:
  // static

  static const Digest& digestOfNullPrePrepareMsg();

  void validate(const ReplicasInfo&) const override;

  bool shouldValidateAsync() const override { return true; }

  // size - total size of all requests that will be added
  PrePrepareMsg(ReplicaId sender, ViewNum v, SeqNum s, CommitPath firstPath, size_t size);

  PrePrepareMsg(ReplicaId sender,
                ViewNum v,
                SeqNum s,
                CommitPath firstPath,
                const std::string& timeData,
                const concordUtils::SpanContext& spanContext,
                size_t size);

  PrePrepareMsg(ReplicaId sender,
                ViewNum v,
                SeqNum s,
                CommitPath firstPath,
                const concordUtils::SpanContext& spanContext,
                const std::string& batchCid,
                const std::string& timeData,
                size_t size);

  PrePrepareMsg* createConsensusPPMsg(
      PrePrepareMsg* pp, uint64_t seq, uint64_t view, uint16_t sender_id, size_t size, const std::string& ts);
  PrePrepareMsg* cloneDataPPMsg(PrePrepareMsg* pp);

  BFTENGINE_GEN_CONSTRUCT_FROM_BASE_MESSAGE(PrePrepareMsg)

  uint32_t remainingSizeForRequests() const;

  uint32_t requestsSize() const;

  void addRequest(const char* pRequest, uint32_t requestSize);

  void finishAddingRequests();

  // getter methods

  ViewNum viewNumber() const { return b()->viewNum; }
  void setViewNumber(ViewNum v) { b()->viewNum = v; }

  SeqNum seqNumber() const { return b()->seqNum; }
  void setSeqNumber(SeqNum s) { b()->seqNum = s; }

  std::string getTimeData() const;
  std::string getCid() const;
  void setCid(SeqNum s);

  /// This is actually the final commit path of the request
  CommitPath firstPath() const;

  bool isNull() const { return ((b()->flags & 0x1) == 0); }

  Digest& digestOfRequests() const { return b()->digestOfRequests; }
  void setDigestOfRequests(const Digest& d) { b()->digestOfRequests = d; }

  uint16_t numberOfRequests() const { return b()->numberOfRequests; }
  void setNumberOfRequests(uint16_t n) { b()->numberOfRequests = n; }

  // update view and first path

  void updateView(ViewNum v, CommitPath firstPath = CommitPath::SLOW);
  const std::string getClientCorrelationIdForMsg(int index) const;
  const std::string getBatchCorrelationIdAsString() const;

  void setDataPPFlag() { b()->flags |= 32; }
  bool isDataPPFlagSet() const {
    const uint16_t flags = b()->flags;
    return (((flags >> 4) & 0x2) == 0x2);
  }
  void setConsensusOnlyFlag() { b()->flags |= 16; }
  void resetConsensusOnlyFlag() {
    b()->flags &= ~(1 << 4);
    b()->flags &= ~(1 << 5);
  }
  bool isConsensusPPFlagSet() const {
    const uint16_t flags = b()->flags;
    return (((flags >> 4) & 0x1) == 0x1);
  }
  bool isLegacyPPMsg() const {
    const uint16_t flags = b()->flags;
    return ((flags >> 4) == 0);
  }

 protected:
  static int16_t computeFlagsForPrePrepareMsg(bool isNull, bool isReady, CommitPath firstPath);

  void calculateDigestOfRequests(Digest& d) const;

  bool isReady() const { return (((b()->flags >> 1) & 0x1) == 1); }

  bool checkRequests() const;

  Header* b() const { return (Header*)msgBody_; }

  uint32_t payloadShift() const;
  friend class RequestsIterator;
};

class RequestsIterator {
 public:
  RequestsIterator(const PrePrepareMsg* const m);

  void restart();

  bool getCurrent(char*& pRequest) const;

  bool end() const;

  void gotoNext();

  bool getAndGoToNext(char*& pRequest);

 protected:
  const PrePrepareMsg* const msg;
  uint32_t currLoc;
};

template <>
inline MsgSize maxMessageSize<PrePrepareMsg>() {
  return ReplicaConfig::instance().getmaxExternalMessageSize() + MessageBase::SPAN_CONTEXT_MAX_SIZE;
}

}  // namespace impl
}  // namespace bftEngine
