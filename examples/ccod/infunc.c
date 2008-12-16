#!/usr/bin/ccod
<?
/*
Inline Functions (infunc.c)

Inline/nested functions is a feature provided by
the GNU C compiler. Therefore, it is important to
note that GNU C++ and a few Non-GNU C compilers
*may not* support inline functions. For these
compilers the functions must be defined in the
"Header Section" of the source file.
*/

printf("Inline Function Example\n");

int whatisawren()
{
	printf("A Bird.\n");
}

printf("What is a Wren?\n");
whatisawren();

?>
