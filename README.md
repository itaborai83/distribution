# **Approximating Empirical Distributions with Constant Space**

Percentiles are useful statistical measures that help us understand and interpret the distribution of a set of data. They divide a dataset into percentage-based segments, providing insights into the relative standing of a particular data point within the entire dataset. Percentiles provide a valuable tool for summarizing and interpreting data, making them an essential part of statistical analysis and decision-making in various fields.

When dealing with potentially unbounded data, it can be challenging to maintain accurate statistics and compute empirical distributions due to memory constraints. In scenarios where it is not feasible to store the entire dataset, approximate methods that use constant space become crucial. One of the techniques that can be employed is the use of histograms to maintain an approximation of the underlying distribution of the data.

This repository shows how to store unbounded data using constant space and how to compute percentiles from the underlying histogram. The use of percentiles of empirical data is an important tool to characterize the data distribution and how it behaves over time.

The algorithm is implemented in C due to its performance sensitive nature. The code does not use memory allocations to keep it as cache friendly as possible. Implementing it in C might also benefit anyone who might need to package it and call this from other programming languages.

In this technique, instead of storing each data point, a series of bins containing a bin value and its associated count are kept ordered (by the bin value) and are updated with each new value seen. When there are no more free bins left, the histogram goes through a compaction process that diminishes the accuracy of the bin values but reduces the required number of bins to store the values seen so far.

The bin value is represented by the formula `alpha * (base ^ exponent)`. The base and the exponent are applied to all the bins in the histogram and `alpha` is the bin specific value used to derive the final bin value.

Assuming that we are dealing with a histogram using base `2` and the current exponent is `-2`, then the value `34.1765` would be represented by the `alpha` value `136`. The `alpha` value was obtained using the following formula `floor(value / pow(base, exponent))`. Given an `alpha`, we can compute the bin value with the formula `alpha * pow(base, exponent)`.

In the above case, the value `34.1765` was stored in a bin whose associated value is only `34`.

When the histogram needs to go under a compaction procedure, to allow the insertion of new values that are not covered by the existing bins, a new `alpha` value is computed using the formula `new_alpha = alpha / exponent` for all existing bins.

If two or more bins result in the same `alpha` value, their counts are added together in the resulting compacted histogram. Since it is not necessarily true that a compaction run will merge bins together (due to the values being too spread), the compaction procedure is repeated until the number of used bins (after compaction) is reduced.

 I hope you find this helpful.

---

 ## **Example Command Line Application**

A simple command line program was created to illustrate how the histogram can be used in a practical scenario.

The program is named `histog` and it reads `stdin` parsing numerical values and updating an histogram. To compile it, simple issue a `make` command on the shell

    $ ./histog -h
    Usage: ./histog [OPTIONS] FILE
    Updates a histogram from values read from stdin and displays the resulting histogram or the percentiles.
    If FILE is given, it will try to read it and save the updated histogram in it afterwards.
    Options:
        -b BASE     Set the base of the histogram. Defaults to 2
        -e EXPONENT Set the exponent of the histogram. Defaults to -3
        -p          Show percentiles. Use default precision of 0.01
        -P          Set the precision of the percentiles. Implies -p.
        -q          Quiet mode
        -h          Print this message and exit

By default, the program will read the numerical values found on `stdin` and output the underlying histogram.

    $ echo "1 this 2 2 will 3 3 3 be 4 4 4 4 ignored 5 5 5 5 5 ." | ./histog
    Histogram: Count = 15, Bin Count = 5, Base = 2, Exponent = -3
        Bins:
        (1.00, 1) (2.00, 2) (3.00, 3) (4.00, 4) (5.00, 5)


The program can also show the percentiles of the underlying distribution using either the `-p` or the `-P` flags.

`-p` shows the percentiles between `0.0` inclusive and `1.0` non inclusive spreading them apart with precision `0.01`.

`-P` does the same thing, but allows you to specify the precision that you want to use like below:

    $ shuf data/normal.txt | ./histog -P 0.1
    PCT     VALUE
    0.000000        -5.000000
    0.100000        -1.285111
    0.200000        -0.844079
    0.300000        -0.526180
    0.400000        -0.254856
    0.500000        -0.001596
    0.600000        0.251572
    0.700000        0.524300
    0.800000        0.842287
    0.900000        1.280881
    1.000000        5.500000


If a filename is specified, it will persist the updated histogram between invocations of the `histog` program.

    $ echo "1 this 2 2 will 3 3 3 be 4 4 4 4 ignored 5 5 5 5 5 ." | ./histog test.hst
    Histogram: Count = 15, Bin Count = 5, Base = 2, Exponent = -3
        Bins:
        (1.00, 1) (2.00, 2) (3.00, 3) (4.00, 4) (5.00, 5)

    $ echo "1 this 2 2 will 3 3 3 be 4 4 4 4 ignored 5 5 5 5 5 ." | ./histog test.hst
    Histogram: Count = 30, Bin Count = 5, Base = 2, Exponent = -3
        Bins:
        (1.00, 2) (2.00, 4) (3.00, 6) (4.00, 8) (5.00, 10)

    $ echo "15 16 17" | ./histog test.hst
    Histogram: Count = 33, Bin Count = 8, Base = 2, Exponent = -3
        Bins:
        (1.00, 2) (2.00, 4) (3.00, 6) (4.00, 8) (5.00, 10)
        (15.00, 1) (16.00, 1) (17.00, 1)