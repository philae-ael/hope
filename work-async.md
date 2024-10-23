# Why ????
## IO ! (read from file, write to file, wait for GPU completion)
-> More like a callback system
-> Tasks are long lived but short but has short CPU execution time 
-> May be event driven (io_uring)
-> TODO: try io_uring vs read vs poll vs idk vs mmaped file
    -> Expected result:
        - mmaped file is the same / faster than an async read but there may be latency issues when accessing memory
        + May be inconsistent
        - It would be better to use an io_uring 
## Compute
-> I have 12 processors, (well 6*2 thx to hyperthreading) i should use all of them!
-> Some things are long lived and should not block a frame to be rendered (eg. loading of a file)

## Control Flow
A frame is a graph!
Each node a task.
That could be executed wither on CPU or on GPU

# How
## Throwing some ideas
Coroutine or Fiber ?!

### Fiber aka stackful coroutines:
- Maybe 2 stacks ?  One for env / cooperation (shared, ro or atomic writes/read are needed) and a primary stack
- Scheduler 
  - Dispatch taks to threads
  - Maybe this https://dl.acm.org/doi/10.1145/1837853.1693479
  - 
- Ability to start multiple tasks at once (with id ot maybe per task data (think shader vertex shader inputs))
- Allocate task + Stack with given size (fixed)
- API could looks like draw call dispatch


## Literature survey

Work stealing
In a task based
can be done stackless

Fork Join style => Tasks are stored locally, not globally!
Thus we have 2 part of the code

See Wool paper [^wool]. The introduction section is very good.
Looks like [rayon](https://docs.rs/rayon/latest/rayon/)'s design.


[^wool]: Faxen, K.-F. (2010) ‘Efficient Work Stealing for Fine Grained Parallelism’, in 2010 39th International Conference on Parallel Processing. 2010 39th International Conference on Parallel Processing (ICPP), San Diego, CA, USA: IEEE, pp. 313–322. Available at: https://doi.org/10.1109/ICPP.2010.39.

[^lace]: Van Dijk, T. and Van De Pol, J.C. (2014) ‘Lace: Non-blocking Split Deque for Work-Stealing’, in L. Lopes et al. (eds) Euro-Par 2014: Parallel Processing Workshops. Cham: Springer International Publishing (Lecture Notes in Computer Science), pp. 206–217. Available at: https://doi.org/10.1007/978-3-319-14313-2_18.

