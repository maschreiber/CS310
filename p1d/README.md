# CS 310 Project 1 Deli

Concurrent Deli

### Spec
https://users.cs.duke.edu/~chase/cps310/p1.html

### How to run

To compile and run the deli.cc in a sample test, open unix vm, cd into this directory and type to compile and run

```
g++ -o deli thread.o deli.cc libinterrupt.a -ldl
```

```
./deli 3 sw.in0 sw.in1 sw.in2 sw.in3 sw.in4
```

### Thread.cc

```
int main(int argc, char *argv[])
void* startf(void* arg)
void* maker()
void* cashier(void* a)

```

## Acknowledgments

* Using std::map in c++ : http://www.cplusplus.com/reference/map/map/
* Using std::queue in c++: http://www.cplusplus.com/reference/queue/queue/
* Using std::pair and make_pair in c++: https://stackoverflow.com/questions/9270563/what-is-the-purpose-of-stdmake-pair-vs-the-constructor-of-stdpair
* Using std::vector in c++: http://www.cplusplus.com/reference/vector/vector/
* Reading input files: http://www.cplusplus.com/doc/tutorial/files/
* Converting string to int: https://stackoverflow.com/questions/22084783/function-stoi-not-declared

