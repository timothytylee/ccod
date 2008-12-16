#!/usr/bin/ccod

#pragma CCOD:script no

/*
Copyright Notice and Credits

The University of Strathclyde, Glasgow, Scotland.

Permission to copy is granted provided that these credits remain intact.

These notes were written by Steve Holmes of the University of Strathclyde 
Computer Centre. They originally formed the basis of the Computer Centre's 
C programming course. Steve no longer works for the University of Strathclyde,
and we are unable to answer queries relating to this course. You are welcome 
to make links to it however, but please bear in mind that it was written for 
students within the University and so some parts may not be relevant to 
external readers.

This page has nothing to do with C programming. The rest of this document 
does, read on and good luck.

http://www.strath.ac.uk/IT/Docs/Ccourse/

*/

#include <stdio.h>

void print_converted(int pounds)
/* Convert U.S. Weight to Imperial and International
   Units. Print the results */
{       int stones = pounds / 14;
        int uklbs = pounds % 14;
        float kilos_per_pound = 0.45359;
        float kilos = pounds * kilos_per_pound;

        printf("   %3d          %2d  %2d        %6.2f\n",
                pounds, stones, uklbs, kilos);
}

main(int argc,char *argv[])
{       int pounds;

        if(argc != 2)
        {        printf("Usage: convert weight_in_pounds\n");
                exit(1);
        }

        sscanf(argv[1], "%d", &pounds); /* Convert String to int */

        printf(" US lbs      UK st. lbs       INT Kg\n");
        print_converted(pounds);
}