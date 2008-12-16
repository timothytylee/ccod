#!/usr/bin/ccod

<#
/*
Header Section (hsect.cpp) 

Header sections are applied to the global space of
the source file. They are used to define variables
and classes that cannot defined in the local space.
Very useful for  c++ source files  and source file
with complex library/include configurations.
*/ 

#pragma CCOD:library expat
#include <expat.h>

class MyClass
{
	int i;
}; 


#> 
<?
	MyClass mcls;
	printf("Header Section Example\n");
?>
