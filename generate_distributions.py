import random
import os.path
import argparse

def generate_normal_distribution(filename, n=1000000):
    print('Generating normal distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        mu, sigma = 0.0, 1.0
        for i in range(n):
            sample = random.normalvariate(mu, sigma)
            print(sample, file=f)

def generate_lognormal_distribution(filename, n=1000000):
    print('Generating lognormal distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        mu, sigma = 0, 1
        for i in range(n):
            sample = random.lognormvariate(mu, sigma)
            print(sample, file=f)

def generate_exponential_distribution(filename, n=1000000):
    print('Generating exponential distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        mu = 1.0
        for i in range(n):
            sample = random.expovariate(mu)
            print(sample, file=f)

def generate_gamma_distribution(filename, n=1000000):
    print('Generating gamma distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        alpha, beta = 1.0, 1.0
        for i in range(n):
            sample = random.gammavariate(alpha, beta)
            print(sample, file=f)

def generate_beta_distribution(filename, n=1000000):
    print('Generating beta distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        alpha, beta = 1.0, 1.0
        for i in range(n):
            sample = random.betavariate(alpha, beta)
            print(sample, file=f)

def generate_weibull_distribution(filename, n=1000000):
    print('Generating weibull distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        alpha, beta = 1.0, 1.0
        for i in range(n):
            sample = random.weibullvariate(alpha, beta)
            print(sample, file=f)

def generate_pareto_distribution(filename, n=1000000):
    print('Generating pareto distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        alpha = 1.0
        for i in range(n):
            sample = random.paretovariate(alpha)
            print(sample, file=f)

def generate_triangular_distribution(filename, n=1000000):
    print('Generating triangular distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        low, high, mode = 0.0, 1.0, 0.5
        for i in range(n):
            sample = random.triangular(low, high, mode)
            print(sample, file=f)

def generate_uniform_distribution(filename, n=1000000):
    print('Generating uniform distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        low, high = 0.0, 1.0
        for i in range(n):
            sample = random.uniform(low, high)
            print(sample, file=f)

def generate_cauchy_distribution(filename, n=1000000):
    print('Generating cauchy distribution with {} samples'.format(n))
    with open(filename, 'w') as f:
        x0, gamma = 0.0, 1.0
        for i in range(n):
            sample = random.vonmisesvariate(x0, gamma)
            print(sample, file=f)


def main(directory):
    if not os.path.exists(directory):
        raise Exception('Directory {} does not exist'.format(directory))
    generate_normal_distribution(directory + '/normal.txt')
    generate_lognormal_distribution(directory + '/lognormal.txt')
    generate_exponential_distribution(directory + '/exponential.txt')
    generate_gamma_distribution(directory + '/gamma.txt')
    generate_beta_distribution(directory + '/beta.txt')
    generate_weibull_distribution(directory + '/weibull.txt')
    generate_pareto_distribution(directory + '/pareto.txt')
    generate_triangular_distribution(directory + '/triangular.txt')
    generate_uniform_distribution(directory + '/uniform.txt')
    generate_cauchy_distribution(directory + '/cauchy.txt')

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', help='directory to store generated distributions')
    args = parser.parse_args()
    main(args.directory)
