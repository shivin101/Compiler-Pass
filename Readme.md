### Part 3 : [Link](https://ucsd-pl.github.io/cse231/wi17/part3.html)

1. **Liveness Analysis**

   Your first task for this part is to implement a liveness analysis based on the framework implemented in the previous assignment. Recall that a variable is live at a particular point in the program if its value at that point will be used in the future. It is dead otherwise.
   
2. **May-point-to Analysis**

   In part 3, you will also need to implement a may-point-to analysis based on the framework you implemented


## Configuration
Install Docker, and follow [this](https://ucsd-pl.github.io/cse231/wi20/part0.html) tutorial

## Useful Tips
There are several commands that might be useful:

* Generate .ll file for a C++ program
```
clang -O0 -S -emit-llvm input.cpp
```

* Run passes 
```
opt -load [.so file] -[pass name] < [.ll file] > /dev/null
opt -load LLVMPart1.so -cse231-csi < input.ll > /dev/null
```

* Generate .bc file
```
clang++ -c -O0 -emit-llvm phi.cpp -o phi.bc
opt -load LLVMPart2.so -cse231-cdi < input.bc -o input-instrumented.bc
```

* Generate executives
```
clang++ /lib231/lib231.cpp input-instrumented.bc -o instrumented
```