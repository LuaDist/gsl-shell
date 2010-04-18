.. highlight:: lua

.. _introduction:

GSL Shell introduction
======================

In order to use GSL Shell effectively you need to know the basics of
the Lua scripting language. Luckily Lua is very easy to learn, its syntax is very simple and easy to understand and most people will be able to understand it with little of no efforts. In this chapter we will learn you about the fundamentals of Lua in order to be able to use GSL Shell. If you want to get a better understanding of Lua you can look have a look at at `Lua.org <http://www.lua.org>`_.

First steps
-----------

The most basic usage of GSL Shell is just like a calculator: you can type any expression and GSL Shell will print the results. GSL Shell works with few fundamentals types:

- real and complex numbers
- real and complex matrices
- tables

Talking about the numbers you should know that in GSL Shell every number is represented as a double precision real or complex number. There is no special type to represent integer numbers and if you give an integer number GSL Shell will treat it just as a real number. So, if you type something like '1/2' the results will be (not entirely surprising) 0.5. Otherwise complex and reals number can be freely mixed in any expression.



Once you start GSL Shell a prompt will be shown and GSL Shell is ready to accept your intructions. More preciselt the instructions accepted are "statements" like, for example, variable assignments, function call or compound statement like :keyword:`for` loop of :keyword:`if` conditionals. Here some examples::

   -- assign to the variable x the value sin(pi/4)
   x = sin(pi/4)

   -- print the values of the 'sin' function for different values
   for k=0, 10 do
      print(sin(2*pi*k/10))
   end

If you want to simply display the value of a variable or of an expression just write the expression after an '=' and its value be shown in the terminal. So for example to show the value of ``sin(pi/4)`` you can write::

   -- to print the value of an expression just put '=' at the beginning
   = sin(pi/4)

