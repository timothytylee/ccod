#!/usr/bin/ccod
<#
/*
C++ and Classes (cppex.cpp)

Simple C++ example using a namespace, class, new/delete, and
cout. Notice the class and includes are contained within a
header section.
*/
#include <iostream>
#include <stdio.h>

using namespace std;

class MyLilClass
{
public:

	char name[50];
	int age;

};
#>
<?
MyLilClass *iam = new MyLilClass();

sprintf(iam->name, "John Denton");
iam->age = 46;

cout << "Hi my name is " << iam->name << "." << endl;
cout << "I'm " << iam->age << " years old." << endl;

delete iam;
return 0;

?>
