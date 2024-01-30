# **Tango**

![GitHub](https://img.shields.io/github/license/AlstrupJacob/Tango.svg) ![GitHub code size in bytes](https://img.shields.io/github/languages/code-size/AlstrupJacob/Tango.svg)

**Tango** is a dynamically typed, transpiled, programming language. This repo contains a compiler for transpiling *Tango* code into an intermediate representation, and a virtual machine to execute the bytecode, implemented in *C*. 

This compiler was written as a self study in the subject of interpreter implementation. It is loosly based on the CLox implementation from Robert Nystrom's *Crafting Interpreters*. 

## **Types**

Under the hood, *Tango* interprets all numbers as 64-bit floats, including integers. *Tango* has a single data structure, strings. Strings are internally represented using contiguous memory blocks chars.

all variables are defined using the keyword *variable*.
### Numbers

Designating numbers requires no special syntax.
```
variable integer = 2; 

variable float = 3.14;
```

Scientific notation is interpretable by the compiler.
```
variable notation = 3.14e-3;
```

#### Supported Operations

the syntax for arithmetic operations in *tango* is straightforward and familiar.
```
variable a = 3.14; variable b = 6.67e-11;

variable addition = a + b;
variable subtraction = a - b;
variable multiplication = a * b;
variable division = a / b;
variable exponentiation = a ^ b;
```

## Data Structures

### Strings
Strings are designated using double quotes.
```
variable greeting = "Hello World.";
```
#### Supported Operations

Strings can be concatenated using the addition operator.
```
variable greeting = "Hello";
variable recipient = "World";

variable greet = greeting + " " + recipient + ".";
```
## **Functions**

Functions are defined using the keyword *function*. Parameters are designated in parentheses directly following the function name. The body of the function is designated using braces.
```
function addTwo(n) {

    return n + 2;
}
```

### Native Functions

*Tango* has one built in function, the print function.
```
variable a = 2;

print a;
```
## **Classes**

Classes are defined using the keyword *class*. 
```
class Adder {
    
    addTwo(n) {

        return n + 2;
    }
}
```

Begrudgingly, as its inclusion relegates *Tango* to being an "Object Oriented" programming language, *Tango* supports inheritance. Inheritance is designated using the less than operator, with the decendent to the left, and ancestor to the right of the operator.

```
class Adderplus < Adder {}
```
