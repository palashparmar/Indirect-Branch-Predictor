# Indirect Branch Predictor

Implemented Indirect Branch Predictor by registering history of the geometric length of the incoming instruction, resulting
in only 0.093 mispredictions per 1000 instructions (MPKI) with SPEC CPU 2006 benchmarks.

## Indirect Branch Predictor
Assembly level code jumps from one place to another if it enconunter a condition such as if, for etc. Such jumps are conditional as it is of yes or no type. Other branches are not conditional as same instruction can jump to different places depending upon the instruction. 

CPU, because of its pipeline stages, takes some cycle to decode the branch before executing the instruction. This result in lose of performace. One way to avoid this is to predict the outcome of a branch instruction before it execuate so that the processor does not hault itself to decode it.

This Project proposes and implement on method which predict the outcome of branch and compare it with actual outcome to calculate the number of mispredictions. The framework is borrowed from CBP2 competition and my implementation is presented in src/my_predictor.h.
