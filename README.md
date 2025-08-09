# Population Simulator

A C++-powered population dynamics simulator with Cython bindings for Python.  
It models an environment with resources, marriage/conception rules, incest avoidance, mortality, and polygamy options — and evolves a population year-by-year.

## ✨ Features

- **Environment parameters**:
  - Resource limit
  - Incest threshold (bitwise genome similarity)
  - Age-specific mortality curve (128 years)
  - Marriage probability
  - Conception probability
  - Age of consent
  - Optional polygamy mode

- **Genome-based incest check**:
  - Each person has a 128-bit genome (two 64-bit integers).
  - If two genomes match in more than `incest_threshold` bits, marriage/conception is blocked.

- **Marriage & reproduction**:
  - Monogamy: marriage between unmarried adults.
  - Polygamy: no marriage; females pick random males yearly.

- **Yearly tick**:
  - Marriages & conceptions (subject to probabilities and incest check)
  - Age increment & mortality check
  - Births, deaths, and population metrics recorded

- **History tracking**:
  - Population size
  - Mean age
  - (Optional patch) births per year & deaths per year

- **Python API**:
  - Create/configure environment & population
  - Step simulation by N years
  - Access population stats & individuals

## 📦 Installation

### Prerequisites
- Python 3.9+
- C++17 compiler (GCC, Clang, or MSVC)
- [pip](https://pip.pypa.io/en/stable/) and [virtualenv](https://virtualenv.pypa.io/en/stable/) (recommended)
- NumPy, Cython, setuptools, wheel

### First-time build

Clone the repository and install in editable mode:

```bash
git clone https://github.com/yourname/popsim.git
cd popsim

# Create & activate venv (optional but recommended)
python -m venv .venv
source .venv/bin/activate

# Install build dependencies
pip install -U pip wheel setuptools Cython numpy

# Build & install
python -m pip install -e .
