#pragma once
#include <cstdint>
#include <vector>
#include <random>
#include <algorithm>
#include <utility>
#include <limits>

namespace popsim {

struct Environment {
    double resources; // arbitrary units
    uint32_t incest_threshold; // maximum allowed equal bits across 128-bit genome; if > threshold => block
    float dying_curve[128]; // per-age (0..127) death probability per year
    bool polygamy; // if true: no marriages, females choose random male for conception
    double marriage_probability; // base probability per candidate pair
    double conceiving_probability; // base probability per couple per year
    uint32_t age_of_consent; // years

    Environment()
        : resources(0.0), incest_threshold(64), polygamy(false),
          marriage_probability(0.0), conceiving_probability(0.0), age_of_consent(18) {
        for (int i = 0; i < 128; ++i) dying_curve[i] = 0.0f;
    }
};

struct Person {
    // 1) Unique id as an uint (<2^63)
    uint64_t id; // we ensure id < 2^63 by construction
    // 2) 2x64-bit genome
    uint64_t g0;
    uint64_t g1;
    // 3) age counter
    uint32_t age;
    // 4) marital: LSB = 1 if married, MSB63 = partner id; 0 if unmarried
    uint64_t marital;
    // 5) gender: 0=female, 1=male (current model)
    uint32_t gender;

    bool married() const { return (marital & 1ull) != 0ull; }
    uint64_t partner_id() const { return marital >> 1; }
};

class Population {
public:
    explicit Population(uint64_t seed = 0xC0FFEEULL);

    // Replace the whole environment
    void set_environment(const Environment &env);
    const Environment & get_environment() const { return env_; }

    // Create N random persons (clears existing)
    void initialize_random(std::size_t N, uint32_t max_start_age = 60);

    // Advance the simulation by `years` ticks
    void step(uint32_t years = 1);

    // Access persons
    const std::vector<Person>& persons() const { return people_; }

    // Metrics history (one entry per year advanced)
    const std::vector<double>& mean_age_history() const { return mean_age_hist_; }
    const std::vector<std::size_t>& population_history() const { return pop_hist_; }
    const std::vector<size_t>& births_history() const { return births_hist_; }
    const std::vector<size_t>& deaths_history() const { return deaths_hist_; }

    // RNG seeding
    void reseed(uint64_t seed);

private:
    Environment env_;
    std::vector<Person> people_;
    std::mt19937_64 rng_;
    uint64_t next_id_; // always < 2^63

    size_t births_this_year;
    size_t deaths_this_year;

    // history
    std::vector<double> mean_age_hist_;
    std::vector<std::size_t> pop_hist_;
    std::vector<size_t> births_hist_;
    std::vector<size_t> deaths_hist_;

    // helpers
    bool incest_blocked(const Person &a, const Person &b) const;
    uint64_t make_marital_field(uint64_t partner_id) const { return (partner_id << 1) | 1ull; }
    void do_year();
    void age_and_maybe_die(std::vector<Person> &out);
    void marriages();
    void conceiving();
    void polygamous_conceiving();
    void add_child(uint32_t mother_idx, uint32_t father_idx);
};

} // namespace popsim