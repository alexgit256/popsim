import matplotlib.pyplot as plt
import numpy as np
from popsim import Environment, Population

# --- Configure environment ---
env = Environment()
env.resources = 12500.
env.incest_threshold = 50  # forbid pairs with >90 equal bits across 128 bits
env.polygamy = False
env.marriage_probability = 0.9
env.conceiving_probability = 0.8
env.age_of_consent = 18

# Simple dying curve: low mortality until 60, then rise
curve = np.zeros(128, dtype=np.float32)
curve[0:5] = 0.01
curve[5:18] = 0.002
curve[18:60] = 0.005
curve[60:90] = np.linspace(0.02, 0.2, 30)
curve[90:120] = np.linspace(0.2, 0.7, 30)
curve[120:128] = 0.99
env.dying_curve = curve

# --- Initialize population ---
pop = Population(seed=12345)
pop.set_environment(env)
pop.initialize_random(8192, max_start_age=60)

YEARS = 150
pop.step(YEARS)

# --- Inspect people ---
persons = pop.persons()
print(f"Population now: {len(persons)}")
# print("Sample person:", vars(persons[0]))

# --- Plot metrics ---
mean_age = pop.mean_age_history()
pop_count = pop.population_history()

x = np.arange(1, len(mean_age) + 1)
plt.figure()
plt.plot(x, mean_age)
plt.title("Mean Age Over Time")
plt.xlabel("Year")
plt.ylabel("Mean age")
plt.grid(True)

plt.figure()
plt.plot(x, pop_count)
plt.title("Population Size Over Time")
plt.xlabel("Year")
plt.ylabel("Population")
plt.grid(True)

bh = pop.births_history()
dh = pop.deaths_history()
plt.plot(x, bh, label="Births")
plt.plot(x, dh, label="Deaths")
plt.legend()

plt.show()