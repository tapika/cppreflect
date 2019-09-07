C++ Reflection (Portable)
=========================

.. image:: https://travis-ci.org/tapika/cppreflect.svg
	:target: https://travis-ci.org/tapika/cppreflect

Overview
--------

Initial presentation on how things work:

- C++ Runtime Type Reflection
https://docs.google.com/presentation/d/1xo0sg10C4DN4iHSD9wqmqZ_99FeVrxI-RcZqxMs_M7Y/edit?usp=sharing

This repository contains C++ reflection library only (no C++ scripting related stuff).

At the moment this library is portable (Verified on windows & linux), but does not yet have 
property accessors support due to missing `__declspec(property)` in gcc.

