#include "cache.h"
#include <cassert>
#include <iostream>
#include <vector>

namespace falcon {
// FALCON functional knobs
constexpr bool LOOKAHEAD_ON = true;
constexpr bool FILTER_ON = true;
constexpr bool GHR_ON = true;
constexpr bool FALCON_SANITY_CHECK = true;
constexpr bool FALCON_DEBUG_PRINT = false;

// Signature table parameters
constexpr std::size_t ST_SET = 1;
constexpr std::size_t ST_WAY = 256;
constexpr unsigned ST_TAG_BIT = 16;
constexpr uint32_t ST_TAG_MASK = ((1 << ST_TAG_BIT) - 1);
constexpr unsigned SIG_SHIFT = 3;
constexpr unsigned SIG_BIT = 12;
constexpr uint32_t SIG_MASK = ((1 << SIG_BIT) - 1);
constexpr unsigned SIG_DELTA_BIT = 7;

// Pattern table parameters
constexpr std::size_t PT_SET = 512;
constexpr std::size_t PT_WAY = 4;
constexpr unsigned C_SIG_BIT = 4;
constexpr unsigned C_DELTA_BIT = 4;
constexpr uint32_t C_SIG_MAX = ((1 << C_SIG_BIT) - 1);
constexpr uint32_t C_DELTA_MAX = ((1 << C_DELTA_BIT) - 1);

// Prefetch filter parameters
constexpr unsigned QUOTIENT_BIT = 10;
constexpr unsigned REMAINDER_BIT = 6;
constexpr unsigned HASH_BIT = (QUOTIENT_BIT + REMAINDER_BIT + 1);
constexpr std::size_t FILTER_SET = (1 << QUOTIENT_BIT);
constexpr uint32_t FILL_THRESHOLD = 90;
constexpr uint32_t PF_THRESHOLD = 25;

// Global register parameters
constexpr unsigned GLOBAL_COUNTER_BIT = 10;
constexpr uint32_t GLOBAL_COUNTER_MAX = ((1 << GLOBAL_COUNTER_BIT) - 1);
constexpr std::size_t MAX_GHR_ENTRY = 8;

enum FILTER_REQUEST {
  FALCON_L2C_PREFETCH,
  FALCON_l2c_PREFETCH,
  L2C_DEMAND,
  L2C_EVICT
}; // Request type for prefetch filter
uint64_t get_hash(uint64_t key);

class SIGNATURE_TABLE {
public:
  bool valid[ST_SET][ST_WAY];
  uint32_t tag[ST_SET][ST_WAY], last_offset[ST_SET][ST_WAY],
      sig[ST_SET][ST_WAY], lru[ST_SET][ST_WAY];

  SIGNATURE_TABLE() {
    for (uint32_t set = 0; set < ST_SET; set++)
      for (uint32_t way = 0; way < ST_WAY; way++) {
        valid[set][way] = 0;
        tag[set][way] = 0;
        last_offset[set][way] = 0;
        sig[set][way] = 0;
        lru[set][way] = way;
      }
  };

  void read_and_update_sig(uint64_t page, uint32_t page_offset,
                           uint32_t &last_sig, uint32_t &curr_sig,
                           int32_t &delta);
};

class PATTERN_TABLE {
public:
  int delta[PT_SET][PT_WAY];
  uint32_t c_delta[PT_SET][PT_WAY], c_sig[PT_SET];

  PATTERN_TABLE() {
    for (uint32_t set = 0; set < PT_SET; set++) {
      for (uint32_t way = 0; way < PT_WAY; way++) {
        delta[set][way] = 0;
        c_delta[set][way] = 0;
      }
      c_sig[set] = 0;
    }
  }

  void update_pattern(uint32_t last_sig, int curr_delta),
      read_pattern(uint32_t curr_sig, std::vector<int> &prefetch_delta,
                   std::vector<uint32_t> &confidence_q, uint32_t &lookahead_way,
                   uint32_t &lookahead_conf, uint32_t &pf_q_tail,
                   uint32_t &depth);
};

class PREFETCH_FILTER {
public:
  uint64_t remainder_tag[FILTER_SET];
  bool valid[FILTER_SET], // Consider this as "prefetched"
      useful[FILTER_SET]; // Consider this as "used"

  PREFETCH_FILTER() {
    for (uint32_t set = 0; set < FILTER_SET; set++) {
      remainder_tag[set] = 0;
      valid[set] = 0;
      useful[set] = 0;
    }
  }

  bool check(uint64_t pf_addr, FILTER_REQUEST filter_request);
};

class GLOBAL_REGISTER {
public:
  // Global counters to calculate global prefetching accuracy
  uint32_t pf_useful, pf_issued;
  uint32_t global_accuracy; // Alpha value in Section III. Equation 3

  // Global History Register (GHR) entries
  uint8_t valid[MAX_GHR_ENTRY];
  uint32_t sig[MAX_GHR_ENTRY], confidence[MAX_GHR_ENTRY], offset[MAX_GHR_ENTRY];
  int delta[MAX_GHR_ENTRY];

  GLOBAL_REGISTER() {
    pf_useful = 0;
    pf_issued = 0;
    global_accuracy = 0;

    for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
      valid[i] = 0;
      sig[i] = 0;
      confidence[i] = 0;
      offset[i] = 0;
      delta[i] = 0;
    }
  }

  void update_entry(uint32_t pf_sig, uint32_t pf_confidence, uint32_t pf_offset,
                    int pf_delta);
  uint32_t check_entry(uint32_t page_offset);
};
} // namespace falcon

namespace {
falcon::SIGNATURE_TABLE ST;
falcon::PATTERN_TABLE PT;
falcon::PREFETCH_FILTER FILTER;
falcon::GLOBAL_REGISTER GHR;
} // namespace

void CACHE::l2c_prefetcher_initialize() {
  std::cout << "Initialize SIGNATURE TABLE" << std::endl;
  std::cout << "ST_SET: " << falcon::ST_SET << std::endl;
  std::cout << "ST_WAY: " << falcon::ST_WAY << std::endl;
  std::cout << "ST_TAG_BIT: " << falcon::ST_TAG_BIT << std::endl;
  std::cout << "ST_TAG_MASK: " << std::hex << falcon::ST_TAG_MASK << std::dec
            << std::endl;

  std::cout << std::endl << "Initialize PATTERN TABLE" << std::endl;
  std::cout << "PT_SET: " << falcon::PT_SET << std::endl;
  std::cout << "PT_WAY: " << falcon::PT_WAY << std::endl;
  std::cout << "SIG_DELTA_BIT: " << falcon::SIG_DELTA_BIT << std::endl;
  std::cout << "C_SIG_BIT: " << falcon::C_SIG_BIT << std::endl;
  std::cout << "C_DELTA_BIT: " << falcon::C_DELTA_BIT << std::endl;

  std::cout << std::endl << "Initialize PREFETCH FILTER" << std::endl;
  std::cout << "FILTER_SET: " << falcon::FILTER_SET << std::endl;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t addr, uint64_t ip,
                                       uint8_t cache_hit, uint8_t type,
                                       uint32_t metadata_in) {
  uint64_t page = addr >> LOG2_PAGE_SIZE;
  uint32_t page_offset =
               (addr >> LOG2_BLOCK_SIZE) & (PAGE_SIZE / BLOCK_SIZE - 1),
           last_sig = 0, curr_sig = 0, depth = 0;
  std::vector<uint32_t> confidence_q(MSHR_SIZE);

  int32_t delta = 0;
  std::vector<int32_t> delta_q(MSHR_SIZE);

  for (uint32_t i = 0; i < MSHR_SIZE; i++) {
    confidence_q[i] = 0;
    delta_q[i] = 0;
  }
  confidence_q[0] = 100;
  ::GHR.global_accuracy =
      ::GHR.pf_issued ? ((100 * ::GHR.pf_useful) / ::GHR.pf_issued) : 0;

  if (falcon::FALCON_DEBUG_PRINT) {
    std::cout << std::endl
              << "[ChampSim] " << __func__ << " addr: " << std::hex << addr
              << " cache_line: " << (addr >> LOG2_BLOCK_SIZE);
    std::cout << " page: " << page << " page_offset: " << std::dec
              << page_offset << std::endl;
  }

  // Stage 1: Read and update a sig stored in ST
  // last_sig and delta are used to update (sig, delta) correlation in PT
  // curr_sig is used to read prefetch candidates in PT
  ::ST.read_and_update_sig(page, page_offset, last_sig, curr_sig, delta);

  // Also check the prefetch filter in parallel to update global accuracy
  // counters
  ::FILTER.check(addr, falcon::L2C_DEMAND);

  // Stage 2: Update delta patterns stored in PT
  if (last_sig)
    ::PT.update_pattern(last_sig, delta);

  // Stage 3: Start prefetching
  uint64_t base_addr = addr;
  uint32_t lookahead_conf = 100, pf_q_head = 0, pf_q_tail = 0;
  uint8_t do_lookahead = 0;

  do {
    uint32_t lookahead_way = falcon::PT_WAY;
    ::PT.read_pattern(curr_sig, delta_q, confidence_q, lookahead_way,
                      lookahead_conf, pf_q_tail, depth);

    do_lookahead = 0;
    for (uint32_t i = pf_q_head; i < pf_q_tail; i++) {
      if (confidence_q[i] >= falcon::PF_THRESHOLD) {
        uint64_t pf_addr =
            (base_addr & ~(BLOCK_SIZE - 1)) + (delta_q[i] << LOG2_BLOCK_SIZE);

        if ((addr & ~(PAGE_SIZE - 1)) ==
            (pf_addr & ~(PAGE_SIZE -
                         1))) { // Prefetch request is in the same physical page
          if (::FILTER.check(pf_addr,
                             ((confidence_q[i] >= falcon::FILL_THRESHOLD)
                                  ? falcon::FALCON_L2C_PREFETCH
                                  : falcon::FALCON_l2c_PREFETCH))) {
            prefetch_line(ip, base_addr, pf_addr,
                          (confidence_q[i] >= falcon::FILL_THRESHOLD),
                          0); // Use addr (not base_addr) to obey the same
                              // physical page boundary

            if (confidence_q[i] >= falcon::FILL_THRESHOLD) {
              ::GHR.pf_issued++;
              if (::GHR.pf_issued > falcon::GLOBAL_COUNTER_MAX) {
                ::GHR.pf_issued >>= 1;
                ::GHR.pf_useful >>= 1;
              }
              if (falcon::FALCON_DEBUG_PRINT) {
                std::cout
                    << "[ChampSim] falcon L2 prefetch issued GHR.pf_issued: "
                    << ::GHR.pf_issued << " GHR.pf_useful: " << ::GHR.pf_useful
                    << std::endl;
              }
            }

            if (falcon::FALCON_DEBUG_PRINT) {
              std::cout << "[ChampSim] " << __func__
                        << " base_addr: " << std::hex << base_addr
                        << " pf_addr: " << pf_addr;
              std::cout << " pf_cache_line: " << (pf_addr >> LOG2_BLOCK_SIZE);
              std::cout << " prefetch_delta: " << std::dec << delta_q[i]
                        << " confidence: " << confidence_q[i];
              std::cout << " depth: " << i << std::endl;
            }
          }
        } else { // Prefetch request is crossing the physical page boundary
          if (falcon::GHR_ON) {
            // Store this prefetch request in GHR to bootstrap falcon learning
            // when we see a ST miss (i.e., accessing a new page)
            ::GHR.update_entry(curr_sig, confidence_q[i],
                               (pf_addr >> LOG2_BLOCK_SIZE) & 0x3F, delta_q[i]);
          }
        }

        do_lookahead = 1;
        pf_q_head++;
      }
    }

    // Update base_addr and curr_sig
    if (lookahead_way < falcon::PT_WAY) {
      uint32_t set = falcon::get_hash(curr_sig) % falcon::PT_SET;
      base_addr += (::PT.delta[set][lookahead_way] << LOG2_BLOCK_SIZE);

      // PT.delta uses a 7-bit sign magnitude representation to generate
      // sig_delta
      // int sig_delta = (PT.delta[set][lookahead_way] < 0) ? ((((-1) *
      // PT.delta[set][lookahead_way]) & 0x3F) + 0x40) :
      // PT.delta[set][lookahead_way];
      int sig_delta = (::PT.delta[set][lookahead_way] < 0)
                          ? (((-1) * ::PT.delta[set][lookahead_way]) +
                             (1 << (falcon::SIG_DELTA_BIT - 1)))
                          : ::PT.delta[set][lookahead_way];
      curr_sig =
          ((curr_sig << falcon::SIG_SHIFT) ^ sig_delta) & falcon::SIG_MASK;
    }

    if (falcon::FALCON_DEBUG_PRINT) {
      std::cout << "Looping curr_sig: " << std::hex << curr_sig
                << " base_addr: " << base_addr << std::dec;
      std::cout << " pf_q_head: " << pf_q_head << " pf_q_tail: " << pf_q_tail
                << " depth: " << depth << std::endl;
    }
  } while (falcon::LOOKAHEAD_ON && do_lookahead);

  return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t addr, uint32_t set,
                                          uint32_t match, uint8_t prefetch,
                                          uint64_t evicted_addr,
                                          uint32_t metadata_in) {
  if (falcon::FILTER_ON) {
    if (falcon::FALCON_DEBUG_PRINT) {
      std::cout << std::endl;
    }
    ::FILTER.check(evicted_addr, falcon::L2C_EVICT);
  }

  return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats() {}

// TODO: Find a good 64-bit hash function
uint64_t falcon::get_hash(uint64_t key) {
  // Robert Jenkins' 32 bit mix function
  key += (key << 12);
  key ^= (key >> 22);
  key += (key << 4);
  key ^= (key >> 9);
  key += (key << 10);
  key ^= (key >> 2);
  key += (key << 7);
  key ^= (key >> 12);

  // Knuth's multiplicative method
  key = (key >> 3) * 2654435761;

  return key;
}

void falcon::SIGNATURE_TABLE::read_and_update_sig(uint64_t page,
                                                  uint32_t page_offset,
                                                  uint32_t &last_sig,
                                                  uint32_t &curr_sig,
                                                  int32_t &delta) {
  uint32_t set = get_hash(page) % ST_SET, match = ST_WAY,
           partial_page = page & ST_TAG_MASK;
  uint8_t ST_hit = 0;
  int sig_delta = 0;

  if (falcon::FALCON_DEBUG_PRINT) {
    std::cout << "[ST] " << __func__ << " page: " << std::hex << page
              << " partial_page: " << partial_page << std::dec << std::endl;
  }

  // Case 2: Invalid
  if (match == ST_WAY) {
    for (match = 0; match < ST_WAY; match++) {
      if (valid[set][match] && (tag[set][match] == partial_page)) {
        last_sig = sig[set][match];
        delta = page_offset - last_offset[set][match];

        if (delta) {
          // Build a new sig based on 7-bit sign magnitude representation of
          // delta sig_delta = (delta < 0) ? ((((-1) * delta) & 0x3F) + 0x40) :
          // delta;
          sig_delta =
              (delta < 0)
                  ? (((-1) * delta) + (1 << (falcon::SIG_DELTA_BIT - 1)))
                  : delta;
          sig[set][match] =
              ((last_sig << falcon::SIG_SHIFT) ^ sig_delta) & falcon::SIG_MASK;
          curr_sig = sig[set][match];
          last_offset[set][match] = page_offset;

          if (falcon::FALCON_DEBUG_PRINT) {
            std::cout << "[ST] " << __func__ << " hit set: " << set
                      << " way: " << match;
            std::cout << " valid: " << valid[set][match] << " tag: " << std::hex
                      << tag[set][match];
            std::cout << " last_sig: " << last_sig << " curr_sig: " << curr_sig;
            std::cout << " delta: " << std::dec << delta
                      << " last_offset: " << page_offset << std::endl;
          }
        } else
          last_sig = 0; // Hitting the same cache line, delta is zero

        ST_hit = 1;
        break;
      }
    }
  }

  // Case 2: Invalid
  if (match == ST_WAY) {
    for (match = 0; match < ST_WAY; match++) {
      if (valid[set][match] == 0) {
        valid[set][match] = 1;
        tag[set][match] = partial_page;
        sig[set][match] = 0;
        curr_sig = sig[set][match];
        last_offset[set][match] = page_offset;

        if (falcon::FALCON_DEBUG_PRINT) {
          std::cout << "[ST] " << __func__ << " invalid set: " << set
                    << " way: " << match;
          std::cout << " valid: " << valid[set][match] << " tag: " << std::hex
                    << partial_page;
          std::cout << " sig: " << sig[set][match]
                    << " last_offset: " << std::dec << page_offset << std::endl;
        }

        break;
      }
    }
  }

  if (FALCON_SANITY_CHECK) {
    // Assertion
    if (match == ST_WAY) {
      for (match = 0; match < ST_WAY; match++) {
        if (lru[set][match] == ST_WAY - 1) { // Find replacement victim
          tag[set][match] = partial_page;
          sig[set][match] = 0;
          curr_sig = sig[set][match];
          last_offset[set][match] = page_offset;

          if (falcon::FALCON_DEBUG_PRINT) {
            std::cout << "[ST] " << __func__ << " miss set: " << set
                      << " way: " << match;
            std::cout << " valid: " << valid[set][match]
                      << " victim tag: " << std::hex << tag[set][match]
                      << " new tag: " << partial_page;
            std::cout << " sig: " << sig[set][match]
                      << " last_offset: " << std::dec << page_offset
                      << std::endl;
          }

          break;
        }
      }

      // Assertion
      if (match == ST_WAY) {
        std::cout << "[ST] Cannot find a replacement victim!" << std::endl;
        assert(0);
      }
    }
  }

  if (falcon::GHR_ON) {
    if (ST_hit == 0) {
      uint32_t GHR_found = ::GHR.check_entry(page_offset);
      if (GHR_found < MAX_GHR_ENTRY) {
        sig_delta = (::GHR.delta[GHR_found] < 0)
                        ? (((-1) * ::GHR.delta[GHR_found]) +
                           (1 << (falcon::SIG_DELTA_BIT - 1)))
                        : ::GHR.delta[GHR_found];
        sig[set][match] =
            ((::GHR.sig[GHR_found] << falcon::SIG_SHIFT) ^ sig_delta) &
            falcon::SIG_MASK;
        curr_sig = sig[set][match];
      }
    }
  }

  // Update LRU
  for (uint32_t way = 0; way < ST_WAY; way++) {
    if (lru[set][way] < lru[set][match]) {
      lru[set][way]++;

      if (FALCON_SANITY_CHECK) {
        // Assertion
        if (lru[set][way] >= ST_WAY) {
          std::cout << "[ST] LRU value is wrong! set: " << set
                    << " way: " << way << " lru: " << lru[set][way]
                    << std::endl;
          assert(0);
        }
      }
    }
  }

  lru[set][match] = 0; // Promote to the MRU position
}

void falcon::PATTERN_TABLE::update_pattern(uint32_t last_sig, int curr_delta) {
  // Update (sig, delta) correlation
  uint32_t set = get_hash(last_sig) % falcon::PT_SET, match = 0;

  // Case 1: Hit
  for (match = 0; match < falcon::PT_WAY; match++) {
    if (delta[set][match] == curr_delta) {
      c_delta[set][match]++;
      c_sig[set]++;
      if (c_sig[set] > C_SIG_MAX) {
        for (uint32_t way = 0; way < falcon::PT_WAY; way++)
          c_delta[set][way] >>= 1;
        c_sig[set] >>= 1;
      }

      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[PT] " << __func__ << " hit sig: " << std::hex << last_sig
                  << std::dec << " set: " << set << " way: " << match;
        std::cout << " delta: " << delta[set][match]
                  << " c_delta: " << c_delta[set][match]
                  << " c_sig: " << c_sig[set] << std::endl;
      }

      break;
    }
  }

  // Case 2: Miss
  if (match == falcon::PT_WAY) {
    uint32_t victim_way = falcon::PT_WAY, min_counter = C_SIG_MAX;

    for (match = 0; match < falcon::PT_WAY; match++) {
      if (c_delta[set][match] <
          min_counter) { // Select an entry with the minimum c_delta
        victim_way = match;
        min_counter = c_delta[set][match];
      }
    }

    delta[set][victim_way] = curr_delta;
    c_delta[set][victim_way] = 0;
    c_sig[set]++;
    if (c_sig[set] > C_SIG_MAX) {
      for (uint32_t way = 0; way < falcon::PT_WAY; way++)
        c_delta[set][way] >>= 1;
      c_sig[set] >>= 1;
    }

    if (falcon::FALCON_DEBUG_PRINT) {
      std::cout << "[PT] " << __func__ << " miss sig: " << std::hex << last_sig
                << std::dec << " set: " << set << " way: " << victim_way;
      std::cout << " delta: " << delta[set][victim_way]
                << " c_delta: " << c_delta[set][victim_way]
                << " c_sig: " << c_sig[set] << std::endl;
    }

    if (falcon::FALCON_SANITY_CHECK) {
      // Assertion
      if (victim_way == falcon::PT_WAY) {
        std::cout << "[PT] Cannot find a replacement victim!" << std::endl;
        assert(0);
      }
    }
  }
}

void falcon::PATTERN_TABLE::read_pattern(uint32_t curr_sig,
                                         std::vector<int> &delta_q,
                                         std::vector<uint32_t> &confidence_q,
                                         uint32_t &lookahead_way,
                                         uint32_t &lookahead_conf,
                                         uint32_t &pf_q_tail, uint32_t &depth) {
  // Update (sig, delta) correlation
  uint32_t set = get_hash(curr_sig) % falcon::PT_SET, local_conf = 0,
           pf_conf = 0, max_conf = 0;

  if (c_sig[set]) {
    for (uint32_t way = 0; way < falcon::PT_WAY; way++) {
      local_conf = (100 * c_delta[set][way]) / c_sig[set];
      pf_conf = depth ? (::GHR.global_accuracy * c_delta[set][way] /
                         c_sig[set] * lookahead_conf / 100)
                      : local_conf;

      if (pf_conf >= PF_THRESHOLD) {
        confidence_q[pf_q_tail] = pf_conf;
        delta_q[pf_q_tail] = delta[set][way];

        // Lookahead path follows the most confident entry
        if (pf_conf > max_conf) {
          lookahead_way = way;
          max_conf = pf_conf;
        }
        pf_q_tail++;

        if (falcon::FALCON_DEBUG_PRINT) {
          std::cout << "[PT] " << __func__ << " HIGH CONF: " << pf_conf
                    << " sig: " << std::hex << curr_sig << std::dec
                    << " set: " << set << " way: " << way;
          std::cout << " delta: " << delta[set][way]
                    << " c_delta: " << c_delta[set][way]
                    << " c_sig: " << c_sig[set];
          std::cout << " conf: " << local_conf << " depth: " << depth
                    << std::endl;
        }
      } else {
        if (falcon::FALCON_DEBUG_PRINT) {
          std::cout << "[PT] " << __func__ << "  LOW CONF: " << pf_conf
                    << " sig: " << std::hex << curr_sig << std::dec
                    << " set: " << set << " way: " << way;
          std::cout << " delta: " << delta[set][way]
                    << " c_delta: " << c_delta[set][way]
                    << " c_sig: " << c_sig[set];
          std::cout << " conf: " << local_conf << " depth: " << depth
                    << std::endl;
        }
      }
    }
    pf_q_tail++;

    lookahead_conf = max_conf;
    if (lookahead_conf >= PF_THRESHOLD)
      depth++;

    if (falcon::FALCON_DEBUG_PRINT) {
      std::cout << "global_accuracy: " << ::GHR.global_accuracy
                << " lookahead_conf: " << lookahead_conf << std::endl;
    }
  } else {
    confidence_q[pf_q_tail] = 0;
  }
}

bool falcon::PREFETCH_FILTER::check(uint64_t check_addr,
                                    FILTER_REQUEST filter_request) {
  uint64_t cache_line = check_addr >> LOG2_BLOCK_SIZE,
           hash = get_hash(cache_line),
           quotient = (hash >> REMAINDER_BIT) & ((1 << QUOTIENT_BIT) - 1),
           remainder = hash % (1 << REMAINDER_BIT);

  if (falcon::FALCON_DEBUG_PRINT) {
    std::cout << "[FILTER] check_addr: " << std::hex << check_addr
              << " check_cache_line: " << (check_addr >> LOG2_BLOCK_SIZE);
    std::cout << " hash: " << hash << std::dec << " quotient: " << quotient
              << " remainder: " << remainder << std::endl;
  }

  switch (filter_request) {
  case falcon::FALCON_L2C_PREFETCH:
    if ((valid[quotient] || useful[quotient]) &&
        remainder_tag[quotient] == remainder) {
      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__
                  << " line is already in the filter check_addr: " << std::hex
                  << check_addr << " cache_line: " << cache_line << std::dec;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient] << std::endl;
      }

      return false; // False return indicates "Do not prefetch"
    } else {
      valid[quotient] = 1;  // Mark as prefetched
      useful[quotient] = 0; // Reset useful bit
      remainder_tag[quotient] = remainder;

      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__
                  << " set valid for check_addr: " << std::hex << check_addr
                  << " cache_line: " << cache_line << std::dec;
        std::cout << " quotient: " << quotient
                  << " remainder_tag: " << remainder_tag[quotient]
                  << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient] << std::endl;
      }
    }
    break;

  case falcon::FALCON_l2c_PREFETCH:
    if ((valid[quotient] || useful[quotient]) &&
        remainder_tag[quotient] == remainder) {
      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__
                  << " line is already in the filter check_addr: " << std::hex
                  << check_addr << " cache_line: " << cache_line << std::dec;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient] << std::endl;
      }

      return false; // False return indicates "Do not prefetch"
    } else {
      // NOTE: falcon::FALCON_l2c_PREFETCH has relatively low confidence
      // (FILL_THRESHOLD <= falcon::FALCON_l2c_PREFETCH < PF_THRESHOLD)
      // Therefore, it is safe to prefetch this cache line in the large l2c and
      // save precious L2C capacity If this prefetch request becomes more
      // confident and falcon eventually issues falcon::FALCON_L2C_PREFETCH, we
      // can get this cache line immediately from the l2c (not from DRAM) To
      // allow this fast prefetch from l2c, falcon does not set the valid bit
      // for falcon::FALCON_l2c_PREFETCH

      // valid[quotient] = 1;
      // useful[quotient] = 0;

      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__
                  << " don't set valid for check_addr: " << std::hex
                  << check_addr << " cache_line: " << cache_line << std::dec;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient] << std::endl;
      }
    }
    break;

  case falcon::L2C_DEMAND:
    if ((remainder_tag[quotient] == remainder) && (useful[quotient] == 0)) {
      useful[quotient] = 1;
      if (valid[quotient])
        ::GHR.pf_useful++; // This cache line was prefetched by falcon and
                           // actually used in the program

      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[FILTER] " << __func__
                  << " set useful for check_addr: " << std::hex << check_addr
                  << " cache_line: " << cache_line << std::dec;
        std::cout << " quotient: " << quotient << " valid: " << valid[quotient]
                  << " useful: " << useful[quotient];
        std::cout << " GHR.pf_issued: " << ::GHR.pf_issued
                  << " GHR.pf_useful: " << ::GHR.pf_useful << std::endl;
      }
    }
    break;

  case falcon::L2C_EVICT:
    // Decrease global pf_useful counter when there is a useless prefetch
    // (prefetched but not used)
    if (valid[quotient] && !useful[quotient] && ::GHR.pf_useful)
      ::GHR.pf_useful--;

    // Reset filter entry
    valid[quotient] = 0;
    useful[quotient] = 0;
    remainder_tag[quotient] = 0;
    break;

  default:
    // Assertion
    std::cout << "[FILTER] Invalid filter request type: " << filter_request
              << std::endl;
    assert(0);
  }

  return true;
}

void falcon::GLOBAL_REGISTER::update_entry(uint32_t pf_sig,
                                           uint32_t pf_confidence,
                                           uint32_t pf_offset, int pf_delta) {
  // NOTE: GHR implementation is slightly different from the original paper
  // Instead of matching (last_offset + delta), GHR simply stores and matches
  // the pf_offset
  uint32_t min_conf = 100, victim_way = MAX_GHR_ENTRY;

  if (falcon::FALCON_DEBUG_PRINT) {
    std::cout << "[GHR] Crossing the page boundary pf_sig: " << std::hex
              << pf_sig << std::dec;
    std::cout << " confidence: " << pf_confidence << " pf_offset: " << pf_offset
              << " pf_delta: " << pf_delta << std::endl;
  }

  for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
    // if (sig[i] == pf_sig) { // TODO: Which one is better and consistent?
    //  If GHR already holds the same pf_sig, update the GHR entry with the
    //  latest info
    if (valid[i] && (offset[i] == pf_offset)) {
      // If GHR already holds the same pf_offset, update the GHR entry with the
      // latest info
      sig[i] = pf_sig;
      confidence[i] = pf_confidence;
      // offset[i] = pf_offset;
      delta[i] = pf_delta;

      if (falcon::FALCON_DEBUG_PRINT) {
        std::cout << "[GHR] Found a matching index: " << i << std::endl;
      }

      return;
    }

    // GHR replacement policy is based on the stored confidence value
    // An entry with the lowest confidence is selected as a victim
    if (confidence[i] < min_conf) {
      min_conf = confidence[i];
      victim_way = i;
    }
  }

  // Assertion
  if (victim_way >= MAX_GHR_ENTRY) {
    std::cout << "[GHR] Cannot find a replacement victim!" << std::endl;
    assert(0);
  }

  if (falcon::FALCON_DEBUG_PRINT) {
    std::cout << "[GHR] Replace index: " << victim_way
              << " pf_sig: " << std::hex << sig[victim_way] << std::dec;
    std::cout << " confidence: " << confidence[victim_way]
              << " pf_offset: " << offset[victim_way]
              << " pf_delta: " << delta[victim_way] << std::endl;
  }

  valid[victim_way] = 1;
  sig[victim_way] = pf_sig;
  confidence[victim_way] = pf_confidence;
  offset[victim_way] = pf_offset;
  delta[victim_way] = pf_delta;
}

uint32_t falcon::GLOBAL_REGISTER::check_entry(uint32_t page_offset) {
  uint32_t max_conf = 0, max_conf_way = MAX_GHR_ENTRY;

  for (uint32_t i = 0; i < MAX_GHR_ENTRY; i++) {
    if ((offset[i] == page_offset) && (max_conf < confidence[i])) {
      max_conf = confidence[i];
      max_conf_way = i;
    }
  }

  return max_conf_way;
}
