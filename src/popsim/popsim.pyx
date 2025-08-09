# distutils: language = c++
# cython: language_level=3

from libcpp.vector cimport vector
from libc.stddef cimport size_t
cimport numpy as cnp
import numpy as np
# NOTE: C++ classes Environment/Person/Population are auto-visible from popsimp.pxd
# Do NOT cimport them again, and do NOT reuse the same names for Python classes.

cdef class PyEnvironment:
    cdef Environment _env  # C++ Environment
    def __cinit__(self):
        self._env = Environment()

    @property
    def resources(self): 
        return self._env.resources
    @resources.setter
    def resources(self, v): self._env.resources = float(v)

    @property
    def incest_threshold(self): 
        return self._env.incest_threshold
    @incest_threshold.setter
    def incest_threshold(self, v): self._env.incest_threshold = <unsigned int> v

    @property
    def dying_curve(self):
        return np.array([self._env.dying_curve[i] for i in range(128)], dtype=np.float32)
    @dying_curve.setter
    def dying_curve(self, arr):
        a = np.asarray(arr, dtype=np.float32)
        if a.size != 128:
            raise ValueError("dying_curve must have length 128")
        for i in range(128):
            self._env.dying_curve[i] = float(a[i])

    @property
    def polygamy(self): 
        return bool(self._env.polygamy)
    @polygamy.setter
    def polygamy(self, v): self._env.polygamy = bool(v)

    @property
    def marriage_probability(self): 
        return self._env.marriage_probability
    @marriage_probability.setter
    def marriage_probability(self, v): self._env.marriage_probability = float(v)

    @property
    def conceiving_probability(self): 
        return self._env.conceiving_probability
    @conceiving_probability.setter
    def conceiving_probability(self, v): self._env.conceiving_probability = float(v)

    @property
    def age_of_consent(self): 
        return self._env.age_of_consent
    @age_of_consent.setter
    def age_of_consent(self, v): self._env.age_of_consent = <unsigned int> v

    # --- NEW: mutation & fertility windows ---
    @property
    def mutation_bits(self):
        return self._env.mutation_bits
    @mutation_bits.setter
    def mutation_bits(self, v):
        self._env.mutation_bits = <unsigned int> v

    @property
    def female_fertility_min(self):
        return self._env.female_fertility_min
    @female_fertility_min.setter
    def female_fertility_min(self, v):
        self._env.female_fertility_min = <unsigned int> v

    @property
    def female_fertility_max(self):
        return self._env.female_fertility_max
    @female_fertility_max.setter
    def female_fertility_max(self, v):
        self._env.female_fertility_max = <unsigned int> v
    @property
    def male_fertility_min(self):
        return self._env.male_fertility_min
    @male_fertility_min.setter
    def male_fertility_min(self, v):
        self._env.male_fertility_min = <unsigned int> v

    @property
    def male_fertility_max(self):
        return self._env.male_fertility_max
    @male_fertility_max.setter
    def male_fertility_max(self, v):
        self._env.male_fertility_max = <unsigned int> v

    cdef Environment* ptr(self):
        return &self._env

cdef class PersonView:
    cdef unsigned long long _id
    cdef unsigned long long _g0
    cdef unsigned long long _g1
    cdef unsigned int _age
    cdef unsigned long long _marital
    cdef unsigned int _gender

    @property
    def id(self): 
        return self._id
    @property
    def g0(self): 
        return self._g0
    @property
    def g1(self): 
        return self._g1
    @property
    def age(self): 
        return self._age
    @property
    def married(self): 
        return bool(self._marital & 1)
    @property
    def partner_id(self): 
        return self._marital >> 1
    @property
    def gender(self): 
        return self._gender  # 0=female, 1=male
    @property
    def as_dict(self):
        return {
            "id": int(self._id),
            "g0": int(self._g0),
            "g1": int(self._g1),
            "age": int(self._age),
            "married": bool(self._marital & 1),
            "partner_id": int(self._marital >> 1),
            "gender": int(self._gender),
        }

    def __repr__(self):
        return f"PersonView(id={self._id}, age={self._age}, married={bool(self._marital & 1)}, gender={self._gender})"

cdef inline PersonView _wrap_person(const Person& p):  # C++ Person
    cdef PersonView v = PersonView.__new__(PersonView)
    v._id = p.id
    v._g0 = p.g0
    v._g1 = p.g1
    v._age = p.age
    v._marital = p.marital
    v._gender = p.gender
    return v


cdef class PyPopulation:
    cdef Population* _pop  # C++ Population
    def __cinit__(self, seed: int = 0xC0FFEE):
        self._pop = new Population(<unsigned long long>seed)
    def __dealloc__(self):
        del self._pop

    def set_environment(self, PyEnvironment env):
        self._pop.set_environment(env._env)

    def get_environment(self) -> PyEnvironment:
        cdef PyEnvironment e = PyEnvironment()
        e._env = self._pop.get_environment()
        return e

    def initialize_random(self, int N, int max_start_age=60):
        self._pop.initialize_random(N, max_start_age)

    def step(self, int years=1):
        if years < 0:
            raise ValueError("years must be non-negative")
        self._pop.step(years)

    def reseed(self, seed: int):
        self._pop.reseed(<unsigned long long>seed)

    def persons(self):
        cdef vector[Person] v = self._pop.persons()
        out = []
        out_extend = out.append
        cdef Py_ssize_t i, n = v.size()
        for i in range(n):
            out_extend(_wrap_person(v[i]))
        return out
        
    def mean_age_history(self):
        cdef vector[double] h = self._pop.mean_age_history()
        cdef Py_ssize_t i, n = h.size()
        return [h[i] for i in range(n)]

    def population_history(self):
        cdef vector[size_t] h = self._pop.population_history()
        cdef Py_ssize_t i, n = h.size()
        return [h[i] for i in range(n)]

    def births_history(self):
        cdef vector[size_t] h = self._pop.births_history()
        return [h[i] for i in range(h.size())]

    def deaths_history(self):
        cdef vector[size_t] h = self._pop.deaths_history()
        return [h[i] for i in range(h.size())]