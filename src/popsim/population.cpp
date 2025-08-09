#include "population.hpp"
#include <cmath>
#include <unordered_set>

#if defined(__GNUC__) || defined(__clang__)
static inline int popcount64(uint64_t x) { return __builtin_popcountll(x); }
#else
// Fallback popcount
static inline int popcount64(uint64_t x) {
    x = x - ((x >> 1) & 0x5555555555555555ull);
    x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull);
    return (int)( (((x + (x >> 4)) & 0x0F0F0F0F0F0F0F0Full) * 0x0101010101010101ull) >> 56 );
}
#endif

namespace popsim {

Population::Population(uint64_t seed) : rng_(seed), next_id_(1ull) {}

void Population::reseed(uint64_t seed) { rng_.seed(seed); }

void Population::set_environment(const Environment &env) { env_ = env; }

void Population::initialize_random(std::size_t N, uint32_t max_start_age) {
    people_.clear();
    people_.reserve(N);
    std::uniform_int_distribution<uint32_t> age_dist(0u, max_start_age);
    std::uniform_int_distribution<uint32_t> gender_dist(0u, 1u);
    std::uniform_int_distribution<uint64_t> gene_dist(0ull, std::numeric_limits<uint64_t>::max());
    for (std::size_t i = 0; i < N; ++i) {
        Person p{};
        p.id = next_id_++;
        p.g0 = gene_dist(rng_);
        p.g1 = gene_dist(rng_);
        p.age = age_dist(rng_);
        p.gender = gender_dist(rng_);
        p.marital = 0ull; // unmarried
        people_.push_back(p);
    }
    mean_age_hist_.clear();
    pop_hist_.clear();
}

// Count equal bits across both 64-bit genome words (total 128 bits)
bool Population::incest_blocked(const Person &a, const Person &b) const {
    uint64_t x0 = ~(a.g0 ^ b.g0); // 1 where bits equal
    uint64_t x1 = ~(a.g1 ^ b.g1);
    int eq = popcount64(x0) + popcount64(x1);
    return (uint32_t)eq > env_.incest_threshold;
}

void Population::do_year() {
    // marriages or polygamy mating choice happens before aging/deaths to keep order consistent
    if (!env_.polygamy) {
        marriages();
        conceiving();
    } else {
        polygamous_conceiving();
    }

    // Age and deaths
    births_this_year = 0;
    deaths_this_year = 0;
    std::vector<Person> survivors;
    survivors.reserve(people_.size());
    age_and_maybe_die(survivors);
    people_.swap(survivors);

    // Metrics
    births_hist_.push_back(births_this_year);
    deaths_hist_.push_back(deaths_this_year);
    double sum_age = 0.0;
    for (const auto &p : people_) sum_age += (double)p.age;
    double mean_age = people_.empty() ? 0.0 : sum_age / (double)people_.size();
    mean_age_hist_.push_back(mean_age);
    pop_hist_.push_back(people_.size());
}

void Population::step(uint32_t years) {
    for (uint32_t i = 0; i < years; ++i) do_year();
}

void Population::age_and_maybe_die(std::vector<Person> &out) {
    std::uniform_real_distribution<double> U(0.0, 1.0);
    for (auto &p : people_) {
        // increment age at start of year
        if (p.age < std::numeric_limits<uint32_t>::max()) p.age += 1u;
        uint32_t idx = p.age < 128u ? p.age : 127u;
        double prob = std::clamp((double)env_.dying_curve[idx], 0.0, 1.0);
        if (U(rng_) < prob) {
            // death: if married, clear partner's marital if still present
            deaths_this_year++;
            if (p.married()) {
                uint64_t partner = p.partner_id();
                for (auto &q : people_) {
                    if (q.id == partner && q.married() && q.partner_id() == p.id) {
                        q.marital = 0ull;
                        break;
                    }
                }
            }
            continue; // skip adding to survivors
        }
        out.push_back(p);
    }
}

void Population::marriages() {
    // Eligible unmarried adults by gender
    std::vector<uint32_t> fem, male;
    fem.reserve(people_.size());
    male.reserve(people_.size());
    for (uint32_t i = 0; i < (uint32_t)people_.size(); ++i) {
        const auto &p = people_[i];
        if (p.married()) continue;
        if (p.age < env_.age_of_consent) continue;
        if (p.gender == 0u) fem.push_back(i); else male.push_back(i);
    }
    std::shuffle(fem.begin(), fem.end(), rng_);
    std::shuffle(male.begin(), male.end(), rng_);

    double pressure = 1.0 - (people_.empty() ? 0.0 : ((double)people_.size() /env_.resources ));
    pressure = std::clamp(pressure, 0.0, 1.0);
    double p_marry = std::clamp(env_.marriage_probability * pressure, 0.0, 1.0);
    std::uniform_real_distribution<double> U(0.0, 1.0);

    size_t pairs = std::min(fem.size(), male.size());
    for (size_t k = 0; k < pairs; ++k) {
        uint32_t i = fem[k];
        uint32_t j = male[k];
        if (people_[i].married() || people_[j].married()) continue; // race condition avoidance
        if (incest_blocked(people_[i], people_[j])) continue;
        if (U(rng_) < p_marry) {
            uint64_t id_i = people_[i].id;
            uint64_t id_j = people_[j].id;
            people_[i].marital = make_marital_field(id_j);
            people_[j].marital = make_marital_field(id_i);
        }
    }
}

void Population::conceiving() {
    std::uniform_real_distribution<double> U(0.0, 1.0);
    double pressure = 1.0 - (people_.empty() ? 0.0 : ((double)people_.size() / env_.resources));
    pressure = std::clamp(pressure, 0.0, 1.0);
    double p_child = std::clamp(env_.conceiving_probability * pressure, 0.0, 1.0);

    // Iterate over married females of age >= consent and with male partner >= consent
    for (uint32_t i = 0; i < (uint32_t)people_.size(); ++i) {
        auto &mother = people_[i];
        if (mother.gender != 0u) continue; // female only
        if (!mother.married()) continue;
        if (mother.age < env_.age_of_consent) continue;
        // NEW fertility window for mother
        if (!fertile_female(mother.age)) continue;
        // find partner index (linear scan; for big populations, consider a map)
        uint64_t pid = mother.partner_id();
        int32_t father_idx = -1;
        for (uint32_t j = 0; j < (uint32_t)people_.size(); ++j) {
            if (people_[j].id == pid) { father_idx = (int32_t)j; break; }
        }
        if (father_idx < 0) continue;
        auto &father = people_[(uint32_t)father_idx];
        if (father.gender != 1u) continue;
        if (father.age < env_.age_of_consent) continue;
        if (incest_blocked(mother, father)) continue;
        // NEW fertility window for father
        if (!fertile_male(father.age)) continue;
        if (U(rng_) < p_child) add_child(i, (uint32_t)father_idx);
    }
}

void Population::polygamous_conceiving() {
    std::uniform_real_distribution<double> U(0.0, 1.0);
    double pressure = 1.0 - (people_.empty() ? 0.0 : (env_.resources / (double)people_.size()));
    pressure = std::clamp(pressure, 0.0, 1.0);
    double p_child = std::clamp(env_.conceiving_probability * pressure, 0.0, 1.0);

    // collect eligible males
    std::vector<uint32_t> males;
    for (uint32_t i = 0; i < (uint32_t)people_.size(); ++i) {
        if (people_[i].gender == 1u &&
            people_[i].age >= env_.age_of_consent &&
            fertile_male(people_[i].age)) {
            males.push_back(i);
        }
    }
    if (males.empty()) return;
    std::uniform_int_distribution<size_t> male_pick(0u, males.size() - 1u);

    for (uint32_t i = 0; i < (uint32_t)people_.size(); ++i) {
        auto &mother = people_[i];
        if (mother.gender != 0u) continue;
        if (mother.age < env_.age_of_consent) continue;
        // NEW fertility window for mother
        if (!fertile_female(mother.age)) continue;
        uint32_t father_idx = males[male_pick(rng_)];
        auto &father = people_[father_idx];
        if (incest_blocked(mother, father)) continue;
        if (U(rng_) < p_child) add_child(i, father_idx);
    }
}

void Population::add_child(uint32_t mother_idx, uint32_t father_idx) {
    // Simple uniform per-bit recombination: choose parent bit with 50% chance
    std::uniform_int_distribution<uint64_t> mask_dist(0ull, std::numeric_limits<uint64_t>::max());
    uint64_t m0 = mask_dist(rng_);
    uint64_t m1 = mask_dist(rng_);

    const auto &mom = people_[mother_idx];
    const auto &dad = people_[father_idx];

    Person child{};
    child.id = next_id_++;
    child.g0 = (mom.g0 & m0) | (dad.g0 & ~m0);
    child.g1 = (mom.g1 & m1) | (dad.g1 & ~m1);
    child.age = 0u;
    child.gender = (mask_dist(rng_) & 1ull) ? 1u : 0u; // 50/50
    child.marital = 0ull;
    // NEW: mutate newborn genome by flipping exactly env_.mutation_bits positions
    mutate_child(child, env_.mutation_bits);
    births_this_year++;

    people_.push_back(child);
}

void Population::mutate_child(Person &child, uint32_t k) {
    if (k == 0) return;
    std::unordered_set<int> pos;
    pos.reserve(k);
    std::uniform_int_distribution<int> pick(0, 127);
    while ((uint32_t)pos.size() < k) pos.insert(pick(rng_));
    for (int b : pos) {
        if (b < 64) child.g0 ^= (1ull << b);
        else        child.g1 ^= (1ull << (b - 64));
    }
}

} // namespace popsim