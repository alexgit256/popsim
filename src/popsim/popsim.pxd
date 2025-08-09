# distutils: language = c++
# cython: language_level=3

from libcpp.vector cimport vector
from libc.stddef cimport size_t

cdef extern from "population.hpp" namespace "popsim":
    cdef cppclass Environment:
        double resources
        unsigned int incest_threshold
        float dying_curve[128]
        bint polygamy
        double marriage_probability
        double conceiving_probability
        unsigned int age_of_consent
        Environment()

    cdef cppclass Person:
        unsigned long long id
        unsigned long long g0
        unsigned long long g1
        unsigned int age
        unsigned long long marital
        unsigned int gender
        bint married() const
        unsigned long long partner_id() const

    cdef cppclass Population:
        Population(unsigned long long seed)
        void set_environment(const Environment&)
        const Environment& get_environment() const
        void initialize_random(size_t N, unsigned int max_start_age)
        void step(unsigned int years)
        const vector[Person]& persons() const
        const vector[double]& mean_age_history() const
        const vector[size_t]& population_history() const
        const vector[size_t]& births_history() const
        const vector[size_t]& deaths_history() const
        void reseed(unsigned long long seed)
