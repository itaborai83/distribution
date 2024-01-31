# **Approximating Empirical Distributions with Constant Space**

When dealing with potentially unbounded data, it can be challenging to maintain accurate statistics and compute empirical distributions due to memory constraints. In scenarios where it is not feasible to store the entire dataset, approximate methods that use constant space become crucial. One of the techniques that can be employed is the use of histograms to maintain an approximation of the underlying distribution of the data.

This repository shows how to store unbounded data using constant space and how to compute percentiles from the underlying histogram. The use of percentiles of empirical data is a important tool to caracterize the data distribution and how it behaves over time.

The algorithm is implemented in C due to its performance sensitive nature. The code does not use memory allocations to keep it as cache friendly as possible. Implementing it in C might also benefit anyone that might need to package it and call this from other programming languages. 

In this technique, instead of storing each data point, a series of bins containing a bin value and its associated count are kept ordered (by the bin value) and are updated with each new value seen. When there are no more free bins left, the histogram goes through a compaction process that diminishes the accuracy of the bin values but reduces the required number of bins to store the values seen so far.

The bin value is represented by the formula `alpha * base ^ exponent`. The base and the exponent is applied to all the bins in the histogram and `alpha` is the bin specific value used to derive the bin value.

Assuming that we are dealing with an histogram using base `2` and the current exponent is `-2`, then the value `34.1765` would be represented by the `alpha` value `136`. The `alpha` value was obtained using the following formula `floor(value / pow(base, exponent))`. Given an `alpha`, we can compute the bin value with the formula `alpha * pow(base, exponent)`.

In the above case, the value `34.1765` was stored in a bin whose associated value is only `34`.

When the histogram needs to go under a compaction procedure, to allow the insertion of new values that are not covered by the existing bins, a new `alpha` value is computed using the formula `new_alpha = alpha / exponent` for all existing bins.

If two or more bins result in the same `alpha` value, their counts are added together in the resulting compacted histogram. Since it is not necessarily true that a compaction run will merge bins together, the compaction procedure is repeated until the number of used bins (after compaction) is reduced.

 I hope you find this helpful.

---

 ## **Example Command Line Application**

TBD

---
 ## **Changing the Bin Count**

TBD

---
## **The Negative Effect of Outlier Values** 

TBD