# CS 310 Project 1 Thread

User level thread library in c++

### Spec
https://users.cs.duke.edu/~chase/cps310/p1.html

### How to run

To compile and run the thread.cc in a sample test, open unix vm, cd into this directory and type to compile and run

```
g++ -o app thread.cc app.cc libinterrupt.a -ldl && ./thread
```

### Thread.cc

```
int thread_libinit(thread_startfunc_t func, void *arg); 
int thread_create(thread_startfunc_t func, void *arg); 
static void STUB(thread_startfunc_t func, void* arg);
int thread_yield(void); // call switch
int thread_lock(unsigned int lock); //call switch
int thread_unlock(unsigned int lock);
int thread_wait(unsigned int lock, unsigned int cond); //call switch
int thread_signal(unsigned int lock, unsigned int cond);
int thread_broadcast(unsigned int lock, unsigned int cond);

static void cleanup();
static void check_ready_queue();
static void switchtodeletethread();
static void switchtorunningthread();
```

### Test cases

The test cases are created for testing bugs in thread libraries.
Specific purposes of the test cases are written in the comments inside the test#.cc files.

```
------ grading student test case test11.cc ------
student test case test11.cc exposed the following instructor buggy thread libraries: B H I J L
------ grading student test case test13.cc ------
student test case test13.cc exposed the following instructor buggy thread libraries: L
------ grading student test case test8.cc ------
student test case test8.cc exposed the following instructor buggy thread libraries: M
------ grading student test case test2.cc ------
student test case test2.cc exposed the following instructor buggy thread libraries: B H I J
------ grading student test case test3.cc ------
student test case test3.cc exposed the following instructor buggy thread libraries: A B D E F I J K
------ grading student test case test5.cc ------
student test case test5.cc exposed the following instructor buggy thread libraries: M
------ grading student test case test6.cc ------
student test case test6.cc exposed the following instructor buggy thread libraries: B I L
------ grading student test case test4.cc ------
student test case test4.cc exposed the following instructor buggy thread libraries: A B E F L
------ grading student test case test12.cc ------
student test case test12.cc exposed the following instructor buggy thread libraries: H
------ grading student test case test9.cc ------
student test case test9.cc exposed the following instructor buggy thread libraries: A B E F I L
------ grading student test case test7.cc ------
student test case test7.cc exposed the following instructor buggy thread libraries: B H I J

overall student test suite exposed 11 of the 13 instructor buggy thread libraries
grade for thread library test suite: 14 out of 14
```


## Acknowledgments

* Using std::map in c++ : http://www.cplusplus.com/reference/map/map/
* Using std::queue in c++: http://www.cplusplus.com/reference/queue/queue/
* Using std::pair and make_pair in c++: https://stackoverflow.com/questions/9270563/what-is-the-purpose-of-stdmake-pair-vs-the-constructor-of-stdpair
* Understanding swapcontext, getcontext, makecontext: http://pubs.opengroup.org/onlinepubs/009695299/functions/makecontext.html etc.
* Deallocate stack in ucontext: https://stackoverflow.com/questions/7829074/how-to-deallocate-stack-in-ucontext-linux
* Catching bad memory allocation using try catch: http://www.cplusplus.com/forum/beginner/49169/
