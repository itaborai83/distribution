import bisect
import random
import unittest
import math
from dataclasses import dataclass

NAN = float("nan")
INF = float("inf")
LOG10 = math.log(10)

DEFAULT_BIN_COUNT = 100
DEFAULT_OUTLIER_COUNT = 10
DEFAULT_DECAY = 0.95

class Distribution:
    
    def __init__(self, bin_count=DEFAULT_BIN_COUNT, outlier_count=DEFAULT_OUTLIER_COUNT, decay=DEFAULT_DECAY):
        self.bin_count = bin_count
        self.outlier_count = outlier_count
        self.decay = decay
        self.left_outliers = []
        self.bins = []
        self.right_outliers = []
        self.count = 0

    def _update_left_outliers(self, value):
        # keep adding values to the left outliers until we have enough
        if len(self.left_outliers) < self.outlier_count:
            bisect.insort_left(self.left_outliers, value)
            return False, NAN

        # if we have enough left outliers, then we need to replace the largest one and insert the new value in the correct place
        # find the index where the new value should be inserted
        insert_idx = self.outlier_count
        curr_idx = self.outlier_count - 1
        while True:
            curr_value = self.left_outliers[curr_idx]
            if value > curr_value:
                break
            insert_idx -= 1
            curr_idx -= 1
            if curr_idx < 0:
                break
        
        # if the new value is the same as the largest left outlier, then we don't need to do anything
        if insert_idx == self.outlier_count:
            return True, value
        
        # replace the largest left outlier with the new value
        new_value = self.left_outliers[self.outlier_count - 1]
        idx = self.outlier_count - 1
        while idx > insert_idx:
            self.left_outliers[idx] = self.left_outliers[idx - 1]
            idx -= 1
        self.left_outliers[insert_idx] = value
        return True, new_value

    def _update_right_outliers(self, value):
        # keep adding values to the right outliers until we have enough
        if len(self.right_outliers) < self.outlier_count:
            bisect.insort_right(self.right_outliers, value)
            return False, NAN

        # if we have enough right outliers, then we need to replace the smallest one and insert the new value in the correct place
        # find the index where the new value should be inserted
        insert_idx = -1
        curr_idx = 0
        while True:
            curr_value = self.right_outliers[curr_idx]
            if value < curr_value:
                break
            insert_idx += 1
            curr_idx += 1
            if curr_idx >= self.outlier_count:
                break
        
        # if the new value is the same as the smallest right outlier, then we don't need to do anything
        if insert_idx == -1:
            return True, value
        
        # replace the smallest right outlier with the new value
        new_value = self.right_outliers[0]
        idx = 0
        while idx < insert_idx:
            self.right_outliers[idx] = self.right_outliers[idx+1]
            idx += 1
        self.right_outliers[insert_idx] = value
        return True, new_value
        
    def update(self, value):
        keep_going, value = self._update_left_outliers(value)
        if not keep_going:
            return
        
        keep_going, value = self._update_right_outliers(value)
        if not keep_going:
            return
                
        if self._is_full():
            exact_match_found = self._compact(value)
            if exact_match_found:
                return

        item = (value, 1, 0)
        bisect.insort_right(self.bins, item)
        self.count += 1
    
    def _is_full(self):
        return len(self.bins) == self.bin_count
            
    def _find_closest_bins(self, value):
        min_delta = INF
        min_idx = None
        for i, curr_item in enumerate(self.bins[:-1]):
            next_item = self.bins[i + 1]
            assert curr_item <= next_item, (i, curr_item, next_item)
            curr_value, curr_count, curr_merges = curr_item
            _, next_count, next_merges = next_item
            if curr_value == value:
                return True, i, i
            merges = (curr_merges + next_merges + 1) / 2
            count = (curr_count + next_count) / 2 
            delta = merges / count
            if delta < min_delta:
                min_delta = delta
                min_idx = i
        return False, min_idx, min_idx + 1

    def _compact(self, value):
        exact_match, curr_idx, next_idx = self._find_closest_bins(value)
        if exact_match:
            # if we found an exact match, then just increment the count
            curr_value, curr_count, curr_merges = self.bins[curr_idx]
            self.bins[curr_idx] = (curr_value, curr_count+1, curr_merges)
            return True
        curr, curr_count, curr_merges = self.bins[curr_idx]
        next, next_count, next_merges = self.bins[next_idx]
        new_count = next_count + curr_count
        new_merges = curr_merges + next_merges + 1
        new_value = (curr * curr_count + next * next_count) / new_count  
        new_item = (new_value, new_count, new_merges)
        del self.bins[next_idx]
        self.bins[curr_idx] = new_item
        return False
    
class TestDistribution(unittest.TestCase):

    NUM_SAMPLES = 10000
    TEST_PCTS = [ i / 100.0 for i in range(0, 100, 5) ] 

    def setUp(self):
        self.dist = Distribution(DEFAULT_BIN_COUNT)
        random.seed(142)

    def tearDown(self):
        pass
    
    @unittest.skip("skip sum normal lognormal")
    def test_sum_normal_lognormal(self):
        mu, sigma = 1., 1.
        lognormal = list([random.lognormvariate(mu, sigma) for i in range(self.NUM_SAMPLES)])
        mu, sigma = 10., 0.5
        normal = list([random.normalvariate(mu, sigma) for i in range(self.NUM_SAMPLES)])
        for value in lognormal:
            self.dist.update(value)
        for value in normal:
            self.dist.update(value)
        combined = sorted(lognormal + normal)
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_combined_idx    = int(pct * self.NUM_SAMPLES * 2)
            pct_combined        = combined[pct_combined_idx]
            pct_diff            = (pct_histogram - pct_combined)
            print(f"{pct_combined:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

        #for bin, freq in self.dist.histogram(bin_count=30):
        #    print(f"{bin:8.4f}\t{freq:8.4f}")
        print()
        print(self.dist.left_outliers)
        print(self.dist.right_outliers)


    #@unittest.skip("skip lognormal")
    def test_lognormal(self):
        mu, sigma = 3., 1.
        lognormal = [random.lognormvariate(mu, sigma) for i in range(self.NUM_SAMPLES)]
        for value in lognormal:
            self.dist.update(value)
        lognormal.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_lognormal_idx   = int(pct * self.NUM_SAMPLES)
            pct_lognormal       = lognormal[pct_lognormal_idx]
            pct_diff            = (pct_histogram - pct_lognormal)
            print(f"{pct_lognormal:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

        #for bin, freq in self.dist.histogram(bin_count=20):
        #    print(f"{bin:8.4f}\t{freq:8.4f}")
            
    @unittest.skip("skip normal")
    def test_normal(self):
        mu, sigma = 10., 1.
        normal = [random.normalvariate(mu, sigma) for i in range(self.NUM_SAMPLES)]
        for value in normal:
            self.dist.update(value)
        normal.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_normal_idx      = int(pct * self.NUM_SAMPLES)
            pct_normal          = normal[pct_normal_idx]
            pct_diff            = (pct_histogram - pct_normal)
            print(f"{pct_normal:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")
        
        for bin, freq in self.dist.histogram(bin_count=20):
            print(f"{bin:8.4f}\t{freq:8.4f}")

    @unittest.skip("skip uniform")
    def test_uniform(self):
        uniform = [random.uniform(0, 1) for i in range(self.NUM_SAMPLES)]
        for value in uniform:
            self.dist.update(value)
        uniform.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_uniform_idx     = int(pct * self.NUM_SAMPLES)
            pct_uniform         = uniform[pct_uniform_idx]
            pct_diff            = (pct_histogram - pct_uniform)
            print(f"{pct_uniform:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip exponential")
    def test_exponential(self):
        lambd = 1.0
        exponential = [random.expovariate(lambd) for i in range(self.NUM_SAMPLES)]
        for value in exponential:
            self.dist.update(value)
        exponential.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_exponential_idx = int(pct * self.NUM_SAMPLES)
            pct_exponential     = exponential[pct_exponential_idx]
            pct_diff            = (pct_histogram - pct_exponential)
            print(f"{pct_exponential:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip gamma")
    def test_gamma(self):
        alpha, beta = 3., 1.
        gamma = [random.gammavariate(alpha, beta) for i in range(self.NUM_SAMPLES)]
        for value in gamma:
            self.dist.update(value)
        gamma.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_gamma_idx       = int(pct * self.NUM_SAMPLES)
            pct_gamma           = gamma[pct_gamma_idx]
            pct_diff            = (pct_histogram - pct_gamma)
            print(f"{pct_gamma:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip beta")
    def test_beta(self):
        alpha, beta = 3., 1.
        beta = [random.betavariate(alpha, beta) for i in range(self.NUM_SAMPLES)]
        for value in beta:
            self.dist.update(value)
        beta.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_beta_idx        = int(pct * self.NUM_SAMPLES)
            pct_beta            = beta[pct_beta_idx]
            pct_diff            = (pct_histogram - pct_beta)
            print(f"{pct_beta:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip triangular")
    def test_triangular(self):
        low, high, mode = 0., 1., 0.5
        triangular = [random.triangular(low, high, mode) for i in range(self.NUM_SAMPLES)]
        for value in triangular:
            self.dist.update(value)
        triangular.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_triangular_idx  = int(pct * self.NUM_SAMPLES)
            pct_triangular      = triangular[pct_triangular_idx]
            pct_diff            = (pct_histogram - pct_triangular)
            print(f"{pct_triangular:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")
    
    @unittest.skip("skip weibull")
    def test_weibull(self):
        alpha, beta = 3., 1.
        weibull = [random.weibullvariate(alpha, beta) for i in range(self.NUM_SAMPLES)]
        for value in weibull:
            self.dist.update(value)
        weibull.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_weibull_idx     = int(pct * self.NUM_SAMPLES)
            pct_weibull         = weibull[pct_weibull_idx]
            pct_diff            = (pct_histogram - pct_weibull)
            print(f"{pct_weibull:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip pareto")
    def test_pareto(self):
        alpha = 3.
        pareto = [random.paretovariate(alpha) for i in range(self.NUM_SAMPLES)]
        for value in pareto:
            self.dist.update(value)
        pareto.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_pareto_idx      = int(pct * self.NUM_SAMPLES)
            pct_pareto          = pareto[pct_pareto_idx]
            pct_diff            = (pct_histogram - pct_pareto)
            print(f"{pct_pareto:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

    @unittest.skip("skip vonmisesvariate")
    def test_vonmisesvariate(self):
        mu, kappa = 0., 4.
        vonmisesvariate = [random.vonmisesvariate(mu, kappa) for i in range(self.NUM_SAMPLES)]
        for value in vonmisesvariate:
            self.dist.update(value)
        vonmisesvariate.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx       = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram           = self.dist.bins[pct_histogram_idx][0]
            pct_vonmisesvariate_idx = int(pct * self.NUM_SAMPLES)
            pct_vonmisesvariate     = vonmisesvariate[pct_vonmisesvariate_idx]
            pct_diff                = (pct_histogram - pct_vonmisesvariate)
            print(f"{pct_vonmisesvariate:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")
    
    @unittest.skip("skip gauss")
    def test_gauss(self):
        gauss = [random.gauss(0, 1) for i in range(self.NUM_SAMPLES)]
        for value in gauss:
            self.dist.update(value)
        gauss.sort()
        print()
        for pct in self.TEST_PCTS:
            pct_histogram_idx   = int(pct * DEFAULT_BIN_COUNT)
            pct_histogram       = self.dist.bins[pct_histogram_idx][0]
            pct_gauss_idx       = int(pct * self.NUM_SAMPLES)
            pct_gauss           = gauss[pct_gauss_idx]
            pct_diff            = (pct_histogram - pct_gauss)
            print(f"{pct_gauss:8.4f}\t{pct_histogram:8.4f}\t{pct_diff:8.4f}")

if __name__ == "__main__":
    unittest.main()